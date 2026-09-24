/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#include "util.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>

static void usage(void)
{
    puts("Usage: hijri [--today | --from-gregorian DATE | --to-gregorian DATE] [--json]\n"
         "  --today                 Today's date using the system timezone (default)\n"
         "  --from-gregorian DATE    Gregorian to tabular Hijri\n"
         "  --to-gregorian DATE      Tabular Hijri to Gregorian\n"
         "  --json                  Nested input/output calendar and date objects\n"
         "  --help                  Show this help\n"
         "  --version               Show version\n"
         "Dates must be YYYY-MM-DD with years 0001-9999 in both calendars.\n"
         "Gregorian dates are proleptic Gregorian; dates before 0001 AH fail.\n"
         "Uses libmuslim's civil tabular calendar, not sighting or Umm al-Qura.\n"
         "Dates change at civil midnight and may differ from local observance.\n"
         "Examples:\n"
         "  hijri --from-gregorian 2024-03-11\n"
         "  hijri --to-gregorian 1445-09-01 --json\n"
         "Exit status: 0 success, 2 usage/invalid date, 3 I/O or clock failure.");
}

int main(int argc, char **argv)
{
    enum { OPT_TODAY = 256, OPT_FROM, OPT_TO, OPT_JSON, OPT_HELP, OPT_VERSION };
    static const struct option options[] = {
        {"today", no_argument, NULL, OPT_TODAY},
        {"from-gregorian", required_argument, NULL, OPT_FROM},
        {"to-gregorian", required_argument, NULL, OPT_TO},
        {"json", no_argument, NULL, OPT_JSON},
        {"help", no_argument, NULL, OPT_HELP},
        {"version", no_argument, NULL, OPT_VERSION},
        {NULL, 0, NULL, 0}
    };
    const char *text = NULL;
    int mode = 0, json = 0, help = 0, version = 0, option;
    signal(SIGPIPE, SIG_IGN);
    opterr = 0;
    while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
        switch (option) {
        case OPT_TODAY:
        case OPT_FROM:
        case OPT_TO:
            if (mode)
                return fail("hijri", 2, "choose exactly one conversion mode");
            mode = option;
            text = optarg;
            break;
        case OPT_JSON: json = 1; break;
        case OPT_HELP: help = 1; break;
        case OPT_VERSION: version = 1; break;
        default: return fail("hijri", 2, "unknown option or missing argument; see --help");
        }
    }
    if (optind != argc)
        return fail("hijri", 2, "unexpected positional argument; see --help");
    if (help) { usage(); return output_status("hijri"); }
    if (version) { puts("hijri " SUITE_VERSION); return output_status("hijri"); }
    Date input, output;
    if (text) {
        if (!parse_date(text, &input))
            return fail("hijri", 2, "invalid date format; expected YYYY-MM-DD");
    } else {
        time_t now = time(NULL);
        input = local_date(now);
        if (now == (time_t)-1 || !gregorian_valid(input))
            return fail("hijri", 3, "cannot determine today's date");
    }
    int reverse = mode == OPT_TO;
    if (!calendar_convert(input, reverse, &output))
        return fail("hijri", 2, "invalid date or conversion outside years 0001-9999");
    char input_text[11], output_text[11];
    date_string(input, input_text);
    date_string(output, output_text);
    if (json) {
        printf("{\"input\":{\"calendar\":\"%s\",\"date\":\"%s\"},"
               "\"output\":{\"calendar\":\"%s\",\"date\":\"%s\"}}\n",
               reverse ? "hijri" : "gregorian", input_text,
               reverse ? "gregorian" : "hijri", output_text);
    } else {
        printf("%s %s\n", output_text, reverse ? "CE" : "AH");
    }
    return output_status("hijri");
}