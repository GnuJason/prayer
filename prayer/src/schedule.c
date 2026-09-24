/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#define PRAYERTIMES_IMPLEMENTATION
#include "schedule.h"
#include <math.h>
#include <stdio.h>

const char *const event_names[EVENT_COUNT] = {"Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha"};
const char *const event_keys[EVENT_COUNT] = {"fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"};

void build_schedules(Date today, const Location *location, CalcMethod method,
                     Schedule schedules[SCHEDULE_DAYS])
{
    long base = mt_days_from_civil(today.year, today.month, today.day);
    const MethodParams *params = method_params_get(method);
    MethodParams horizon = *params;
    horizon.fajr_angle = REFRACTION_CORRECTION;
    horizon.ihtiyat = 0;
    horizon.high_lat_method = HIGHLAT_NONE;
    horizon.high_lat_ref = 0;
    for (int index = 0; index < SCHEDULE_DAYS; index++) {
        long day = base + index - TODAY;
        Date date;
        mt_civil_from_days(day, &date.year, &date.month, &date.day);
        schedules[index].date = date;
        struct PrayerTimes times = calculate_prayer_times(date.year, date.month, date.day,
            location->latitude, location->longitude, 0, params);
        struct PrayerTimes sunrise = calculate_prayer_times(date.year, date.month, date.day,
            location->latitude, location->longitude, 0, &horizon);
        double hours[EVENT_COUNT] = {times.fajr, sunrise.fajr, times.dhuhr,
                                    times.asr, times.maghrib, times.isha};
        for (int event = 0; event < EVENT_COUNT; event++) {
            Event *result = &schedules[index].events[event];
            result->available = isfinite(hours[event]);
            result->instant = result->available
                ? (time_t)((double)day * 86400 + ceil(hours[event] * 60) * 60) : 0;
        }
    }
}

Window next_prayer(const Schedule schedules[SCHEDULE_DAYS], time_t now)
{
    Window result = {.name = -1};
    for (int day = 0; day < SCHEDULE_DAYS; day++) {
        for (int name = 0; name < EVENT_COUNT; name++) {
            const Event *event = &schedules[day].events[name];
            if (name != SUNRISE && event->available && event->instant > now
                && (!result.start || event->instant < result.start->instant))
                result = (Window){name, event, NULL};
        }
    }
    return result;
}

Window current_prayer(const Schedule schedules[SCHEDULE_DAYS], time_t now)
{
    Window result = {.name = -1};
    for (int day = 0; day < SCHEDULE_DAYS; day++) {
        for (int name = 0; name < EVENT_COUNT; name++) {
            if (name == SUNRISE || (name == ISHA && day == SCHEDULE_DAYS - 1))
                continue;
            const Event *start = &schedules[day].events[name];
            const Event *end = name == ISHA ? &schedules[day + 1].events[FAJR]
                                           : &schedules[day].events[name + 1];
            if (start->available && end->available && start->instant <= now
                && now < end->instant
                && (!result.start || start->instant > result.start->instant))
                result = (Window){name, start, end};
        }
    }
    return result;
}

void event_clock(const Event *event, char output[6])
{
    struct tm local;
    if (!event || !event->available || !localtime_r(&event->instant, &local))
        snprintf(output, 6, "--:--");
    else
        strftime(output, 6, "%H:%M", &local);
}

void event_date(const Event *event, char output[11])
{
    date_string(local_date(event->instant), output);
}