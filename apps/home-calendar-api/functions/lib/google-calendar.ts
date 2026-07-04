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

// The display's local timezone. The device renders "today" and the month grid
// in this zone, so the backend must compute the grid and the day boundary here
// too - not in the Lambda's UTC clock. Overridable per deployment.
const DISPLAY_TZ = process.env.DISPLAY_TIMEZONE || "America/Toronto";

// Year/month/day as seen in the given timezone (NOT the Lambda's UTC clock).
// Used both for the month grid and for the day-boundary stamp in the ETag, so a
// new local day forces a fresh 200 exactly at local midnight.
function getLocalDateParts(
  date: Date,
  timeZone: string
): { year: number; month: number; day: number } {
  const parts = new Intl.DateTimeFormat("en-CA", {
    timeZone,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
  }).formatToParts(date);
  const get = (type: string) =>
    parseInt(parts.find((p) => p.type === type)!.value, 10);
  return { year: get("year"), month: get("month"), day: get("day") };
}

// Generate month info for the current month IN THE DISPLAY TIMEZONE.
function getMonthInfo(now: Date): MonthInfo {
  const { year, month } = getLocalDateParts(now, DISPLAY_TZ);

  // Day-of-week of the 1st and the month lengths are fixed calendar facts for a
  // given year+month. Build them from UTC so the Lambda's offset can't shift the
  // 1st back into the previous day (the bug the old local-time construction had).
  const firstDayOfWeek = new Date(Date.UTC(year, month - 1, 1)).getUTCDay();
  const daysInMonth = new Date(Date.UTC(year, month, 0)).getUTCDate();
  const daysInPrevMonth = new Date(Date.UTC(year, month - 1, 0)).getUTCDate();

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
      // Timed event - extract the day in the DISPLAY timezone so an evening
      // event doesn't land on the wrong grid cell (getDate() would use UTC).
      day = getLocalDateParts(new Date(event.start.dateTime), DISPLAY_TZ).day;
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

  const month = getMonthInfo(now);

  // Generate etag from response data using SHA256 hash.
  //
  // It MUST cover more than event content. The month grid layout and the
  // highlighted "today" roll over at the local day/month boundary even when no
  // event changed. If the ETag hashed only appointments/monthEvents, an
  // unchanged-events day boundary would return 304 and freeze the device on the
  // previous day's/month's grid (the confirmed "stuck showing June on July 3"
  // failure). So we also hash:
  //   - `localDate`: the current day in the display timezone -> forces exactly
  //     one fresh 200 per local day (moves the "today" box, ages off past
  //     appointments), while staying stable *within* a day (no hourly redraws).
  //   - `month`: the full grid layout -> any month rollover changes the ETag.
  // Start times and titles are still included so rescheduled/renamed events
  // also change the ETag.
  const { year, month: mm, day: dd } = getLocalDateParts(now, DISPLAY_TZ);
  const localDate = `${year}-${mm}-${dd}`;
  const etag = createHash("sha256")
    .update(
      JSON.stringify({
        localDate,
        month,
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
    month,
  };
}
