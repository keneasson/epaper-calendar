#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "calendar_data.h"

// Fetch calendar data from API endpoint
// Populates the provided CalendarData struct on success
// Pass existing etag to get 304 (NotModified) if unchanged
// Optionally returns raw JSON in rawJsonOut for caching (if not null)
// Returns Success, NotModified, or appropriate error code
ResultCode api_fetch_calendar(CalendarData& data, const char* currentEtag = nullptr, String* rawJsonOut = nullptr);

#endif // API_CLIENT_H
