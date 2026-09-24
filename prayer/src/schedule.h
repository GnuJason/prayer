/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#ifndef SUITE_SCHEDULE_H
#define SUITE_SCHEDULE_H

#include "location.h"
#include "util.h"
#include <prayertimes.h>

enum { FAJR, SUNRISE, DHUHR, ASR, MAGHRIB, ISHA, EVENT_COUNT };
enum { SCHEDULE_DAYS = 5, TODAY = 2 };

typedef struct {
    int available;
    time_t instant;
} Event;

typedef struct {
    Date date;
    Event events[EVENT_COUNT];
} Schedule;

typedef struct {
    int name;
    const Event *start;
    const Event *end;
} Window;

extern const char *const event_names[EVENT_COUNT];
extern const char *const event_keys[EVENT_COUNT];
void build_schedules(Date today, const Location *location, CalcMethod method,
                     Schedule schedules[SCHEDULE_DAYS]);
Window next_prayer(const Schedule schedules[SCHEDULE_DAYS], time_t now);
Window current_prayer(const Schedule schedules[SCHEDULE_DAYS], time_t now);
void event_clock(const Event *event, char output[6]);
void event_date(const Event *event, char output[11]);

#endif