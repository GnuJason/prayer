/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#include "schedule.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts("Usage: prayer [--location CITY | --auto | --coordinates LAT,LON] [OPTIONS]\n"
         "  --location CITY     Exact name in the offline locations JSON config\n"
         "  --config FILE       Config (default: $XDG_CONFIG_HOME/prayer/locations.json)\n"
         "  --auto              Approximate IP location via geome --json\n"
         "  --coordinates A,B   Offline latitude,longitude; --location can label it\n"
         "  --timezone ZONE     IANA timezone override (otherwise config/system)\n"
         "  --method ID         mwl (default), isna, ummalqura, egypt, karachi,\n"
         "                      kemenag, gulf, moonsighting\n"
         "  --next              Next prayer, including its calendar date\n"
         "  --current           Active window; none between sunrise and Dhuhr\n"
         "  --hijri             Include today's tabular Hijri date\n"
         "  --json              Full structured schedule, dates, next and current\n"
         "  --at SECONDS        Use a Unix timestamp instead of the system clock\n"
         "  --help              Show help without accessing the network\n"
         "  --version           Show version\n"
         "No location options: use the config's default city, or report an error.\n"
         "geome 1.0 has no city lookup or timezone: auto uses the system timezone\n"
         "unless --timezone is given. VPNs/proxies can give the wrong location.\n"
         "Privacy: --auto sends your public IP to geome's HTTPS provider (ipwho.is).\n"
         "Offline modes and help make no network requests. Nothing is stored.\n"
         "Dates: 1600-2200 CE. Times follow libmuslim, including its high-latitude\n"
         "estimates. Isha's displayed window ends at Fajr; this is a scheduling\n"
         "convention, not a ruling about the preferred time for prayer.\n"
         "Exit status: 0 success, 2 usage, 3 data/dependency/I/O failure.");
}

static int coordinates(const char *text, Location *location)
{
    char copy[128];
    if (strlen(text) >= sizeof(copy))
        return 0;
    strcpy(copy, text);
    char *comma = strchr(copy, ',');
    if (!comma)
        return 0;
    *comma++ = '\0';
    return parse_number(copy, -90, 90, &location->latitude)
        && parse_number(comma, -180, 180, &location->longitude);
}

static void json_event(const Event *event, int date)
{
    char text[11];
    if (!event || !event->available) {
        json_string(NULL);
        return;
    }
    if (date)
        event_date(event, text);
    else
        event_clock(event, text);
    json_string(text);
}

static void json_window(Window window, int current)
{
    if (!window.start) {
        fputs("null", stdout);
        return;
    }
    fputs("{\"name\":", stdout);
    json_string(event_names[window.name]);
    fputs(current ? ",\"start\":" : ",\"time\":", stdout);
    json_event(window.start, 0);
    fputs(current ? ",\"start_date\":" : ",\"date\":", stdout);
    json_event(window.start, 1);
    if (current) {
        fputs(",\"end\":", stdout);
        json_event(window.end, 0);
        fputs(",\"end_date\":", stdout);
        json_event(window.end, 1);
    }
    putchar('}');
}

static void print_json(const Location *location, const char *method,
                       const Schedule schedules[SCHEDULE_DAYS], Date hijri,
                       Window next, Window current)
{
    char date[11];
    fputs("{\"location\":{\"city\":", stdout);
    json_string(location->city);
    printf(",\"latitude\":%.10g,\"longitude\":%.10g,\"timezone\":",
           location->latitude, location->longitude);
    json_string(location->timezone);
    fputs("},\"method\":", stdout);
    json_string(method);
    fputs(",\"date\":{\"gregorian\":", stdout);
    date_string(schedules[TODAY].date, date);
    json_string(date);
    fputs(",\"hijri\":", stdout);
    date_string(hijri, date);
    json_string(date);
    fputs("},\"hijri_calendar\":\"tabular\",\"prayers\":{", stdout);
    for (int index = 0; index < EVENT_COUNT; index++) {
        if (index) putchar(',');
        json_string(event_keys[index]);
        putchar(':');
        json_event(&schedules[TODAY].events[index], 0);
    }
    fputs("},\"prayer_dates\":{", stdout);
    for (int index = 0; index < EVENT_COUNT; index++) {
        if (index) putchar(',');
        json_string(event_keys[index]);
        putchar(':');
        json_event(&schedules[TODAY].events[index], 1);
    }
    fputs("},\"next\":", stdout);
    json_window(next, 0);
    fputs(",\"current\":", stdout);
    json_window(current, 1);
    puts("}");
}

static void print_window(Window window, int current)
{
    if (!window.start) {
        puts(current ? "None (no active prayer window)" : "No upcoming prayer available");
        return;
    }
    char clock[6], date[11];
    event_clock(window.start, clock);
    event_date(window.start, date);
    printf("%s %s %s", event_names[window.name], date, clock);
    if (current) {
        event_clock(window.end, clock);
        event_date(window.end, date);
        printf(" - %s %s", date, clock);
    }
    putchar('\n');
}

int main(int argc, char **argv)
{
    enum { OPT_LOCATION = 256, OPT_AUTO, OPT_METHOD, OPT_NEXT, OPT_CURRENT,
           OPT_HIJRI, OPT_JSON, OPT_HELP, OPT_VERSION, OPT_CONFIG,
           OPT_COORDINATES, OPT_TIMEZONE, OPT_AT };
    static const struct option options[] = {
        {"location", required_argument, NULL, OPT_LOCATION},
        {"auto", no_argument, NULL, OPT_AUTO},
        {"method", required_argument, NULL, OPT_METHOD},
        {"next", no_argument, NULL, OPT_NEXT},
        {"current", no_argument, NULL, OPT_CURRENT},
        {"hijri", no_argument, NULL, OPT_HIJRI},
        {"json", no_argument, NULL, OPT_JSON},
        {"help", no_argument, NULL, OPT_HELP},
        {"version", no_argument, NULL, OPT_VERSION},
        {"config", required_argument, NULL, OPT_CONFIG},
        {"coordinates", required_argument, NULL, OPT_COORDINATES},
        {"timezone", required_argument, NULL, OPT_TIMEZONE},
        {"at", required_argument, NULL, OPT_AT},
        {NULL, 0, NULL, 0}
    };
    const char *city = NULL, *config = NULL, *coords = NULL, *zone = NULL;
    const char *method_name = "mwl";
    int automatic = 0, next_only = 0, current_only = 0, show_hijri = 0;
    int json = 0, help = 0, version = 0, option;
    time_t now = time(NULL);
    signal(SIGPIPE, SIG_IGN);
    opterr = 0;
    while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
        switch (option) {
        case OPT_LOCATION: city = optarg; break;
        case OPT_AUTO: automatic = 1; break;
        case OPT_METHOD: method_name = optarg; break;
        case OPT_NEXT: next_only = 1; break;
        case OPT_CURRENT: current_only = 1; break;
        case OPT_HIJRI: show_hijri = 1; break;
        case OPT_JSON: json = 1; break;
        case OPT_HELP: help = 1; break;
        case OPT_VERSION: version = 1; break;
        case OPT_CONFIG: config = optarg; break;
        case OPT_COORDINATES: coords = optarg; break;
        case OPT_TIMEZONE: zone = optarg; break;
        case OPT_AT:
            if (!parse_instant(optarg, &now))
                return fail("prayer", 2, "--at requires an integer Unix timestamp");
            break;
        default: return fail("prayer", 2, "unknown option or missing argument; see --help");
        }
    }
    if (optind != argc)
        return fail("prayer", 2, "unexpected positional argument; see --help");
    if (help) { usage(); return output_status("prayer"); }
    if (version) { puts("prayer " SUITE_VERSION); return output_status("prayer"); }
    if ((automatic && (city || coords || config)) || (coords && config)
        || (next_only && current_only))
        return fail("prayer", 2, "conflicting options; see --help");
    CalcMethod method = !strcmp(method_name, "ummalqura") ? CALC_MAKKAH
                                                       : method_from_string(method_name);
    if (method == CALC_CUSTOM)
        return fail("prayer", 2, "unknown calculation method; see --help");
    method_name = method == CALC_MAKKAH ? "ummalqura" : method_to_string(method);
    if (zone && !select_timezone(zone))
        return fail("prayer", 2, "unknown IANA timezone; install tzdata or correct --timezone");
    Location location = {0};
    char error[256];
    if (coords) {
        if (!coordinates(coords, &location) || (city && !plain_text(city, sizeof(location.city))))
            return fail("prayer", 2, "invalid coordinates or city label");
        snprintf(location.city, sizeof(location.city), "%s", city ? city : "Coordinates");
    } else if (automatic) {
        if (!location_auto(&location, error))
            return fail("prayer", 3, error);
    } else if (!location_config(config, city, &location, error)) {
        return fail("prayer", 3, error);
    }
    if (zone)
        snprintf(location.timezone, sizeof(location.timezone), "%s", zone);
    else if (location.timezone[0]) {
        if (!select_timezone(location.timezone))
            return fail("prayer", 3, "location contains an unknown IANA timezone");
    } else {
        system_timezone(location.timezone, sizeof(location.timezone));
        if (automatic)
            fputs("prayer: geome supplied no timezone; using system timezone (override with --timezone)\n", stderr);
    }
    Date today = local_date(now), hijri;
    if (today.year < 1600 || today.year > 2200 || !calendar_convert(today, 0, &hijri))
        return fail("prayer", 3, "prayer dates must be within 1600-2200 CE");
    Schedule schedules[SCHEDULE_DAYS];
    build_schedules(today, &location, method, schedules);
    Window next = next_prayer(schedules, now), current = current_prayer(schedules, now);
    if (json) {
        print_json(&location, method_name, schedules, hijri, next, current);
    } else {
        if (show_hijri) {
            char date[11];
            date_string(hijri, date);
            printf("%s AH (tabular)\n", date);
        }
        if (next_only || current_only) {
            print_window(next_only ? next : current, current_only);
        } else {
            char date[11], clock[6];
            date_string(today, date);
            printf("%s | %s | %s | %s\n", location.city, date, location.timezone, method_name);
            for (int index = 0; index < EVENT_COUNT; index++) {
                const Event *event = &schedules[TODAY].events[index];
                event_clock(event, clock);
                printf("%-8s %s", event_names[index], clock);
                if (event->available) {
                    event_date(event, date);
                    printf("  %s", date);
                }
                putchar('\n');
            }
        }
    }
    return output_status("prayer");
}