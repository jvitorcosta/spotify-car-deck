#pragma once
#include <Arduino.h>
namespace net {
bool connectAny();
bool isOnline();
void loop();
String deviceIp();
}
