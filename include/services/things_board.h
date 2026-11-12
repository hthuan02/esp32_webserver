#pragma once

#include <WebServer.h>

namespace ThingsBoard {

using AuthenticationCheck = bool (*)();

void begin();
void loop();
void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated);

} // namespace ThingsBoard
