#include "display.h"
#include "config.h"

// Only include display libraries when not in native build
#ifndef NATIVE_BUILD

#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <time.h>

#if DEBUG_MODE
#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// Display dimensions (Waveshare 7.5" V2)
static constexpr int SCREEN_WIDTH = 800;
static constexpr int SCREEN_HEIGHT = 480;

// Layout constants
namespace Layout {
    // Margins
    constexpr int MARGIN_LEFT = 50;
    constexpr int MARGIN_RIGHT = 50;

    // Header section (two lines)
    constexpr int HEADER_Y = 40;
    constexpr int HEADER_LINE_HEIGHT = 35;
    constexpr int DIVIDER_TOP_Y = 115;

    // Date section
    constexpr int DATE_Y = 150;
    constexpr int DIVIDER_DATE_Y = 170;

    // Service roles section
    constexpr int ROLES_START_Y = 200;
    constexpr int ROLE_LABEL_X = MARGIN_LEFT;
    constexpr int ROLE_VALUE_X = 280;
    constexpr int ROLE_LINE_HEIGHT = 38;

    // Collection and message section
    constexpr int MESSAGE_Y = 400;

    // Footer section
    constexpr int DIVIDER_BOTTOM_Y = 450;
    constexpr int FOOTER_Y = 470;
}

// Display instance
static GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(
    GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// Get X position to center text on screen
static int get_center_x(const char* text, const GFXfont* font) {
    int16_t x1, y1;
    uint16_t w, h;
    display.setFont(font);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    return (x < 0) ? 0 : x;  // Clamp to screen bounds
}

// Draw a horizontal line
static void draw_divider(int y) {
    display.drawLine(Layout::MARGIN_LEFT, y,
                     SCREEN_WIDTH - Layout::MARGIN_RIGHT, y,
                     GxEPD_BLACK);
}

// Helper to render a role line: "Label: Value"
static void render_role(int y, const char* label, const char* value) {
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(Layout::ROLE_LABEL_X, y);
    display.print(label);
    display.print(":");

    display.setFont(&FreeSans12pt7b);
    display.setCursor(Layout::ROLE_VALUE_X, y);
    display.print(value);
}

// Render header section (church name on two lines, divider)
static void render_header() {
    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(get_center_x(CHURCH_NAME_LINE1, &FreeSansBold24pt7b), Layout::HEADER_Y);
    display.print(CHURCH_NAME_LINE1);
    display.setCursor(get_center_x(CHURCH_NAME_LINE2, &FreeSansBold24pt7b), Layout::HEADER_Y + Layout::HEADER_LINE_HEIGHT);
    display.print(CHURCH_NAME_LINE2);

    draw_divider(Layout::DIVIDER_TOP_Y);
}

// Render the service date
static void render_date(const char* dateStr) {
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(get_center_x(dateStr, &FreeSansBold18pt7b), Layout::DATE_Y);
    display.print(dateStr);

    draw_divider(Layout::DIVIDER_DATE_Y);
}

// Render service roles
static void render_roles(const Schedule& schedule) {
    int yPos = Layout::ROLES_START_Y;

    render_role(yPos, "Presiding", schedule.presiding);
    yPos += Layout::ROLE_LINE_HEIGHT;

    render_role(yPos, "Exhorting", schedule.exhorting);
    yPos += Layout::ROLE_LINE_HEIGHT;

    render_role(yPos, "Keyboard", schedule.keyboard);
    yPos += Layout::ROLE_LINE_HEIGHT;

    render_role(yPos, "Steward", schedule.steward);
    yPos += Layout::ROLE_LINE_HEIGHT;

    render_role(yPos, "Doorkeeper", schedule.doorkeeper);
    yPos += Layout::ROLE_LINE_HEIGHT;

    // 2nd Collection (if present)
    if (schedule.hasCollection()) {
        yPos += 10;  // Extra spacing before collection
        render_role(yPos, "2nd Collection", schedule.collection);
    }

    // Fellowship message (two lines)
    display.setFont(&FreeSans12pt7b);
    const char* msgLine1 = schedule.hasLunch ? "Please stay for" : "Fellowship activities";
    const char* msgLine2 = schedule.hasLunch ? "lunch and fellowship." : "are encouraged.";
    display.setCursor(get_center_x(msgLine1, &FreeSans12pt7b), Layout::MESSAGE_Y);
    display.print(msgLine1);
    display.setCursor(get_center_x(msgLine2, &FreeSans12pt7b), Layout::MESSAGE_Y + 25);
    display.print(msgLine2);
}

// Render footer section (last update time, battery status)
static void render_footer(const BatteryStatus& battery) {
    draw_divider(Layout::DIVIDER_BOTTOM_Y);

    display.setFont(&FreeSans9pt7b);

    // Last update time
    if (SHOW_LAST_UPDATE) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[50];
            strftime(timeStr, sizeof(timeStr), "Updated: %b %d, %Y %I:%M %p", &timeinfo);
            display.setCursor(Layout::MARGIN_LEFT, Layout::FOOTER_Y);
            display.print(timeStr);
        }
    }

    // Battery status
    if (SHOW_BATTERY) {
        char battStr[30];
        snprintf(battStr, sizeof(battStr), "Battery: %d%% (%.2fV)",
                 battery.percentage, battery.voltage);
        // Right-align battery status
        int16_t x1, y1;
        uint16_t w, h;
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds(battStr, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(SCREEN_WIDTH - Layout::MARGIN_RIGHT - w, Layout::FOOTER_Y);
        display.print(battStr);
    }

    // Low battery warning
    if (battery.isLow) {
        display.setFont(&FreeSansBold12pt7b);
        const char* warning = "LOW BATTERY - CHARGE SOON";
        display.setCursor(get_center_x(warning, &FreeSansBold12pt7b), Layout::FOOTER_Y + 20);
        display.print(warning);
    }
}

void display_init() {
    LOG("Display: Initializing");
    display.init(SERIAL_BAUD);
    display.setRotation(DISPLAY_ROTATION);
    display.setTextColor(GxEPD_BLACK);
    LOG("Display: Ready");
}

void display_schedule(const Schedule& schedule, const BatteryStatus& battery) {
    LOG("Display: Rendering schedule");

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        render_header();
        render_date(schedule.serviceDate);
        render_roles(schedule);
        render_footer(battery);

    } while (display.nextPage());

    LOG("Display: Render complete");
}

void display_error(const char* message) {
    LOG("Display: Rendering error");
    LOGF("Display: Error message: %s\n", message);

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        // Error title
        display.setFont(&FreeSansBold24pt7b);
        const char* title = "Error";
        display.setCursor(get_center_x(title, &FreeSansBold24pt7b), 200);
        display.print(title);

        // Error message
        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(get_center_x(message, &FreeSansBold18pt7b), 250);
        display.print(message);

        // Retry notice
        display.setFont(&FreeSans12pt7b);
        const char* retry = "Will retry soon";
        display.setCursor(get_center_x(retry, &FreeSans12pt7b), 300);
        display.print(retry);

    } while (display.nextPage());

    LOG("Display: Error render complete");
}

void display_sleep() {
    LOG("Display: Entering sleep mode");
    display.hibernate();
}

#else // NATIVE_BUILD - simulation implementations

#include "Arduino.h"
#include "GxEPD2_BW.h"
#include "time_sim.h"
#include <cstdio>

#define LOG(msg) Serial.println(msg)
#define LOGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

// Display dimensions - Portrait mode
static constexpr int SCREEN_WIDTH = 480;
static constexpr int SCREEN_HEIGHT = 800;

// Layout constants for portrait orientation
namespace Layout {
    constexpr int MARGIN_LEFT = 30;
    constexpr int MARGIN_RIGHT = 30;

    // Header section (two lines)
    constexpr int HEADER_Y = 50;
    constexpr int HEADER_LINE_HEIGHT = 30;
    constexpr int DIVIDER_TOP_Y = 115;

    // Date section
    constexpr int DATE_Y = 160;
    constexpr int DIVIDER_DATE_Y = 185;

    // Service roles section
    constexpr int ROLES_START_Y = 235;
    constexpr int ROLE_LABEL_X = MARGIN_LEFT;
    constexpr int ROLE_VALUE_X = 220;
    constexpr int ROLE_LINE_HEIGHT = 55;

    // Collection and message section
    constexpr int MESSAGE_Y = 560;

    // Footer section
    constexpr int DIVIDER_BOTTOM_Y = 740;
    constexpr int FOOTER_Y = 770;
}

static DisplaySimulator display;

static int get_center_x(const char* text, const GFXfont* font) {
    int16_t x1, y1;
    uint16_t w, h;
    display.setFont(font);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    return (x < 0) ? 0 : x;  // Clamp to screen bounds
}

// Helper to render a role line: "Label: Value"
static void render_role(int y, const char* label, const char* value) {
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(Layout::ROLE_LABEL_X, y);
    display.print(label);
    display.print(":");

    display.setFont(&FreeSans12pt7b);
    display.setCursor(Layout::ROLE_VALUE_X, y);
    display.print(value);
}

void display_init() {
    LOG("Display: Initializing (simulation)");
    display.init(SERIAL_BAUD);
    display.setRotation(DISPLAY_ROTATION);
    display.setTextColor(GxEPD_BLACK);
    LOG("Display: Ready");
}

void display_schedule(const Schedule& schedule, const BatteryStatus& battery) {
    LOG("Display: Rendering schedule (simulation)");

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        // Header - Church name (two lines)
        display.setFont(&FreeSansBold24pt7b);
        display.setCursor(get_center_x(CHURCH_NAME_LINE1, &FreeSansBold24pt7b), Layout::HEADER_Y);
        display.print(CHURCH_NAME_LINE1);
        display.setCursor(get_center_x(CHURCH_NAME_LINE2, &FreeSansBold24pt7b), Layout::HEADER_Y + Layout::HEADER_LINE_HEIGHT);
        display.print(CHURCH_NAME_LINE2);

        display.drawLine(Layout::MARGIN_LEFT, Layout::DIVIDER_TOP_Y,
                        SCREEN_WIDTH - Layout::MARGIN_RIGHT, Layout::DIVIDER_TOP_Y, GxEPD_BLACK);

        // Date
        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(get_center_x(schedule.serviceDate, &FreeSansBold18pt7b), Layout::DATE_Y);
        display.print(schedule.serviceDate);

        display.drawLine(Layout::MARGIN_LEFT, Layout::DIVIDER_DATE_Y,
                        SCREEN_WIDTH - Layout::MARGIN_RIGHT, Layout::DIVIDER_DATE_Y, GxEPD_BLACK);

        // Service roles
        int yPos = Layout::ROLES_START_Y;
        render_role(yPos, "Presiding", schedule.presiding);
        yPos += Layout::ROLE_LINE_HEIGHT;

        render_role(yPos, "Exhorting", schedule.exhorting);
        yPos += Layout::ROLE_LINE_HEIGHT;

        render_role(yPos, "Keyboard", schedule.keyboard);
        yPos += Layout::ROLE_LINE_HEIGHT;

        render_role(yPos, "Steward", schedule.steward);
        yPos += Layout::ROLE_LINE_HEIGHT;

        render_role(yPos, "Doorkeeper", schedule.doorkeeper);
        yPos += Layout::ROLE_LINE_HEIGHT;

        // 2nd Collection (if present)
        if (schedule.hasCollection()) {
            yPos += 20;  // Extra spacing before collection
            render_role(yPos, "2nd Collection", schedule.collection);
        }

        // Fellowship message (two lines)
        display.setFont(&FreeSans12pt7b);
        const char* msgLine1 = schedule.hasLunch ? "Please stay for" : "Fellowship activities";
        const char* msgLine2 = schedule.hasLunch ? "lunch and fellowship." : "are encouraged.";
        display.setCursor(get_center_x(msgLine1, &FreeSans12pt7b), Layout::MESSAGE_Y);
        display.print(msgLine1);
        display.setCursor(get_center_x(msgLine2, &FreeSans12pt7b), Layout::MESSAGE_Y + 25);
        display.print(msgLine2);

        // Footer
        display.drawLine(Layout::MARGIN_LEFT, Layout::DIVIDER_BOTTOM_Y,
                        SCREEN_WIDTH - Layout::MARGIN_RIGHT, Layout::DIVIDER_BOTTOM_Y, GxEPD_BLACK);

        // Date (YYYY/MM/DD format)
        display.setFont(&FreeSans9pt7b);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char dateStr[20];
            strftime(dateStr, sizeof(dateStr), "%Y/%m/%d", &timeinfo);
            display.setCursor(Layout::MARGIN_LEFT, Layout::FOOTER_Y);
            display.print(dateStr);
        }

        // Battery indicator (4 bars)
        // Calculate bars: 1 bar = needs charge, 4 bars = full
        int bars = 1;
        if (battery.percentage > 75) bars = 4;
        else if (battery.percentage > 50) bars = 3;
        else if (battery.percentage > 25) bars = 2;

        int battX = SCREEN_WIDTH - Layout::MARGIN_RIGHT - 45;
        int battY = Layout::FOOTER_Y - 12;
        int battW = 40;
        int battH = 16;
        int barW = 8;
        int barGap = 2;

        // Battery outline
        display.drawLine(battX, battY, battX + battW, battY, GxEPD_BLACK);
        display.drawLine(battX, battY + battH, battX + battW, battY + battH, GxEPD_BLACK);
        display.drawLine(battX, battY, battX, battY + battH, GxEPD_BLACK);
        display.drawLine(battX + battW, battY, battX + battW, battY + battH, GxEPD_BLACK);
        // Battery tip
        display.drawLine(battX + battW + 1, battY + 4, battX + battW + 3, battY + 4, GxEPD_BLACK);
        display.drawLine(battX + battW + 1, battY + battH - 4, battX + battW + 3, battY + battH - 4, GxEPD_BLACK);
        display.drawLine(battX + battW + 3, battY + 4, battX + battW + 3, battY + battH - 4, GxEPD_BLACK);

        // Fill bars
        for (int i = 0; i < bars; i++) {
            int bx = battX + 2 + i * (barW + barGap);
            for (int y = battY + 2; y < battY + battH - 1; y++) {
                display.drawLine(bx, y, bx + barW - 1, y, GxEPD_BLACK);
            }
        }

    } while (display.nextPage());

    LOG("Display: Render complete (simulation)");
}

void display_error(const char* message) {
    LOG("Display: Rendering error (simulation)");
    LOGF("Display: Error message: %s\n", message);

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        display.setFont(&FreeSansBold24pt7b);
        display.setCursor(get_center_x("Error", &FreeSansBold24pt7b), 200);
        display.print("Error");

        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(get_center_x(message, &FreeSansBold18pt7b), 250);
        display.print(message);

        display.setFont(&FreeSans12pt7b);
        display.setCursor(get_center_x("Will retry soon", &FreeSans12pt7b), 300);
        display.print("Will retry soon");

    } while (display.nextPage());
}

void display_sleep() {
    LOG("Display: Entering sleep mode (simulation)");
    display.hibernate();
}

#endif // NATIVE_BUILD
