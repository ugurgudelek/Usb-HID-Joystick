#include "hidjoystickrptparser.h"



JoystickReportParser::JoystickReportParser(JoystickEvents *evt) :
	joyEvents(evt),
	oldHat(0xDE),
	oldButtons(0) {
	for (uint8_t i = 0; i < RPT_GEMEPAD_LEN; i++)
		oldPad[i] = 0xD;
}

void JoystickReportParser::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
	bool match = true;
	for (int i = 0; i< len; i++)
	{
		Serial.print(buf[i]); Serial.print(" ");
	}

	// Checking if there are changes in report since the method was last called
	for (uint8_t i = 0; i < RPT_GEMEPAD_LEN; i++)
		if (buf[i] != oldPad[i]) {
			match = false;
			break;
		}

	// Calling Game Pad event handler
	if (!match && joyEvents) {
		joyEvents->OnGamePadChanged((const GamePadEventData*)buf);

		for (uint8_t i = 0; i < RPT_GEMEPAD_LEN; i++) oldPad[i] = buf[i];
	}

	uint8_t hat = (buf[5] & 0x0F);

	// Calling Hat Switch event handler
	if (hat != oldHat && joyEvents) {
		joyEvents->OnHatSwitch(hat);
		oldHat = hat;
	}



	//buf[0]  : X
	//buf[1]  : Y
	//buf[2]  : Z
	//buf[3]  : Slider
	//buf[4]  : Rz
	//buf[5]  : 4bit hat : 4bit unknown
	//buf[6]  : [bt_7, bt_6, bt_5, bt_4, bt_3, bt_2, bt_1, bt_0]
	//buf[7]  : [x,    x,    bt_13,bt_12,bt_11,bt_10,bt_9, bt_8]



	uint16_t buttons = 0x0000;
	buttons |= (buf[7] << 8);
	buttons |= (buf[6]);
	uint16_t changes = (buttons ^ oldButtons);

	// Calling Button Event Handler for every button changed
	if (changes) {
		for (uint8_t i = 0; i < 0x0C; i++) {
			uint16_t mask = (0x0001 << i);

			if (((mask & changes) > 0) && joyEvents) {
				if ((buttons & mask) > 0)
					joyEvents->OnButtonDn(i + 1);
				else
					joyEvents->OnButtonUp(i + 1);
			}
		}
		oldButtons = buttons;

	}
}

void JoystickEvents::OnGamePadChanged(const GamePadEventData *evt) {

	Serial.print("X: ");
	PrintHex<uint8_t >(evt->X, 0x80);
	Serial.print("\tY: ");
	PrintHex<uint8_t >(evt->Y, 0x80);
	Serial.print("\tZ: ");
	PrintHex<uint8_t >(evt->Z, 0x80);
	Serial.print("\tSl: ");
	PrintHex<uint8_t >(evt->Slider, 0x80);
	Serial.print("\tRz: ");
	PrintHex<uint8_t >(evt->Rz, 0x80);
	Serial.println("");
}

void JoystickEvents::OnHatSwitch(uint8_t hat) {
	Serial.print("Hat Switch: ");
	PrintHex<uint8_t >(hat, 0x80);
	Serial.println("");
}

void JoystickEvents::OnButtonUp(uint8_t but_id) {
	Serial.print("Up: ");
	Serial.println(but_id, DEC);

}

void JoystickEvents::OnButtonDn(uint8_t but_id) {
	Serial.print("Dn: ");
	Serial.println(but_id, DEC);

}


