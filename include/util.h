//
// Created by turker on 18.08.2026.
//

#ifndef REDIS_CLONE_UTIL_H
#define REDIS_CLONE_UTIL_H

#include "data_types/redis_string.h"

int sdsmatchlen(sds pattern, sds string, int nocase);

#endif //REDIS_CLONE_UTIL_H
