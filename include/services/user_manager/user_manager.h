#pragma once

#include <Arduino.h>

namespace UserManager {

bool authenticate(const String& username, const String& password);

} // namespace UserManager
