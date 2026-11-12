#pragma once

#include <WebServer.h>

namespace OtaManager {

using AuthenticationCheck = bool (*)();

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated);

} // namespace OtaManager
