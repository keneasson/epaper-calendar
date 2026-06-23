#ifndef DISPLAY_H
#define DISPLAY_H

#include "schedule.h"

// Display mode determines layout and which week to show
enum DisplayMode {
    DISPLAY_MODE_CURRENT = 0,   // Default: show this week's schedule
    DISPLAY_MODE_NEXT_WEEK = 1  // Show next week with red banner
};

// Initialize the e-paper display
void display_init();

// Render the schedule to the display
// mode: DISPLAY_MODE_CURRENT for this week, DISPLAY_MODE_NEXT_WEEK for next week's preview
void display_schedule(const Schedule& schedule, const BatteryStatus& battery, DisplayMode mode = DISPLAY_MODE_CURRENT);

// Render an error message
void display_error(const char* message);

// Show a status message (for debugging/progress)
void display_status(const char* line1, const char* line2 = nullptr, const char* line3 = nullptr);

// Put display into low-power mode
void display_sleep();

#endif // DISPLAY_H
