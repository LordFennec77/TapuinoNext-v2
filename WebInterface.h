#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <string>

void initWebInterface();
void resetWiFiCredentials();
String getWiFiStatusSummary();

extern volatile bool remotePlayRequested;
extern volatile bool remoteStopRequested;
extern volatile bool isTapPlaying;
extern volatile bool c64MotorActive;
extern volatile uint16_t tapCounterCurrent;
extern std::string remoteFilePath;

#endif
