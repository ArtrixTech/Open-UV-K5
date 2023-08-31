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
#include "app/fm.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/fmradio.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"
#include "helper/battery.h"
void UI_DisplayFM(void)
{
	uint8_t i;
	char String[16];

	memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

	memset(String, 0, sizeof(String));
	strcpy(String, "FM-RADIO");

	UI_PrintString(String, 8, 127, 0, 8, false);
	memset(String, 0, sizeof(String));

	if (gAskToSave) {
		strcpy(String, "SAVE?");
	} else if (gAskToDelete) {
		strcpy(String, "DEL?");
	} else {
		if (gFM_Step == 0) {
			if (!gEeprom.FM_IsMrMode) {
				for (i = 0; i < 20; i++) {
					if (gEeprom.FM_FrequencyPlaying == gFM_Channels[i]) {
						sprintf(String, "Freq(CH%02d)", i + 1);
						break;
					}
				}
				if (i == 20) {
					strcpy(String, "FREQ");
				}
			} else {
				sprintf(String, "CH%02d", gEeprom.FM_SelectedChannel + 1);
			}
		} else {
			if (!gFM_AutoScan) {
				strcpy(String, "MANUAL-SCAN");
			} else {
				sprintf(String, "AUTO-SCAN(%d)", gFM_ChannelPosition + 1);
			}
		}
	}

	UI_PrintString(String, 8, 127, 2, 8, false);
	memset(String, 0, sizeof(String));

	if (gAskToSave || (gEeprom.FM_IsMrMode && gInputBoxIndex)) {
		UI_GenerateChannelString(String, gFM_ChannelPosition);
	} else if (!gAskToDelete) {
		if (gInputBoxIndex == 0) {
			NUMBER_ToDigits(gEeprom.FM_FrequencyPlaying * 10000, String);
			UI_DisplayFrequency(String, 8, 4, true, true);
		} else {
			UI_DisplayFrequency(gInputBox, 8, 4, true, false);
		}
		ST7565_BlitFullScreen();
		return;
	} else {
		sprintf(String, "CH-%02d", gEeprom.FM_SelectedChannel + 1);
	}

	UI_PrintString(String, 8, 127, 4, 10, false);
	ST7565_BlitFullScreen();
}

