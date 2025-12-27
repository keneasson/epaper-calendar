#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "schedule.h"

// Fetch schedule from configured API endpoint
// Populates the provided Schedule struct on success
// Returns Success or appropriate error code
ResultCode api_fetch_schedule(Schedule& schedule);

#endif // API_CLIENT_H
