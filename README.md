# Calendar — ESP32 E-Paper Calendar Displays

Low-power ESP32 displays that show your calendar on e-paper and sleep for months on a battery — a buildable open-hardware product, plus the firmware, serverless backend, and bench tools behind it.

The flagship **home-calendar** pulls a Google Calendar through a serverless API and renders it on a 7.5" three-color e-paper panel, then deep-sleeps between hourly sync checks. It's built for real homes on real routers: the WiFi stack negotiates modern WiFi-6 / WPA3 gateways and turns connection failures into plain-language, fix-it-yourself messages on the screen instead of opaque error codes.

## Repository structure

```
Calendar/
├── apps/
│   ├── home-calendar/       ★ Production firmware — ESP32-C6 + 7.5" 3-color e-paper
│   ├── home-calendar-api/     Serverless backend (AWS Lambda via SST) — Google Calendar
│   ├── church-calendar/       Church schedule display — ESP32-E / C6, with native simulator
│   ├── ble-test/              ESP32-C6 BLE server (provisioning bench)
│   ├── ble-idf-test/          ESP32-S3 BLE client / WiFi provisioner (LCD + IMU)
│   ├── lcd_control/           ESP32-S3 control unit (BLE scan + provisioning + touch UI)
│   └── lcd-demo/              ESP32-S3 LCD tilt-navigation demo
├── libs/                      Shared libraries (battery monitor, S3 LCD driver)
├── docs/                      Assembly, wiring, provisioning, quick start
├── tools/                     Bench / diagnostic tooling (serial capture)
└── reference/                 Vendor SDKs — local only, gitignored
```

## Apps

| App | Target | What it does |
|-----|--------|--------------|
| **home-calendar** ★ | ESP32-C6 | Production firmware: Google Calendar on a 7.5" 3-color e-paper panel (480×800 portrait), resilient WiFi, SPIFFS offline cache, deep sleep |
| **home-calendar-api** | AWS Lambda (SST) | Serverless Google Calendar reader; API-key auth, ETag caching; secrets via SST |
| **church-calendar** | ESP32-E / C6 | Church schedule e-paper display; ships with a native (desktop) simulator |
| **ble-test** | ESP32-C6 | BLE server: battery + WiFi-config services, deep sleep |
| **ble-idf-test** | ESP32-S3 | BLE client / provisioner on a Waveshare S3-LCD-1.47B; WiFi scan, tilt navigation (QMI8658 IMU) |
| **lcd_control** | ESP32-S3 | LCD control unit: BLE scan + WiFi provisioning + touch UI for display units |
| **lcd-demo** | ESP32-S3 | LCD tilt-navigation demo (LovyanGFX) |

### Flagship: home-calendar

- **Hardware:** DFRobot FireBeetle 2 ESP32-C6 · GooDisplay GDEY075Z08 7.5" three-color e-paper · DESPI-C02 adapter · 3.7 V LiPo
- **Resilient WiFi:** a connection-compatibility ladder (802.11ax → b/g/n → low-TX → WPA2-only) for modern WiFi-6 / WPA3 gateways, with 802.11 disconnect-reason classification
- **Self-service errors:** failures render as plain language on the panel — e.g. *"Your router is rejecting your connection"* — instead of an opaque code
- **Months on a charge:** deep sleep between hourly sync checks; the SPIFFS cache redraws the last-known calendar when the network is down

[Full firmware documentation →](apps/home-calendar/README.md)

## Quick start

Firmware apps are standalone [PlatformIO](https://platformio.org/) projects:

```bash
cd apps/home-calendar
pio run -e firebeetle_c6              # build
pio run -e firebeetle_c6 -t upload   # flash
pio device monitor -b 115200         # serial monitor
```

> **home-calendar requires PlatformIO Core ≥ 6.1.19** — its pinned pioarduino 55.03.39 platform (arduino-esp32 3.3.9 / ESP-IDF 5.5.4) depends on it. Run `pio upgrade` if needed.

The backend deploys with [SST](https://sst.dev/):

```bash
cd apps/home-calendar-api
npm install
npm run deploy        # = sst deploy --stage keneasson  (do NOT use --stage prod)
```

## Configuration

Apps that need WiFi / API credentials use a **gitignored `config.h`**:

```bash
cd apps/home-calendar/src
cp config_template.h config.h        # then edit WIFI_SSID, WIFI_PASSWORD, API_ENDPOINT, API_KEY, …
```

`**/config.h` is gitignored repo-wide. The backend keeps its secrets in SST, never in source.

## Contributing & secret scanning

Pull requests are welcome — fork, branch, and open a PR (only the maintainer can write to `main`).

Secrets are taken seriously here:

- **gitleaks runs in CI** on every push and PR (`.github/workflows/secrets-scan.yml`).
- A **local pre-commit hook** scans staged changes. Enable it once per clone:
  ```bash
  git config core.hooksPath .githooks
  brew install gitleaks   # recommended; the hook falls back to a grep scan without it
  ```
- Never commit real credentials — use a gitignored `config.h` (firmware) or SST secrets (backend).

## Hardware platforms

| Platform | Display | MCU | Notes |
|----------|---------|-----|-------|
| GooDisplay GDEY075Z08 | 7.5" 800×480, 3-color | ESP32-C6 | home-calendar; rendered portrait 480×800 |
| Waveshare 7.5" e-paper | 800×480 B/W | ESP32-E / C6 | church-calendar |
| Waveshare ESP32-S3-LCD-1.47B | 172×320 IPS | ESP32-S3 | BLE provisioner, LCD control/demo; QMI8658 IMU |

## Documentation

- [ESP32-C6 + e-paper assembly](docs/epaper-esp32c6-assembly.md) · [C6 firmware setup](docs/esp32c6-firmware-setup.md) · [wiring guide](docs/wiring_guide.md)
- [BLE WiFi provisioning](docs/ble-wifi-provisioning.md) · [provisioning architecture](docs/ble_provisioning_architecture.md)
- [Quick start](docs/QUICK_START.md)

## License

MIT — see individual app directories for specifics.
