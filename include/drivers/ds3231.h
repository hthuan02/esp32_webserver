#pragma once

#include <time.h>

namespace Ds3231 {

// Starts the external DS3231 on SDA GPIO17 and SCL GPIO16.
bool begin();

// Reads and writes Unix time in UTC. Returns false when RTC time is invalid.
bool readUnixTime(time_t& epoch);
bool writeUnixTime(time_t epoch);

} // namespace Ds3231
