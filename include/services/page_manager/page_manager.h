#pragma once

#include <WebServer.h>

namespace PageManager {

using AuthenticationCheck = bool (*)();

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated);

} // namespace PageManager
