#pragma once
#include <Arduino.h>
namespace spauth {
String loadRefreshToken();
bool runSetupPortalIfNeeded();   // blocks in loop until token obtained
}
