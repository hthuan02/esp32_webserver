#pragma once

#include <Arduino.h>

namespace WifiConfig {

// Loads saved router credentials (or defaults) and retries until connected.
void connectSavedNetwork();

// Attempts a new network; saves it on success and restores the current one on failure.
bool changeNetwork(const String& ssid, const String& password);

// Clears saved router credentials and restarts to use the default network.
void resetSavedNetwork();

} // namespace WifiConfig
