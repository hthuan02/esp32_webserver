#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace WifiConfig {

using AuthenticationCheck = bool (*)();

// Loads saved router credentials (or defaults) and retries until connected.
void connectSavedNetwork();

// Attempts a new network; saves it on success and restores the current one on failure.
bool changeNetwork(const String& ssid, const String& password);

// Clears saved router credentials and restarts to use the default network.
void resetSavedNetwork();

// Registers authenticated Wi-Fi scan and saved-network routes.
void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated);

} // namespace WifiConfig
