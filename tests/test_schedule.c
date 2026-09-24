/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#include "schedule.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    Location location = {.latitude = 32.7765, .longitude = -79.9311};
    Schedule schedules[SCHEDULE_DAYS];
    build_schedules((Date){2026, 9, 23}, &location, CALC_MWL, schedules);
    const Event *today = schedules[TODAY].events;
    assert(current_prayer(schedules, today[FAJR].instant).name == FAJR);
    assert(current_prayer(schedules, today[SUNRISE].instant).name == -1);
    assert(next_prayer(schedules, today[FAJR].instant).name == DHUHR);
    assert(current_prayer(schedules, today[DHUHR].instant).name == DHUHR);
    assert(current_prayer(schedules, today[ASR].instant).name == ASR);
    assert(current_prayer(schedules, today[MAGHRIB].instant).name == MAGHRIB);
    assert(current_prayer(schedules, today[ISHA].instant).name == ISHA);
    Window next = next_prayer(schedules, today[ISHA].instant);
    assert(next.name == FAJR && next.start == &schedules[TODAY + 1].events[FAJR]);
    Window previous = current_prayer(schedules, today[FAJR].instant - 1);
    assert(previous.name == ISHA && previous.start == &schedules[TODAY - 1].events[ISHA]);
    puts("schedule boundary tests passed");
    return 0;
}