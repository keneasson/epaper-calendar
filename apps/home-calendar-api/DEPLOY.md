# Deploying home-calendar-api

> **TL;DR — the live production stack is the SST stage named `keneasson`, NOT `prod`.**
> Deploy with `npm run deploy` (which runs `sst deploy --stage keneasson`). Do **not** use `--stage prod`.

## Why this matters (the trap)

The live API at **https://api.kene.info/calendar** is served by the SST stage
**`keneasson`** (the machine's default stage, see `.sst/stage`). This is the
"production" deployment even though the stage is named after the developer.

There is **no `prod` stack.** `prod` was only ever a string in an old
`package.json` deploy script and was never successfully deployed. Running
`sst deploy --stage prod` creates a **second, parallel stack** that tries to
claim the same custom domain `api.kene.info`, which fails on the existing ACM
validation CNAME — after partially creating orphaned duplicate resources
(API Gateway + ACM certificate). If you ever see a CNAME-already-exists error,
this is why: you're deploying the wrong stage.

## How to deploy

```bash
cd apps/home-calendar-api
npm run deploy          # = sst deploy --stage keneasson
```

## How to verify after deploy

```bash
KEY=<API_KEY>           # same value as the device's src/config.h API_KEY
# 1) Healthy + returns data:
curl -s -o /dev/null -w "%{http_code}\n" -H "X-Api-Key: $KEY" https://api.kene.info/calendar   # -> 200
# 2) ETag is stable across repeated calls (the fix for hourly redraws):
curl -sD - -o /dev/null -H "X-Api-Key: $KEY" https://api.kene.info/calendar | grep -i etag
# 3) If-None-Match returns 304 (device skips redraw when unchanged):
curl -s -o /dev/null -w "%{http_code}\n" -H "X-Api-Key: $KEY" -H "If-None-Match: <etag>" https://api.kene.info/calendar  # -> 304
```

## Facts

- **AWS account:** `911911532459`, region **`ca-central-1`**.
- **App / stage:** `home-calendar-api` / `keneasson`.
- **Custom domain:** `api.kene.info` (Route53 zone `ZY38SC8MUHDWN`), ACM cert
  `…/57d2c831-d6a1-403e-bea3-fe80a23fd3f3`, API Gateway v2 `0qmvy1qfu8`.
- **Secrets** (already set on the `keneasson` stage; view with `sst secret list`):
  `ApiKey`, `CalendarId` (`ken.easson@gmail.com`), `GoogleCredentials`
  (service-account JSON, also in `google-credentials.json`).
  These are NOT set on any other stage — another reason a `prod` deploy fails.

## If you need to change the production stage name

It is risky (re-creating the domain/cert). Prefer keeping `keneasson`. If you
must, do a fresh deploy to the new stage, set its secrets, repoint the domain,
verify, then `sst remove` the old stage.
