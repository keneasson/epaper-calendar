/// <reference path="./.sst/platform/config.d.ts" />

export default $config({
  app(input) {
    return {
      name: "home-calendar-api",
      removal: input?.stage === "prod" ? "retain" : "remove",
      home: "aws",
      providers: {
        aws: {
          region: "ca-central-1",
        },
      },
    };
  },
  async run() {
    // API Key for authentication (stored in SST secrets)
    const apiKey = new sst.Secret("ApiKey");

    // Google Service Account credentials (stored in SST secrets)
    const googleCredentials = new sst.Secret("GoogleCredentials");

    // Calendar ID (user's email address)
    const calendarId = new sst.Secret("CalendarId");

    // Lambda function for calendar API
    const api = new sst.aws.ApiGatewayV2("CalendarApi", {
      domain: {
        name: "api.kene.info",
        dns: sst.aws.dns({ zone: "ZY38SC8MUHDWN" }),
      },
      cors: {
        allowOrigins: ["*"],
        allowMethods: ["GET"],
      },
    });

    api.route("GET /calendar", {
      handler: "functions/calendar.handler",
      environment: {
        API_KEY: apiKey.value,
        GOOGLE_CREDENTIALS: googleCredentials.value,
        CALENDAR_ID: calendarId.value,
      },
      timeout: "30 seconds",
    });

    // Output the API URL
    return {
      apiUrl: api.url,
    };
  },
});
