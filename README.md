# Islamic Utilities Suite

Two C11 command-line programs using [libmuslim](https://github.com/muslimtify-org/libmuslim):
`prayer` calculates prayer schedules; `hijri` converts civil Gregorian and tabular
Hijri dates. No network access occurs unless `prayer --auto` is explicitly used.

## Build

Requires a C11 compiler, GNU make, libm, and a POSIX environment. Linux is the
tested platform. Install the host's timezone database (`tzdata`). Python 3.9+
with `zoneinfo` is needed only for `make check`.

```sh
make
make check
./prayer/prayer --help
./hijri/hijri --from-gregorian 2024-03-11
```

System libmuslim headers are preferred. Unmodified pinned headers are included
as a fallback so the checkout builds offline. `prayer` also uses cJSON, preferring
`pkg-config libcjson` when available and otherwise compiling the included cJSON
source. `hijri` has no JSON-library runtime dependency. See
[dependency provenance](vendor/README.md).

```sh
make -C prayer check
make -C hijri check
make CFLAGS='-O2 -g -Werror'
make MUSLIM_DIR=/path/to/libmuslim
make SYSTEM_LIBMUSLIM=1 SYSTEM_CJSON=1
make install DESTDIR="$PWD/build/stage" PREFIX=/usr
```

`CC`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, and `LDLIBS` are honored. Normal Linux builds
add warnings, stack protection, PIE, RELRO/NOW, and unused-section removal.
`HARDEN=0` disables platform-specific hardening. Build configuration changes
trigger recompilation. Each directory's Makefile supports `all`, `check`,
`install`, `uninstall`, `clean`, and `dist`; cleaning removes the shared build
directory and both binaries. Do not run independent make processes concurrently
against the same checkout; use the root `make -j` instead.

The default binaries embed libmuslim but dynamically link libc/libm (and system
cJSON when selected). Fully static binaries are opt-in:

```sh
make STATIC=1 SYSTEM_CJSON=0 CFLAGS='-Os' LDFLAGS='-s'
```

This requires your compiler's static libc/libm archives. They are not installed
on the development host, so a fully static build remains unverified. Static
binaries still need external timezone files; automatic location still needs
the separately installed geome executable. openSUSE templates use dynamic
linking for distro integration.

## Prayer

```sh
./prayer/prayer --coordinates 32.7765,-79.9311 --timezone America/New_York
./prayer/prayer --config examples/locations.json --location 'Charleston, US' --next
./prayer/prayer --config examples/locations.json --current
./prayer/prayer --config examples/locations.json --method isna --hijri --json
./prayer/prayer --auto --timezone America/New_York
```

Supported requested methods: `mwl` (default), `isna`, `ummalqura` (upstream
`makkah`), `egypt`, `karachi`, `kemenag`, `gulf`, and `moonsighting`. Additional
named upstream methods are also accepted; unknown/custom methods fail. The
upstream parameter sets are used unchanged, including the fixed 90-minute Isha
interval for Makkah; no Ramadan adjustment is added by this application.

### Offline Locations

`--location` performs an exact, case-sensitive lookup in a UTF-8 JSON file.
Default path: `$XDG_CONFIG_HOME/prayer/locations.json`, or
`$HOME/.config/prayer/locations.json` when XDG_CONFIG_HOME is unset or relative.
`--config FILE` overrides the path. The format is:

```json
{
  "default": "Charleston, US",
  "locations": {
    "Charleston, US": {
      "latitude": 32.7765,
      "longitude": -79.9311,
      "timezone": "America/New_York"
    }
  }
}
```

With no arguments, the configured default is used. Missing configuration or
location is an error, not implicit geolocation. Configurations are never written
by the program. `--coordinates LAT,LON` works without config or geome; optional
`--location` labels those coordinates. `--timezone` overrides the configured
zone. Explicit zones must exist in `/usr/share/zoneinfo`. Without a configured
or explicit zone, libc's system timezone (including `TZ`) is used; the output
names it `system` if its IANA name cannot be discovered.

### Geome Compatibility and Privacy

The installed/upstream geome 1.0 supports `--json`, **not** `--lat`, `--lon`,
`--timezone`, or `--lookup`. `--auto` invokes `geome --json` exactly once via
`fork`/`exec`, without a shell, and reads `city`, `lat`, and `lon`. It accepts an
optional string `timezone` for compatible future implementations. Otherwise it
uses the system timezone and prints a warning to stderr. Use `--timezone` when
the selected location is in a different timezone.

City-name network lookup is therefore not implemented; configure coordinates
for named cities instead. Invalid/missing coordinates, duplicate keys, malformed
JSON, control characters in labels, nonzero exit status, responses over 64 KiB,
and execution beyond 15 seconds are errors. No IP, ISP, or provider metadata is
retained or echoed. The runtime needs geome only for `--auto`.

Geome sends an HTTPS request to `ipwho.is`, which receives your public IP. VPNs,
proxies, and provider inaccuracies can yield another location. Coordinates are
approximate, not GPS. No live geolocation request is made by the test suite.

### Time and Calendar Conventions

- Prayer dates are supported from 1600 through 2200 CE. libmuslim's documented
  independent ephemeris validation is narrower (1900-2100); calculations are
  estimates, not authoritative local timetables.
- UTC event timestamps retain upstream negative/greater-than-24-hour values.
  The timezone offset is applied at each event, so DST transitions and adjacent
  dates are not inferred from today's fixed UTC offset.
- Times round upward to the minute, matching libmuslim's formatter. Window
  comparisons use those same rounded timestamps. An event exactly at `now`
  starts the current window; `--next` is strictly later than `now`.
- Fajr ends at sunrise. There is no active obligatory-prayer window from sunrise
  to Dhuhr. Other windows end at the next prayer, with Isha ending at next Fajr
  for scheduling purposes. This is not a religious ruling on preferred times.
- Sunrise was removed from upstream's public struct. It is calculated by a
  separate libmuslim call with its documented 0.833-degree horizon angle, no
  precautionary delay, and no high-latitude substitution.
- The five prayers retain the selected upstream method's high-latitude
  substitutions. Sunrise may be unavailable even when estimated prayers are
  present. Missing boundaries produce no active window, not fabricated times.
- `--hijri` adds the local civil day's **tabular** Hijri date to human output.
  JSON always contains both dates. A Hijri day here changes at midnight, not
  sunset, and may differ from sighting-based or Umm al-Qura dates.
- `--at SECONDS` selects a Unix timestamp for reproducible schedules and tests.
  Ordinarily the current system time is used.

JSON always returns the complete schedule even with `--next` or `--current`.
The `location`, `method`, `date`, `prayers`, `next`, and `current` objects follow
the requested schema. Additional `prayer_dates`, next `date`, and current
`start_date`/`end_date` fields disambiguate midnight rollover. Missing events and
inactive windows are JSON `null`; human times use `--:--` for unavailable events.
`hijri_calendar` explicitly identifies the tabular policy.

## Hijri

```sh
./hijri/hijri --today
./hijri/hijri --from-gregorian 2026-09-23
./hijri/hijri --to-gregorian 1448-04-10 --json
```

No arguments means `--today`. Conversion modes are mutually exclusive. Dates
must be exactly `YYYY-MM-DD`, with valid month lengths and years 0001-9999 in
both input and output. Gregorian dates before the Hijri epoch (0622-07-19,
proleptic Gregorian) and conversions beyond the output range are rejected.

Both directions use libmuslim's civil tabular calendar (Friday epoch). Its
proleptic Gregorian day helpers avoid the historical Gregorian/Julian switch
in the separate astronomical inverse API. There is no hand-written Islamic
calendar algorithm. Tabular dates are calculated dates, not claims about local
moon sightings or religious observance.

JSON uses the later, nested schema in the specification:

```json
{"input":{"calendar":"gregorian","date":"2026-09-23"},"output":{"calendar":"hijri","date":"1448-04-10"}}
```

Human output is `1448-04-10 AH` or `2026-09-23 CE`. The specification's sample
`1448-02-10` is illustrative and is not the tabular conversion of that date.

## License

This project is licensed under the GNU General Public License v3.0 or later (GPL-3.0-or-later).
You may use, modify, and redistribute this software under the terms of the GPL.
Commercial use is allowed, but derivative works must remain open-source under the same license.

## Verification and Packaging

`make check` runs C reference/boundary tests and offline Python integration
tests. Coverage includes known Jakarta and London prayer times, calendar
round trips across centuries, invalid dates, exact prayer boundaries, midnight,
DST, polar sunrise, JSON escaping, geome/config errors, and output failures.
London reference values come from upstream's pinned Aladhan-comparison tests
with a two-minute tolerance; Jakarta's Fajr comes from upstream's quick start.
These are regression/reference checks, not independent religious validation.

```sh
make check HARDEN=0 \
  CFLAGS='-O1 -g -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie' \
  LDFLAGS='-no-pie'
make check
make dist
```

See [packaging/README.md](packaging/README.md) for the independent openSUSE
templates and release gates. Third-party dependency notices remain in their
respective source files and documentation. No package submission, remote
repository, release, or git commit has been created.