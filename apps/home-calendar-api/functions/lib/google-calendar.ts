import { google, calendar_v3 } from "googleapis";
import { createHash } from "crypto";

export interface CalendarEvent {
  id: string;
  title: string;
  start: number; // Unix timestamp
  end: number; // Unix timestamp
  allDay: boolean;
}

export interface MonthEvent {
  day: number;
  title: string;
}

export interface MonthInfo {
  year: number;
  month: number;
  firstDayOfWeek: number; // 0 = Sunday
  daysInMonth: number;
  daysInPrevMonth: number;
}

export interface CalendarResponse {
  meta: {
    etag: string;
    generatedAt: number;
  };
  appointments: CalendarEvent[];
  monthEvents: MonthEvent[];
  month: MonthInfo;
}

// Get Google Calendar client using service account credentials
export function getCalendarClient(credentialsJson: string): calendar_v3.Calendar {
  const credentials = JSON.parse(credentialsJson);

  const auth = new google.auth.GoogleAuth({
    credentials,
    scopes: ["https://www.googleapis.com/auth/calendar.readonly"],
  });

  return google.calendar({ version: "v3", auth });
}

// Abbreviate event title for month grid (max 8 chars)
function abbreviateTitle(title: string): string {
  // Common abbreviations
  const abbreviations: Record<string, string> = {
    appointment: "Appt",
    orthodontics: "Ortho",
    orthodontist: "Ortho",
    chiropractor: "Chiro",
    chiropractic: "Chiro",
    dentist: "Dentist",
    doctor: "Dr",
    meeting: "Mtg",
    birthday: "Bday",
    volleyball: "Vball",
    basketball: "Bball",
  };

  const lower = title.toLowerCase();
  for (const [full, abbrev] of Object.entries(abbreviations)) {
    if (lower.includes(full)) {
      return abbrev;
    }
  }

  // Just truncate if no abbreviation found
  if (title.length > 8) {
    return title.substring(0, 8);
  }
  return title;
}

// Parse Google Calendar event to our format
function parseEvent(event: calendar_v3.Schema$Event): CalendarEvent | null {
  if (!event.id || !event.summary) return null;

  const isAllDay = !event.start?.dateTime;

  let startTime: number;
  let endTime: number;

  if (isAllDay) {
    // All-day events use date strings (YYYY-MM-DD)
    startTime = new Date(event.start?.date + "T00:00:00Z").getTime() / 1000;
    endTime = new Date(event.end?.date + "T00:00:00Z").getTime() / 1000;
  } else {
    // Timed events use dateTime strings
    startTime = new Date(event.start?.dateTime!).getTime() / 1000;
    endTime = new Date(event.end?.dateTime!).getTime() / 1000;
  }

  return {
    id: event.id,
    title: event.summary,
    start: startTime,
    end: endTime,
    allDay: isAllDay,
  };
}

// Generate month info for current month
function getMonthInfo(): MonthInfo {
  const now = new Date();
  const year = now.getFullYear();
  const month = now.getMonth() + 1; // 1-indexed

  // First day of current month
  const firstDay = new Date(year, month - 1, 1);
  const firstDayOfWeek = firstDay.getDay(); // 0 = Sunday

  // Days in current month
  const daysInMonth = new Date(year, month, 0).getDate();

  // Days in previous month
  const daysInPrevMonth = new Date(year, month - 1, 0).getDate();

  return {
    year,
    month,
    firstDayOfWeek,
    daysInMonth,
    daysInPrevMonth,
  };
}

// Fetch calendar events and format response
export async function fetchCalendarData(
  calendar: calendar_v3.Calendar,
  calendarId: string = "primary"
): Promise<CalendarResponse> {
  const now = new Date();

  // Anchor the appointments window to the START OF DAY, not the current moment.
  //
  // Previously timeMin was `now`, which slid forward on every request. Combined
  // with maxResults, that meant the set of events in the window (and therefore
  // the ETag below) could shift on every hourly device wake — even when nothing
  // on the calendar actually changed — forcing the e-paper to redraw needlessly
  // (the "refreshes every hour" symptom). Anchoring to start-of-day makes the
  // window stable for the whole day, so the ETag only changes when a real
  // appointment is added/moved/removed (or at the next day boundary).
  const startOfDay = new Date(now);
  startOfDay.setHours(0, 0, 0, 0);
  const sixtyDaysLater = new Date(startOfDay.getTime() + 60 * 24 * 60 * 60 * 1000);

  // Get start of current month for month events
  const monthStart = new Date(now.getFullYear(), now.getMonth(), 1);
  const monthEnd = new Date(now.getFullYear(), now.getMonth() + 1, 0, 23, 59, 59);

  // Fetch upcoming events (next 60 days) for appointments
  // Display will show as many as fit in the available space
  const appointmentsResponse = await calendar.events.list({
    calendarId,
    timeMin: startOfDay.toISOString(),
    timeMax: sixtyDaysLater.toISOString(),
    singleEvents: true,
    orderBy: "startTime",
    maxResults: 20,
  });

  // Fetch all events in current month for month grid
  const monthResponse = await calendar.events.list({
    calendarId,
    timeMin: monthStart.toISOString(),
    timeMax: monthEnd.toISOString(),
    singleEvents: true,
    orderBy: "startTime",
    maxResults: 50,
  });

  // Parse appointments
  const appointments: CalendarEvent[] = [];
  for (const event of appointmentsResponse.data.items || []) {
    const parsed = parseEvent(event);
    if (parsed) {
      appointments.push(parsed);
    }
  }

  // Parse month events (abbreviated for grid)
  const monthEvents: MonthEvent[] = [];
  const seenDays = new Set<string>(); // Track day+title combos to avoid duplicates

  for (const event of monthResponse.data.items || []) {
    if (!event.summary) continue;

    let day: number;
    if (event.start?.date) {
      // All-day event
      day = parseInt(event.start.date.split("-")[2]);
    } else if (event.start?.dateTime) {
      // Timed event - extract day in local timezone
      const eventDate = new Date(event.start.dateTime);
      day = eventDate.getDate();
    } else {
      continue;
    }

    const abbrevTitle = abbreviateTitle(event.summary);
    const key = `${day}-${abbrevTitle}`;

    if (!seenDays.has(key)) {
      seenDays.add(key);
      monthEvents.push({ day, title: abbrevTitle });
    }
  }

  // Sort month events by day
  monthEvents.sort((a, b) => a.day - b.day);

  // Generate etag from response data using SHA256 hash
  // Include start times and titles so rescheduled/renamed events change the etag
  const etag = createHash("sha256")
    .update(
      JSON.stringify({
        appointments: appointments.map((a) => ({
          id: a.id,
          start: a.start,
          title: a.title,
        })),
        monthEvents: monthEvents.map((e) => `${e.day}-${e.title}`),
      })
    )
    .digest("hex")
    .substring(0, 16);

  return {
    meta: {
      etag,
      generatedAt: Math.floor(Date.now() / 1000),
    },
    appointments,
    monthEvents,
    month: getMonthInfo(),
  };
}
