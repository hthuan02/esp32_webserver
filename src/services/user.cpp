#include "services/user.h"

namespace UserManager {
namespace {

// constexpr: giá trị cố định từ lúc biên dịch
constexpr char kLoginUsername[] = "esp32";
constexpr char kLoginPassword[] = "1234";
} // namespace

bool authenticate(const String& username, const String& password)
{
    return username == kLoginUsername && password == kLoginPassword;
}

} // namespace UserManager
