/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>
#include "app/dtmf.h"
#include "app/fm.h"
#include "bsp/dp32g030/gpio.h"
#include "dcs.h"
#include "driver/bk1080.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/status.h"
#include "ui/ui.h"

FUNCTION_Type_t gCurrentFunction;

void FUNCTION_Init(void)
{
	if (IS_NOT_NOAA_CHANNEL(gRxInfo->CHANNEL_SAVE)) {
		gCopyOfCodeType = gCodeType;
		if (g_20000381 == 0) {
			if (gRxInfo->IsAM) {
				gCopyOfCodeType = CODE_TYPE_OFF;
			} else {
				gCopyOfCodeType = gRxInfo->pCurrent->CodeType;
			}
		}
	} else {
		gCopyOfCodeType = CODE_TYPE_CONTINUOUS_TONE;
	}
	gDTMF_RequestPending = false;
	gDTMF_WriteIndex = 0;
	memset(gDTMF_Received, 0, sizeof(gDTMF_Received));
	g_CxCSS_TAIL_Found = false;
	g_CDCSS_Lost = false;
	g_CTCSS_Lost = false;
	g_VOX_Lost = false;
	g_SquelchLost = false;
	g_20000342 = 0;
	gSystickFlag10 = false;
	gFoundCTCSS = false;
	gFoundCDCSS = false;
	gFoundCTCSSCountdown = 0;
	gFoundCDCSSCountdown = 0;
	g_20000377 = 0;
	gSystickCountdown2 = 0;
}

void FUNCTION_Select(FUNCTION_Type_t Function)
{
	FUNCTION_Type_t PreviousFunction;
	bool bWasPowerSave;
	char *pString;
	char String[16]; // Can be overflown with the right EEPROM values
	uint16_t Delay;

	PreviousFunction = gCurrentFunction;
	bWasPowerSave = (PreviousFunction == FUNCTION_POWER_SAVE);
	gCurrentFunction = Function;

	if (bWasPowerSave) {
		if (Function != FUNCTION_POWER_SAVE) {
			BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable();
			gThisCanEnable_BK4819_Rxon = false;
			UI_DisplayStatus();
		}
	}

	if (Function == FUNCTION_0) {
		if (g_200003BE) {
			RADIO_Something();
		}
		if (PreviousFunction == FUNCTION_TRANSMIT) {
			gVFO_RSSI_Level[0] = 0;
			gVFO_RSSI_Level[1] = 0;
		} else if (PreviousFunction != FUNCTION_TRANSMIT) {
			gBatterySaveCountdown = 1000;
			gSchedulePowerSave = false;
			return;
		}
		if (gFmRadioMode) {
			g_2000038E = 500;
		}
		if (g_200003BC != 1 && g_200003BC != 2) {
			gBatterySaveCountdown = 1000;
			gSchedulePowerSave = false;
			return;
		}
		gBatterySaveCountdown = 1000;
		gSchedulePowerSave = false;
		gDTMF_AUTO_RESET_TIME = 1 + (gEeprom.DTMF_AUTO_RESET_TIME * 2);
		return;
	}

	if (Function == FUNCTION_MONITOR || Function == FUNCTION_3 || Function == FUNCTION_RECEIVE) {
		gBatterySaveCountdown = 1000;
		gSchedulePowerSave = false;
		g_2000038E = 0;
		return;
	}

	if (Function == FUNCTION_POWER_SAVE) {
		gBatterySave = gEeprom.BATTERY_SAVE * 10;
		gThisCanEnable_BK4819_Rxon = true;
		BK4819_DisableVox();
		BK4819_Sleep();
		BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2, false);
		gBatterySaveCountdownExpired = false;
		gUpdateStatus = true;
		GUI_SelectNextDisplay(DISPLAY_MAIN);
		return;
	}

	if (Function != FUNCTION_TRANSMIT) {
		return;
	}

	if (gFmRadioMode) {
		BK1080_Init(0, false);
	}

	if (g_20000383 == 1 && gEeprom.ALARM_MODE != ALARM_MODE_TONE) {
		g_20000383 = 2;
		GUI_DisplayScreen();
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		SYSTEM_DelayMs(20);
		BK4819_PlayTone(500, 0);
		SYSTEM_DelayMs(2);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		g_2000036B = 1;
		SYSTEM_DelayMs(60);
		BK4819_ExitTxMute();
		gBatterySaveCountdown = 1000;
		gSchedulePowerSave = false;
		g_2000038E = 0;
		g_20000420 = 0;
		return;
	}

	GUI_DisplayScreen();
	RADIO_PrepareTransmit();
	BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29, true);

	if (g_200003BE == 1) {
		if (g_20000438 == 2) {
			pString = gDTMF_String;
		} else {
			sprintf(String, "%s%c%s", gDTMF_String, gEeprom.DTMF_SEPARATE_CODE, gEeprom.ANI_DTMF_ID);
			pString = String;
		}
	} else if (g_200003BE == 2) {
		pString = "AB";
	} else {
		if (g_200003BE == 3) {
			sprintf(String, "%s%c%s", gEeprom.ANI_DTMF_ID, gEeprom.DTMF_SEPARATE_CODE, "AAAAA");
			pString = String;
		}
		if (g_200003BC || (gCrossTxRadioInfo->DTMF_PTT_ID_TX_MODE != 1 && gCrossTxRadioInfo->DTMF_PTT_ID_TX_MODE != 3)) {
			g_200003BE = 0;
			goto Skip;
		}
		pString = gEeprom.DTMF_UP_CODE;
	}
	g_200003BE = 0;
	Delay = gEeprom.DTMF_PRELOAD_TIME;
	if (gEeprom.DTMF_SIDE_TONE) {
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		g_2000036B = 1;
		Delay = gEeprom.DTMF_PRELOAD_TIME;
		if (gEeprom.DTMF_PRELOAD_TIME < 60) {
			Delay = 60;
		}
	}
	SYSTEM_DelayMs(Delay);

	BK4819_EnterDTMF_TX(gEeprom.DTMF_SIDE_TONE);

	BK4819_PlayDTMFString(
		pString,
		1,
		gEeprom.DTMF_FIRST_CODE_PERSIST_TIME,
		gEeprom.DTMF_HASH_CODE_PERSIST_TIME,
		gEeprom.DTMF_CODE_PERSIST_TIME,
		gEeprom.DTMF_CODE_INTERVAL_TIME);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	g_2000036B = 0;
	BK4819_ExitDTMF_TX(false);

Skip:
	if (g_20000383) {
		if (g_20000383 == 3) {
			BK4819_TransmitTone(true, 1750);
		} else {
			BK4819_TransmitTone(true, 500);
		}
		SYSTEM_DelayMs(2);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		g_20000420 = 0;
		g_2000038E = 0;
		g_2000036B = 1;
		gSchedulePowerSave = false;
		gBatterySaveCountdown = 1000;
		return;
	}
	if (gCrossTxRadioInfo->SCRAMBLING_TYPE && gSetting_ScrambleEnable) {
		BK4819_EnableScramble(gCrossTxRadioInfo->SCRAMBLING_TYPE - 1U);
		gBatterySaveCountdown = 1000;
		gSchedulePowerSave = false;
		g_2000038E = 0;
		return;
	}
	BK4819_DisableScramble();
	gBatterySaveCountdown = 1000;
	gSchedulePowerSave = false;
	g_2000038E = 0;
}

