//
// Created by turker on 18.08.2026.
//

#include "include/util.h"

#include <ctype.h>
#include <stddef.h>

int isinrange(char a, char b, char o) {
    if (a >= b && a <= o) {
        return 1;
    }
    return 0;
}

int sdsmatchlen(sds pattern, sds string, int nocase) {
    sds endpattern = pattern + sdslen(pattern);
    sds endstring = string + sdslen(string);

    sds star_idx = NULL;
    sds match_idx = NULL;

    while (string < endstring) {

        if (pattern < endpattern && *pattern == '*') {
            star_idx = pattern;
            match_idx = string;
            pattern++;
        }

        else if (pattern < endpattern &&
                (*pattern == '?' ||
                (nocase ? tolower(*pattern) == tolower(*string) : *pattern == *string))) {
            pattern++;
            string++;
        }

        else if (pattern < endpattern && *pattern == '[') {
            sds p_temp = pattern + 1;
            int invert = (p_temp < endpattern && *p_temp == '^');
            if (invert) p_temp++;

            int match_found = 0;

            while (p_temp < endpattern && *p_temp != ']') {
                if (p_temp + 2 < endpattern && p_temp[1] == '-') {
                    if (isinrange(p_temp[0], p_temp[2], *string)) match_found = 1;
                    p_temp += 3;
                } else {
                    if (*p_temp == *string) match_found = 1;
                    p_temp++;
                }
            }

            if (invert) match_found = !match_found;

            if (match_found && p_temp < endpattern && *p_temp == ']') {
                pattern = p_temp + 1;
                string++;
            }

            else if (star_idx != NULL) {
                pattern = star_idx + 1;
                match_idx++;
                string = match_idx;
            }

            else {
                return 0;
            }
        }

        else if (star_idx != NULL) {
            pattern = star_idx + 1;
            match_idx++;
            string = match_idx;
        }

        else {
            return 0;
        }
    }

    while (pattern < endpattern && *pattern == '*') {
        pattern++;
    }

    return pattern == endpattern;
}