import { APIGatewayProxyEventV2, APIGatewayProxyResultV2 } from "aws-lambda";
import { getCalendarClient, fetchCalendarData } from "./lib/google-calendar.js";

// Response helper
function response(statusCode: number, body: unknown): APIGatewayProxyResultV2 {
  return {
    statusCode,
    headers: {
      "Content-Type": "application/json",
      "Cache-Control": "no-cache",
    },
    body: JSON.stringify(body),
  };
}

export async function handler(
  event: APIGatewayProxyEventV2
): Promise<APIGatewayProxyResultV2> {
  try {
    // Validate API key
    const apiKey = process.env.API_KEY;
    const providedKey =
      event.headers["x-api-key"] || event.headers["X-Api-Key"];

    if (!apiKey || !providedKey || apiKey !== providedKey) {
      return response(401, { error: "Unauthorized" });
    }

    // Get Google credentials
    const googleCredentials = process.env.GOOGLE_CREDENTIALS;
    if (!googleCredentials) {
      console.error("GOOGLE_CREDENTIALS not configured");
      return response(500, { error: "Server configuration error" });
    }

    // Check for If-None-Match header (ETag caching)
    const ifNoneMatch = event.headers["if-none-match"];

    // Get calendar ID (user's email)
    const calendarId = process.env.CALENDAR_ID || "primary";

    // Initialize Google Calendar client
    const calendar = getCalendarClient(googleCredentials);

    // Fetch calendar data
    const data = await fetchCalendarData(calendar, calendarId);

    // Check if ETag matches (304 Not Modified)
    if (ifNoneMatch && ifNoneMatch === data.meta.etag) {
      return {
        statusCode: 304,
        headers: {
          ETag: data.meta.etag,
        },
        body: "",
      };
    }

    // Return full response with ETag
    return {
      statusCode: 200,
      headers: {
        "Content-Type": "application/json",
        "Cache-Control": "no-cache",
        ETag: data.meta.etag,
      },
      body: JSON.stringify(data),
    };
  } catch (error) {
    console.error("Error fetching calendar:", error);
    return response(500, {
      error: "Failed to fetch calendar",
      message: error instanceof Error ? error.message : "Unknown error",
    });
  }
}
