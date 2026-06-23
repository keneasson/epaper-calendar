#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "schedule.h"

// Fetch schedule from configured API endpoint
// Populates the provided Schedule struct on success
// weekOffset: 0 = this week (default), 1 = next week
// Returns Success or appropriate error code
ResultCode api_fetch_schedule(Schedule& schedule, int weekOffset = 0);

#endif // API_CLIENT_H
