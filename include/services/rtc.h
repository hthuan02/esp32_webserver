#pragma once

#include <WebServer.h>

namespace RtcManager {

using AuthenticationCheck = bool (*)();

void begin();
void loop();
void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated);

} // namespace RtcManager
