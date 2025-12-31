#ifndef DISPLAY_H
#define DISPLAY_H

#include "schedule.h"

// Initialize the e-paper display
void display_init();

// Render the schedule to the display
void display_schedule(const Schedule& schedule, const BatteryStatus& battery);

// Render an error message
void display_error(const char* message);

// Show a status message (for debugging/progress)
void display_status(const char* line1, const char* line2 = nullptr, const char* line3 = nullptr);

// Put display into low-power mode
void display_sleep();

#endif // DISPLAY_H
