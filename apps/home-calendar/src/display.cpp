#include "display.h"
#include "config.h"

#ifndef NATIVE_BUILD

#include <GxEPD2_3C.h>  // 3-color display (black/white/red)
#include <SPI.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <time.h>

#if DEBUG_MODE
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// Display dimensions - Portrait mode (rotated)
static constexpr int SCREEN_WIDTH = 480;
static constexpr int SCREEN_HEIGHT = 800;

// Layout constants
namespace Layout {
    constexpr int MARGIN = 8;

    // Page title at the very top
    constexpr int PAGE_TITLE_HEIGHT = 28;

    // Appointments section (starts after page title)
    constexpr int APPOINTMENTS_TOP = PAGE_TITLE_HEIGHT;
    constexpr int APPOINTMENTS_HEIGHT = 340;
    constexpr int DATE_HEADER_HEIGHT = 26;
    constexpr int EVENT_HEIGHT = 26;
    constexpr int SEPARATOR_HEIGHT = 1;
    constexpr int MAX_APPOINTMENTS_DISPLAY = 8;

    // Divider between sections
    constexpr int DIVIDER_Y = APPOINTMENTS_TOP + APPOINTMENTS_HEIGHT;

    // Month grid (starts after divider)
    constexpr int MONTH_TOP = DIVIDER_Y + 5;
    constexpr int DAY_HEADER_HEIGHT = 22;
    constexpr int GRID_TOP = MONTH_TOP + DAY_HEADER_HEIGHT;
    constexpr int CELL_WIDTH = 68;   // 480 / 7 = 68.5
    constexpr int CELL_HEIGHT = 60;
    constexpr int CELL_PADDING = 4;
    constexpr int CELL_DAY_OFFSET_Y = 13;
    constexpr int CELL_EVENT_OFFSET_Y = 26;  // First event line
    constexpr int CELL_EVENT_LINE_HEIGHT = 10;  // Spacing between event lines

    // Footer
    constexpr int FOOTER_Y = 785;
}

// Display instance - GooDisplay GDEY075Z08 - 7.5" 3-color
static GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 2> display(
    GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// Month names
static const char* MONTH_NAMES[] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

// Day abbreviations
static const char* DAY_ABBREV[] = {"S", "M", "T", "W", "T", "F", "S"};

// Get X position to center text on screen
static int get_center_x(const char* text, const GFXfont* font) {
    int16_t x1, y1;
    uint16_t w, h;
    display.setFont(font);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    return (x < 0) ? 0 : x;
}

// Format Unix timestamp to "9:30am" or "All day"
// Uses localtime_r() which respects the timezone and DST settings
// configured by configTime() in power_manager.cpp
static void format_time(uint32_t timestamp, bool allDay, char* buffer, size_t bufSize) {
    if (allDay) {
        strncpy(buffer, "All day", bufSize);
        return;
    }

    time_t t = timestamp;
    struct tm timeinfo;
    localtime_r(&t, &timeinfo);

    int hour = timeinfo.tm_hour;
    const char* ampm = (hour >= 12) ? "pm" : "am";
    if (hour > 12) hour -= 12;
    if (hour == 0) hour = 12;

    if (timeinfo.tm_min == 0) {
        snprintf(buffer, bufSize, "%d%s", hour, ampm);
    } else {
        snprintf(buffer, bufSize, "%d:%02d%s", hour, timeinfo.tm_min, ampm);
    }
}

// Format Unix timestamp to "Jan 5, 2026" for date header
static void format_date_header(uint32_t timestamp, bool allDay, char* buffer, size_t bufSize) {
    time_t t = timestamp;
    struct tm timeinfo;
    // All-day events stored as midnight UTC - use gmtime to preserve the date
    // Timed events use localtime to apply timezone + DST
    if (allDay) {
        gmtime_r(&t, &timeinfo);
    } else {
        localtime_r(&t, &timeinfo);
    }
    strftime(buffer, bufSize, "%b %d, %Y", &timeinfo);
}

// Get day of month from Unix timestamp
static int get_day_of_month(uint32_t timestamp, bool allDay) {
    time_t t = timestamp;
    struct tm timeinfo;
    if (allDay) {
        gmtime_r(&t, &timeinfo);
    } else {
        localtime_r(&t, &timeinfo);
    }
    return timeinfo.tm_mday;
}

// Draw a very light dithered red background
static void draw_dithered_red_rect(int x, int y, int w, int h) {
    // Very sparse pattern - ~10% density for very light appearance
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if ((px + py * 3) % 10 == 0) {  // ~10% density
                display.drawPixel(px, py, GxEPD_RED);
            }
        }
    }
}

// Draw a date header text (inside a day block) with light pink background
static void draw_date_header_text(int y, const char* dateStr) {
    // Light pink dithered background on date row
    draw_dithered_red_rect(1, y + 1, SCREEN_WIDTH - 2, Layout::DATE_HEADER_HEIGHT - 1);

    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(Layout::MARGIN + 5, y + 18);
    display.print(dateStr);
}

// Calculate how many lines a title will need when wrapped
static int calculate_wrapped_lines(const char* title, int maxWidth) {
    if (!title || title[0] == '\0') return 1;

    display.setFont(&FreeSans9pt7b);
    int16_t x1, y1;
    uint16_t w, h;

    // Get full text width
    display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    if ((int)w <= maxWidth) return 1;

    // Fixed space width (must match draw_event_line)
    const int spaceWidth = 5;

    // Need to wrap - count lines by simulating word wrap
    int lines = 1;
    int currentWidth = 0;
    const char* p = title;
    char word[64];
    int wordIdx = 0;

    while (*p) {
        if (*p == ' ' || *(p + 1) == '\0') {
            // End of word
            if (*(p + 1) == '\0' && *p != ' ') {
                word[wordIdx++] = *p;
            }
            word[wordIdx] = '\0';

            if (wordIdx > 0) {
                display.getTextBounds(word, 0, 0, &x1, &y1, &w, &h);
                int wordWidth = w;

                // Check if word fits (include space if not first word)
                int neededWidth = (currentWidth > 0) ? (spaceWidth + wordWidth) : wordWidth;

                if (currentWidth + neededWidth > maxWidth && currentWidth > 0) {
                    // Word doesn't fit, wrap to next line
                    lines++;
                    currentWidth = wordWidth;
                } else {
                    currentWidth += neededWidth;
                }
            }
            wordIdx = 0;
        } else {
            if (wordIdx < 63) word[wordIdx++] = *p;
        }
        p++;
    }

    return lines;
}

// Draw an event line with word wrapping (returns number of lines drawn)
static int draw_event_line(int y, const char* timeStr, const char* title) {
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(Layout::MARGIN + 10, y + 18);
    display.print(timeStr);

    display.setFont(&FreeSans9pt7b);

    // Available width for title (screen width minus margins and time column)
    const int titleStartX = Layout::MARGIN + 80;
    const int maxWidth = SCREEN_WIDTH - titleStartX - Layout::MARGIN;

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);

    // If title fits on one line, just print it
    if ((int)w <= maxWidth) {
        display.setCursor(titleStartX, y + 18);
        display.print(title);
        return 1;
    }

    // Fixed space width for FreeSans9pt7b (getTextBounds doesn't work well for spaces)
    const int spaceWidth = 5;

    // Word wrap the title
    int lineNum = 0;
    int currentX = titleStartX;
    int currentY = y + 18;
    const char* p = title;
    char word[64];
    int wordIdx = 0;
    bool firstWordOnLine = true;

    while (*p) {
        if (*p == ' ' || *(p + 1) == '\0') {
            // End of word
            if (*(p + 1) == '\0' && *p != ' ') {
                word[wordIdx++] = *p;
            }
            word[wordIdx] = '\0';

            if (wordIdx > 0) {
                display.getTextBounds(word, 0, 0, &x1, &y1, &w, &h);
                int wordWidth = w;

                // Check if word fits on current line (include space if not first word)
                int neededWidth = firstWordOnLine ? wordWidth : (spaceWidth + wordWidth);
                if (currentX - titleStartX + neededWidth > maxWidth && !firstWordOnLine) {
                    // Wrap to next line
                    lineNum++;
                    currentX = titleStartX;
                    currentY += Layout::EVENT_HEIGHT;
                    firstWordOnLine = true;
                }

                // Add space before word if not first on line
                if (!firstWordOnLine) {
                    currentX += spaceWidth;
                }

                display.setCursor(currentX, currentY);
                display.print(word);
                currentX += wordWidth;
                firstWordOnLine = false;
            }
            wordIdx = 0;
        } else {
            if (wordIdx < 63) word[wordIdx++] = *p;
        }
        p++;
    }

    return lineNum + 1;  // Return total lines drawn
}

// Draw a complete day block with red border around date + all events
static void draw_day_block(int y, int height) {
    display.drawRect(0, y, SCREEN_WIDTH, height, GxEPD_RED);
}

// Render the page title at the very top ("January 2026")
static void render_page_title(const MonthInfo& month) {
    char title[32];
    snprintf(title, sizeof(title), "%s %d", MONTH_NAMES[month.month], month.year);

    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(get_center_x(title, &FreeSansBold9pt7b), 20);
    display.print(title);

    // Draw line under title
    display.drawLine(0, Layout::PAGE_TITLE_HEIGHT - 2, SCREEN_WIDTH, Layout::PAGE_TITLE_HEIGHT - 2, GxEPD_BLACK);
}

// Get start of today as Unix timestamp (midnight local time in UTC)
static uint32_t get_start_of_today() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return 0;  // Can't filter if no time
    }

    // Set to midnight local time
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;

    // mktime interprets as local time and returns UTC timestamp
    return (uint32_t)mktime(&timeinfo);
}

// Check if an appointment is today or in the future
static bool is_appointment_current(const Appointment& apt, uint32_t startOfToday) {
    if (startOfToday == 0) return true;  // Show all if we can't get time

    if (apt.allDay) {
        // All-day events use UTC midnight, compare date portions
        // The startTime is already UTC midnight for that day
        return apt.startTime >= startOfToday;
    } else {
        // Timed events: check if end time is still in the future
        // Or if it's an ongoing event (started but not ended)
        return apt.endTime >= startOfToday;
    }
}

// Render the appointments section (below page title)
static void render_appointments(const CalendarData& data) {
    int yPos = Layout::APPOINTMENTS_TOP;
    int maxY = Layout::APPOINTMENTS_TOP + Layout::APPOINTMENTS_HEIGHT;
    int i = 0;

    // Calculate max width for title wrapping
    const int titleStartX = Layout::MARGIN + 80;
    const int maxTitleWidth = SCREEN_WIDTH - titleStartX - Layout::MARGIN;

    // Get start of today for filtering
    uint32_t startOfToday = get_start_of_today();
    LOGF("Display: Filtering appointments, startOfToday=%u\n", startOfToday);

    // Skip past appointments
    while (i < data.appointmentCount && !is_appointment_current(data.appointments[i], startOfToday)) {
        LOGF("Display: Skipping past appointment: %s\n", data.appointments[i].title);
        i++;
    }

    while (i < data.appointmentCount && yPos < maxY - Layout::DATE_HEADER_HEIGHT - Layout::EVENT_HEIGHT) {
        // Get the day for current appointment (considering all-day flag)
        const Appointment& firstApt = data.appointments[i];
        int currentDay = get_day_of_month(firstApt.startTime, firstApt.allDay);

        // Count how many events are on this day and calculate total lines needed
        int eventsOnDay = 0;
        int totalLines = 0;
        for (int j = i; j < data.appointmentCount; j++) {
            const Appointment& apt = data.appointments[j];
            if (get_day_of_month(apt.startTime, apt.allDay) == currentDay) {
                eventsOnDay++;
                // Calculate how many lines this event title needs
                int lines = calculate_wrapped_lines(apt.title, maxTitleWidth);
                totalLines += lines;
            } else {
                break;
            }
        }

        // Calculate block height: date header + all lines for all events
        int blockHeight = Layout::DATE_HEADER_HEIGHT + totalLines * Layout::EVENT_HEIGHT;

        // Check if block fits
        if (yPos + blockHeight > maxY) break;

        // Draw red border around entire day block
        draw_day_block(yPos, blockHeight);

        // Draw date header text
        char dateStr[32];
        format_date_header(firstApt.startTime, firstApt.allDay, dateStr, sizeof(dateStr));
        draw_date_header_text(yPos, dateStr);
        yPos += Layout::DATE_HEADER_HEIGHT;

        // Draw all events for this day
        for (int j = 0; j < eventsOnDay; j++) {
            const Appointment& apt = data.appointments[i + j];
            char timeStr[16];
            format_time(apt.startTime, apt.allDay, timeStr, sizeof(timeStr));
            int linesUsed = draw_event_line(yPos, timeStr, apt.title);
            yPos += linesUsed * Layout::EVENT_HEIGHT;
        }

        // Move to next day's events
        i += eventsOnDay;

        // Small gap between day blocks
        yPos += 2;
    }

    // Draw divider between sections
    display.fillRect(0, Layout::DIVIDER_Y, SCREEN_WIDTH, 3, GxEPD_BLACK);
}

// Render day header row (S M T W T F S)
static void render_day_headers() {
    display.setFont(&FreeSans9pt7b);
    int y = Layout::MONTH_TOP + 15;  // Day headers directly at MONTH_TOP

    for (int i = 0; i < 7; i++) {
        // Left-align in cell with padding
        int x = i * Layout::CELL_WIDTH + Layout::CELL_PADDING;
        display.setCursor(x, y);
        display.print(DAY_ABBREV[i]);
    }

    // Draw horizontal line under day headers
    display.drawLine(0, Layout::GRID_TOP - 2, SCREEN_WIDTH, Layout::GRID_TOP - 2, GxEPD_BLACK);
}

// Find all events for a given day (returns count, fills titles array)
static int find_events_for_day(const CalendarData& data, int day, const char** titles, int maxTitles) {
    int count = 0;
    for (int i = 0; i < data.monthEventCount && count < maxTitles; i++) {
        if (data.monthEvents[i].day == day) {
            titles[count++] = data.monthEvents[i].title;
        }
    }
    return count;
}

// Render the month grid (bottom half - title is now at page top)
static void render_month_grid(const CalendarData& data) {
    render_day_headers();

    // Get current day for highlighting
    struct tm timeinfo;
    int today = -1;
    if (getLocalTime(&timeinfo)) {
        today = timeinfo.tm_mday;
    }

    int dayNum = 1;
    int prevMonthDay = data.month.daysInPrevMonth - data.month.firstDayOfWeek + 1;

    // Draw horizontal grid lines
    for (int row = 0; row <= 6; row++) {
        int y = Layout::GRID_TOP + row * Layout::CELL_HEIGHT;
        display.drawLine(0, y, SCREEN_WIDTH, y, GxEPD_BLACK);
    }

    // Draw vertical grid lines
    for (int col = 0; col <= 7; col++) {
        int x = col * Layout::CELL_WIDTH;
        display.drawLine(x, Layout::GRID_TOP, x, Layout::GRID_TOP + 6 * Layout::CELL_HEIGHT, GxEPD_BLACK);
    }

    for (int row = 0; row < 6; row++) {
        int y = Layout::GRID_TOP + row * Layout::CELL_HEIGHT;

        for (int col = 0; col < 7; col++) {
            int x = col * Layout::CELL_WIDTH;

            // Determine what day number to show
            int gridPos = row * 7 + col;
            int displayDay;
            bool isPrevMonth = false;
            bool isNextMonth = false;

            if (gridPos < data.month.firstDayOfWeek) {
                // Previous month
                displayDay = prevMonthDay++;
                isPrevMonth = true;
            } else if (dayNum <= data.month.daysInMonth) {
                // Current month
                displayDay = dayNum++;
            } else {
                // Next month
                displayDay = dayNum++ - data.month.daysInMonth;
                isNextMonth = true;
            }

            // Draw day number - LEFT aligned with 1px top padding
            char dayStr[4];
            snprintf(dayStr, sizeof(dayStr), "%d", displayDay);

            display.setFont(&FreeSans9pt7b);

            // Position: left-aligned with padding, 1px from top
            int dayX = x + Layout::CELL_PADDING;
            int dayY = y + 1 + Layout::CELL_DAY_OFFSET_Y;  // 1px top padding

            // Set color based on month
            if (isPrevMonth || isNextMonth) {
                display.setTextColor(GxEPD_RED);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }

            // Highlight today with red box and padding
            if (!isPrevMonth && !isNextMonth && displayDay == today) {
                display.fillRect(x + 2, y + 2, 24, 18, GxEPD_RED);
                display.setTextColor(GxEPD_WHITE);
            }

            display.setCursor(dayX, dayY);
            display.print(dayStr);

            // Reset text color
            display.setTextColor(GxEPD_BLACK);

            // Draw abbreviated events (if any, and only for current month)
            if (!isPrevMonth && !isNextMonth) {
                const char* eventTitles[3];  // Max 3 events per day in grid
                int eventCount = find_events_for_day(data, displayDay, eventTitles, 3);

                if (eventCount > 0) {
                    // Use built-in tiny font for events (5x8 pixels)
                    display.setFont(NULL);
                    display.setTextSize(1);

                    for (int e = 0; e < eventCount && e < 3; e++) {
                        char shortTitle[10];
                        strncpy(shortTitle, eventTitles[e], 9);
                        shortTitle[9] = '\0';

                        int eventY = y + Layout::CELL_EVENT_OFFSET_Y + e * Layout::CELL_EVENT_LINE_HEIGHT;
                        display.setCursor(x + Layout::CELL_PADDING, eventY);
                        display.print(shortTitle);
                    }

                    // Reset to default font
                    display.setFont(&FreeSans9pt7b);
                }
            }
        }
    }
}

// Draw graphical battery indicator (or USB icon when powered by USB)
static void draw_battery_indicator(int x, int y, int percentage, bool isUsbPowered) {
    const int battW = 40;
    const int battH = 16;
    const int tipW = 3;
    const int tipH = 8;
    const int barCount = 4;
    const int barW = 8;
    const int barGap = 1;
    const int barMargin = 2;

    if (isUsbPowered) {
        // Draw USB/plug indicator instead of battery
        // Simple plug shape: rectangle with prongs
        display.drawRect(x, y, battW, battH, GxEPD_BLACK);
        // Draw "USB" text inside
        display.setFont(NULL);  // Built-in 5x8 font
        display.setTextSize(1);
        display.setCursor(x + 8, y + 4);
        display.print("USB");
        display.setFont(&FreeSans9pt7b);
        return;
    }

    // Battery outline
    display.drawRect(x, y, battW, battH, GxEPD_BLACK);
    display.fillRect(x + battW, y + (battH - tipH) / 2, tipW, tipH, GxEPD_BLACK);

    // Calculate filled bars
    int bars = (percentage + 12) / 25;
    if (bars < 1) bars = 1;
    if (bars > barCount) bars = barCount;

    for (int i = 0; i < bars; i++) {
        int bx = x + barMargin + i * (barW + barGap);
        display.fillRect(bx, y + barMargin, barW, battH - 2 * barMargin, GxEPD_BLACK);
    }
}

// Draw a small warning triangle icon
static void draw_warning_icon(int x, int y) {
    // Triangle: 14px wide, 12px tall
    display.fillTriangle(x + 7, y, x, y + 12, x + 14, y + 12, GxEPD_RED);
    // Exclamation mark in white
    display.drawLine(x + 7, y + 3, x + 7, y + 8, GxEPD_WHITE);
    display.drawPixel(x + 7, y + 10, GxEPD_WHITE);
}

// Render footer (time + battery + connection warning)
static void render_footer(const BatteryStatus& battery, CacheStatus cacheStatus,
                          const char* statusNote) {
    display.setFont(&FreeSans9pt7b);

    if (cacheStatus != CacheStatus::None) {
        // Connection failed - show the cause-specific note instead of the date
        int iconX = Layout::MARGIN;
        int iconY = Layout::FOOTER_Y - 12;
        draw_warning_icon(iconX, iconY);

        display.setTextColor(GxEPD_RED);
        display.setCursor(iconX + 18, Layout::FOOTER_Y);
        display.print(statusNote ? statusNote : "Unable to connect to your calendar");
        display.setTextColor(GxEPD_BLACK);
    } else {
        // Normal - show last update time
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", &timeinfo);
            display.setCursor(Layout::MARGIN, Layout::FOOTER_Y);
            display.print(timeStr);
        }
    }

    // Battery indicator
    draw_battery_indicator(SCREEN_WIDTH - Layout::MARGIN - 45, Layout::FOOTER_Y - 12, battery.percentage, battery.isUsbPowered);
}

void display_init() {
    LOG("Display: Initializing");
    SPI.begin(SPI_CLK, -1, SPI_MOSI, EPD_CS);
    LOG("Display: SPI initialized for C6");
    display.init(SERIAL_BAUD);
    display.setRotation(DISPLAY_ROTATION);
    display.setTextColor(GxEPD_BLACK);
    LOG("Display: Ready");
}

void display_calendar(const CalendarData& data, const BatteryStatus& battery, CacheStatus cacheStatus,
                      const char* statusNote) {
    LOG("Display: Rendering calendar");

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        render_page_title(data.month);  // "January 2026" at the very top
        render_appointments(data);
        render_month_grid(data);
        render_footer(battery, cacheStatus, statusNote);

    } while (display.nextPage());

    LOG("Display: Render complete");
}

void display_error(const char* message, const char* hint) {
    LOG("Display: Rendering error");
    LOGF("Display: Error message: %s\n", message);

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        display.setFont(&FreeSansBold18pt7b);
        display.setTextColor(GxEPD_RED);
        const char* title = "Error";
        display.setCursor(get_center_x(title, &FreeSansBold18pt7b), 380);
        display.print(title);
        display.setTextColor(GxEPD_BLACK);

        display.setFont(&FreeSans12pt7b);
        display.setCursor(get_center_x(message, &FreeSans12pt7b), 420);
        display.print(message);

        if (hint) {
            // The self-service remedy, e.g. "Reboot router, or re-add device
            // in router app" - smaller font so a full sentence fits the width.
            display.setFont(&FreeSans9pt7b);
            display.setCursor(get_center_x(hint, &FreeSans9pt7b), 452);
            display.print(hint);
            display.setFont(&FreeSans12pt7b);
        }

        const char* retry = "Will retry soon";
        display.setCursor(get_center_x(retry, &FreeSans12pt7b), 490);
        display.print(retry);

    } while (display.nextPage());
}

void display_status(const char* line1, const char* line2, const char* line3) {
    LOG("Display: Status update");

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(get_center_x(line1, &FreeSansBold18pt7b), 380);
        display.print(line1);

        if (line2) {
            display.setFont(&FreeSans12pt7b);
            display.setCursor(get_center_x(line2, &FreeSans12pt7b), 420);
            display.print(line2);
        }

        if (line3) {
            display.setFont(&FreeSans12pt7b);
            display.setCursor(get_center_x(line3, &FreeSans12pt7b), 460);
            display.print(line3);
        }

    } while (display.nextPage());
}

void display_sleep() {
    LOG("Display: Entering sleep mode");
    display.hibernate();
}

#else // NATIVE_BUILD

#include "Arduino.h"
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

void display_init() {
    LOG("Display: Initializing (simulation)");
}

void display_calendar(const CalendarData& data, const BatteryStatus& battery, CacheStatus cacheStatus) {
    LOG("Display: Rendering calendar (simulation)");
    LOGF("  Appointments: %d\n", data.appointmentCount);
    LOGF("  Month events: %d\n", data.monthEventCount);
    LOGF("  Month: %d/%d\n", data.month.month, data.month.year);
    if (cacheStatus != CacheStatus::None) {
        LOGF("  Cache status: %s\n", cacheStatus == CacheStatus::Fresh ? "Fresh" : "Stale");
    }
}

void display_error(const char* message) {
    LOGF("Display: Error - %s\n", message);
}

void display_status(const char* line1, const char* line2, const char* line3) {
    LOG("Display: Status");
    LOGF("  %s\n", line1);
    if (line2) LOGF("  %s\n", line2);
    if (line3) LOGF("  %s\n", line3);
}

void display_sleep() {
    LOG("Display: Sleep (simulation)");
}

#endif // NATIVE_BUILD
