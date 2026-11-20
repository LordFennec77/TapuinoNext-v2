#ifdef ESP32

#include <iostream>
#include <string>
#include <vector>

#include "FS.h"
#include "Lang.h"
#include "LoadSelector.h"
#include "Menu.h"
#include "Options.h"
#include "RecordSelector.h"
#include "SD_MMC.h"
#include "config.h"
#include "OptionEventHandler.h"
#include "WebInterface.h"
#include <WiFi.h>

using namespace std;
using namespace TapuinoNext;

#include "ESP32FileLoader.h"
#include "ESP32InputHandler.h"
#include "ESP32TapLoader.h"
#include "ESP32TapeCartLoader.h"
#include "ESP32TapRecorder.h"
#include "FS.h"
#include "LCD_HD44780.h"
#include "OLED_1306.h"
#include "Updater.h"

#include "Version.h"

OptionEventHandler optionEventHander;

ESP32FileLoader theFileLoader;

#if I2C_DISP_TYPE == I2C_DISP_HD44780
LCD_HD44780 theLCD(&optionEventHander);
#else
OLED_1306 theLCD(&optionEventHander);
#endif

ESP32InputHandler theInputHandler(&optionEventHander);
LCDUtils lcdUtils(&theLCD);

bool initTapuino()
{
    Serial.begin(115200);
    Serial.println(S_INIT);
    theLCD.Init(I2C_DISP_ADDR);

    lcdUtils.Title("TapuinoNext");
    Serial.println("TapuinoNext");

    theInputHandler.Init();

    char version[I2C_DISP_COLS + 1];
    memset(version, 0, I2C_DISP_COLS + 1);
    snprintf(version, I2C_DISP_COLS + 1, "%s", FW_VERSION);
    lcdUtils.Status(version);
    Serial.println(version);
    delay(2000);

    lcdUtils.Status("Avvio WiFi...");
    initWebInterface();
    String ipStr = getWiFiStatusSummary();
    lcdUtils.Status(ipStr.c_str());
    delay(3000);

    lcdUtils.Status(S_INIT);
    Serial.println("Starting SD init");

    if (!theFileLoader.Init())
    {
        Serial.println(S_INIT_FAILED);
        lcdUtils.Status(S_INIT_FAILED);
        delay(2000);
        return false;
    }
    Serial.println(S_INIT_OK);
    lcdUtils.Status(S_INIT_OK);
    delay(1000);

    return (true);
}

MenuEntry mainMenuEntries[] = {
    {MenuEntryType::IndexEntry, S_INSERT_TAP, NULL},
    {MenuEntryType::IndexEntry, S_RECORD_TAP, NULL},
    {MenuEntryType::IndexEntry, S_OPTIONS, NULL},
};

TheMenu mainMenu = {S_MAIN_MENU, (MenuEntry*) mainMenuEntries, 3, 0, NULL};

void setup()
{
    if (!initTapuino())
        return;

    MenuHandler menu(&lcdUtils, &theInputHandler);
    Updater updater(&lcdUtils, &theInputHandler, &theFileLoader);

    Options options(&theFileLoader, &optionEventHander, &menu, &updater);

    FlipBuffer flipBuffer(4096);
    ErrorCodes ret = flipBuffer.Init();
    if (ret != ErrorCodes::OK)
    {
        lcdUtils.Error(S_OUT_OF_MEMORY, ret);
        while (1) {}
    }

    UtilityCollection utilityCollection(&lcdUtils, &theInputHandler, &theFileLoader, &options, &flipBuffer);
    LoadSelector lsel(&utilityCollection);
    RecordSelector rsel(&utilityCollection);
    options.LoadOptions();

    while (true)
    {
        if (remotePlayRequested)
        {
            remotePlayRequested = false;
            File tmpFile;
            ErrorCodes err = theFileLoader.OpenFile(remoteFilePath.c_str(), tmpFile);
            if (err == ErrorCodes::OK && !tmpFile.isDirectory())
            {
                String lowerPath = String(remoteFilePath.c_str());
                lowerPath.toLowerCase();
                isTapPlaying = true;
                if (lowerPath.endsWith(".prg"))
                {
                    ESP32TapeCartLoader prgLoader(&utilityCollection, tmpFile);
                    prgLoader.Init();
                    prgLoader.LoadPRG();
                }
                else
                {
                    ESP32TapLoader tl(&utilityCollection);
                    tl.PlayTap(tmpFile);
                }
                tmpFile.close();
                isTapPlaying = false;
            }
            else
            {
                lcdUtils.Error(S_FILE_ERROR, err);
            }
            continue;
        }

        int index = menu.Display(&mainMenu);
        if (index == -2 || remotePlayRequested)
        {
            continue;
        }

        switch (index)
        {
            case 0:
            {
                lsel.OnAction();
                break;
            }
            case 1:
            {
                rsel.OnAction();
                break;
            }
            case 2:
            {
                options.OnAction();
                break;
            }
        }
    }
}

void loop()
{
}

#endif
