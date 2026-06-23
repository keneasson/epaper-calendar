#ifndef DISPLAY_H
#define DISPLAY_H

#include "calendar_data.h"
#include "cache_manager.h"

// Initialize the e-paper display
void display_init();

// Render the calendar (appointments + month grid)
// cacheStatus: indicates if using cached data (shows indicator in footer)
// statusNote: optional cause-specific footer text shown instead of the generic
//             connection warning when rendering from cache (e.g. "Your router
//             is rejecting your connection"). Ignored when cacheStatus is None.
void display_calendar(const CalendarData& data, const BatteryStatus& battery,
                      CacheStatus cacheStatus = CacheStatus::None,
                      const char* statusNote = nullptr);

// Show an error message on the display.
// hint: optional smaller line under the message with the self-service remedy
//       (e.g. "Reboot router, or re-add device in router app").
void display_error(const char* message, const char* hint = nullptr);

// Show status update (used during boot/refresh)
void display_status(const char* line1, const char* line2 = nullptr, const char* line3 = nullptr);

// Put display into low-power mode
void display_sleep();

#endif // DISPLAY_H
