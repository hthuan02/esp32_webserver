#include "drivers/ds3231.h"

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <sys/time.h>

namespace Ds3231 {
namespace {
constexpr int kSdaPin = 17;
constexpr int kSclPin = 16;
constexpr time_t kMinimumValidEpoch = 1704067200; // 2024-01-01 UTC

RTC_DS3231 rtc;
bool rtcFound = false;

bool isValid(const DateTime& value)
{
    return value.isValid() && value.year() >= 2024;
}
} // namespace

bool begin()
{
    Wire.begin(kSdaPin, kSclPin); // SDA=GPIO17, SCL=GPIO16
    Wire.setClock(100000);

    rtcFound = rtc.begin(&Wire);
    if (!rtcFound)
    {
        Serial.println("DS3231 khong tim thay. Kiem tra SDA GPIO17 va SCL GPIO16.");
        return false;
    }

    const DateTime now = rtc.now();
    Serial.printf("DS3231: %04u-%02u-%02u %02u:%02u:%02u\n",
                  static_cast<unsigned>(now.year()),
                  static_cast<unsigned>(now.month()),
                  static_cast<unsigned>(now.day()),
                  static_cast<unsigned>(now.hour()),
                  static_cast<unsigned>(now.minute()),
                  static_cast<unsigned>(now.second()));

    if (rtc.lostPower() || !isValid(now))
    {
        Serial.println("DS3231 chua co thoi gian hop le; se cho dong bo NTP.");
        return false;
    }

    const time_t epoch = static_cast<time_t>(now.unixtime());
    if (epoch < kMinimumValidEpoch)
    {
        return false;
    }

    const struct timeval systemTime = {epoch, 0};
    if (settimeofday(&systemTime, nullptr) != 0)
    {
        Serial.println("Khong the khoi phuc gio he thong tu DS3231.");
        return false;
    }

    Serial.println("Da khoi phuc gio tu RTC DS3231.");
    return true;
}

bool readUnixTime(time_t& epoch)
{
    if (!rtcFound || rtc.lostPower())
    {
        return false;
    }

    const DateTime now = rtc.now();
    if (!isValid(now))
    {
        return false;
    }

    epoch = static_cast<time_t>(now.unixtime());
    return epoch >= kMinimumValidEpoch;
}

bool writeUnixTime(time_t epoch)
{
    if (!rtcFound || epoch < kMinimumValidEpoch)
    {
        return false;
    }

    const DateTime value(static_cast<uint32_t>(epoch));
    if (!isValid(value))
    {
        return false;
    }

    rtc.adjust(value);
    Serial.println("Da dong bo thoi gian NTP xuong DS3231.");
    return true;
}

} // namespace Ds3231
