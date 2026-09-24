/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#define HIJRI_IMPLEMENTATION
#include <hijri.h>
#define PRAYERTIMES_IMPLEMENTATION
#include <prayertimes.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    double julian = hijri_jd_from_gregorian(2024, 3, 11);
    HijriDate date = hijri_tabular_from_jd(julian);
    assert(date.year == 1445 && date.month == 9 && date.day == 1);
    assert(hijri_tabular_to_jd(date) == julian);
    struct PrayerTimes times = calculate_prayer_times(
        2025, 11, 21, -6.2851291, 106.9814968, 7.0,
        method_params_get(CALC_KEMENAG));
    assert(fabs(times.fajr * 60.0 - 245.0) < 2.0);
    times = calculate_prayer_times(2026, 1, 15, 51.5074, -0.1278, 0,
                                   method_params_get(CALC_MWL));
    assert(fabs(ceil(times.fajr * 60) - 359) <= 2);
    assert(fabs(ceil(times.dhuhr * 60) - 730) <= 2);
    assert(fabs(ceil(times.asr * 60) - 840) <= 2);
    assert(fabs(ceil(times.maghrib * 60) - 981) <= 2);
    assert(fabs(ceil(times.isha * 60) - 1095) <= 2);
    puts("libmuslim API and reference checks passed");
    return 0;
}