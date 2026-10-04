#pragma once
#include <Arduino.h>
namespace spauth {
String loadRefreshToken();
void saveRefreshToken(const String& token);
bool runSetupPortalIfNeeded();   // blocks in loop until token obtained
}
