/* Copyright (c) 2012 KrossX <krossx@live.com>
 * License: http://www.opensource.org/licenses/mit-license.html  MIT License
 */

#include "General.h"
#include "playstation_codes.h"
#include "playstation_devices.h"
#include "Input.h"

#include "FileIO.h"

////////////////////////////////////////////////////////////////////////
// PlayStation Device
////////////////////////////////////////////////////////////////////////

PlayStationDevice::PlayStationDevice(_Settings &config, u16 bufferSize) :
	settings(config), sizeBuffer(bufferSize)
{
	gamepadPlugged = false;

	cmdBuffer = new u8[sizeBuffer];
	dataBuffer = new u8[sizeBuffer];
}

PlayStationDevice::~PlayStationDevice()
{
	delete cmdBuffer;
	delete dataBuffer;

	cmdBuffer = dataBuffer = NULL;
}

void PlayStationDevice::Recheck()
{
	gamepadPlugged = Input::Recheck(settings.xinputPort);
}

void PlayStationDevice::SetPortX(u8 p)
{
	port = p;
	DebugPrint("[%d] -> XInputPort [%d]", port, settings.xinputPort);
}

////////////////////////////////////////////////////////////////////////
// DualShock
////////////////////////////////////////////////////////////////////////

void DualShock::Reset()
{
	memset(dataBuffer, 0xFF, sizeBuffer);
	memset(cmdBuffer, 0x00, sizeBuffer);

	dataBuffer[2] = 0x5A;

	padDefault = settings.defaultAnalog ? (settings.greenAnalog ? ID_ANALOG_GREEN : ID_ANALOG_RED) : ID_DIGITAL;
	padID = padDefault;

	buttons = buttonsStick = 0xFFFF;
	analogL = analogR = 0x7F7F;
	triggerL = triggerR = 0x00;

	motorMapS = 0xFF;
	motorMapL = 0xFF;

	gamepadPlugged = false;
	bConfig = bModeLock = false;
}

DualShock::DualShock(_Settings &config, u16 bsize): PlayStationDevice(config, bsize)
{
	Reset();
	DebugPrint("XInputPort [%d]", settings.xinputPort);
}

u8 DualShock::command(const u32 counter, const u8 data)
{
	static bool loop = false;

	if(!gamepadPlugged)
	{
		if (counter == 0)
		{
			Recheck();
			loop = true;
		}

		if (!gamepadPlugged)
		{
			return loop? 0x00 : 0xFF;
		}
		else
		{ 
			padID = padDefault;
		}
	}

	loop = false;

	if (counter >= sizeBuffer)
	{
		printf("PJ64L_XInput: Out of Bound Buffer ERROR! [%02d:%02d]\n", sizeBuffer, counter);
		return 0x00;
	}

	cmdBuffer[counter] = data;

	switch(counter)
	{
	case 0x00: Cmd0(); break;

	case 0x01:
		dataBuffer[1] = bConfig? ID_CONFIG : padID;
		Cmd1(cmdBuffer[1]);
		break;

	case 0x02: dataBuffer[2] = 0x5A; break;

	case 0x04:Cmd4(cmdBuffer[1]); break;
	case 0x08:Cmd8(cmdBuffer[1]); break;
	}

	return dataBuffer[counter];
}

void DualShock::ReadInput(u8 *buffer)
{
	poll();

	if(padID == ID_DIGITAL)
	{
		buffer[3] = buttonsStick & 0xFF;
		buffer[4] = buttonsStick >> 8;
		memset(&buffer[5], 0xFF, 4);
	}
	else
	{
		buffer[3] = buttons & 0xFF;
		buffer[4] = buttons >> 8;

		buffer[5] = analogR & 0xFF;
		buffer[6] = analogR >> 8;
		buffer[7] = analogL & 0xFF;
		buffer[8] = analogL >> 8;
	}
}

void DualShock::SetVibration()
{
	motorMapS = motorMapL = 0xFF;

	for(u8 i = 3; i<9; i++)
	{
		if(cmdBuffer[i] == 0x00) motorMapS = i - 3;
		if(cmdBuffer[i] == 0x01) motorMapL = i - 3;
	}
}

extern u8 multitap;
bool bPressed[4] = {false};

void DualShock::Cmd0()
{
	if (!gamepadPlugged)
	{
		Input::SetAnalogLed(port, true);
	}
	else
	{
		if (!bModeLock)
		{
			bool analogPressed = Input::CheckAnalogToggle(settings.xinputPort);

			if (!bPressed[port] && analogPressed)
			{
				if (padID == ID_DIGITAL)
				{
					padID = settings.greenAnalog ? ID_ANALOG_GREEN : ID_ANALOG_RED;
					//Debug("PJ64L_XInput -> [%d] Switched to analog mode (%X).\n", port, padID);
				}
				else
				{
					padID = (u8)ID_DIGITAL;
					//Debug("PJ64L_XInput -> [%d] Switched to digital mode (%X).\n", port, padID);
				}

				bPressed[port] = true;
			}
			else if (bPressed[port] && !analogPressed)
			{
				bPressed[port] = false;
			}
		}

		Input::SetAnalogLed(port, padID == ID_DIGITAL);
	}
}

void DualShock::Cmd1(const u8 data)
{
	switch(data)
	{
	case 0x40: if(bConfig) memcpy(&dataBuffer[3], WTF, 6); // WTF ???!!!

	case 0x41: break;

	case 0x42: // Read_Data
		if(bConfig) memset(&dataBuffer[3], 0xFF, 6);
		else ReadInput(dataBuffer);
		break;

	case 0x43: // Config mode
		if(bConfig) memset(&dataBuffer[3], 0x00, 6);
		else ReadInput(dataBuffer);
		break;

	case 0x44: // Set mode and lock
		memset(&dataBuffer[3], 0x00, 6);
		break;

	case 0x45: if(bConfig) { // Query model, 5th means LED status
		memcpy(&dataBuffer[3], DUALSHOCK2_MODEL, 6); // Using DS2 Model for now
		dataBuffer[5] = padID == ID_DIGITAL? 0x00 : 0x01; }
		break;

	case 0x46:
	case 0x47:
		if(bConfig) dataBuffer[3] =0x00;
		break;

	case 0x48: break;
	case 0x49: break;
	case 0x4A: break;
	case 0x4B: break;

	case 0x4C:
		if(bConfig) dataBuffer[3] = 0x00;
		break;

	case 0x4D: if(bConfig) { // Sets which bytes will be for vibration on cmd42
		dataBuffer[3] = motorMapS;
		dataBuffer[4] = motorMapL;
		memset(&dataBuffer[5], 0xFF, 4);	}
		break;

	case 0x4E: break;
	case 0x4F: break;
	}
}

void DualShock::Cmd4(const u8 data)
{
	switch(data)
	{
	case 0x40: break;
	case 0x41: break;
	case 0x42: 
		if(padID == ID_DIGITAL)
		vibration(motorMapS == 0xFF ? 0 : cmdBuffer[motorMapS + 3],
			      motorMapL == 0xFF ? 0 : cmdBuffer[motorMapL + 3]);
		break;

	case 0x43: // Config mode
		bConfig = cmdBuffer[3] == 1;
		break;

	case 0x44: if(bConfig) {// Set mode and lock
		padID = cmdBuffer[3] == 0x01 ? ID_ANALOG_RED : ID_DIGITAL;
		bModeLock = cmdBuffer[4] == 0x03; } // Disgaea sends 0x01 here
		break;

	case 0x45: break;

	// Using DS2 ID code
	case 0x46: if(bConfig) {// Unknown constant part 1 and 2
		if(cmdBuffer[3] == 0x00) memcpy(&dataBuffer[4], DUALSHOCK2_ID[0], 5);
		else memcpy(&dataBuffer[4], DUALSHOCK2_ID[1], 5);}
		break;

	case 0x47: if(bConfig) {//Unknown constant part 3
		memcpy(&dataBuffer[4], DUALSHOCK2_ID[2], 5); }
		break;

	case 0x48: break;
	case 0x49: break;
	case 0x4A: break;
	case 0x4B: break;

	case 0x4C: if(bConfig) {// Unknown constant part 4 and 5
		if(cmdBuffer[3] == 0x00) memcpy(&dataBuffer[4], DUALSHOCK2_ID[3], 5);
		else memcpy(&dataBuffer[4], DUALSHOCK2_ID[4], 5);}
		break;

	case 0x4D: break;
	case 0x4E: break;
	case 0x4F: break;
	}
}

void DualShock::Cmd8(const u8 data)
{
	switch(data)
	{
	case 0x40: break;
	case 0x41: break;

	case 0x42:
		vibration(motorMapS == 0xFF? 0: cmdBuffer[motorMapS+3],
			      motorMapL == 0xFF? 0: cmdBuffer[motorMapL+3]);
		break;

	case 0x43: break;
	case 0x44: break;
	case 0x45: break;
	case 0x46: break;
	case 0x47: break;
	case 0x48: break;
	case 0x49: break;
	case 0x4A: break;
	case 0x4B: break;
	case 0x4C: break;

	case 0x4D:
		SetVibration();
		break;

	case 0x4E: break;
	case 0x4F: break;
	}
}

void DualShock::poll()
{
	u16 buffer[5];
	Input::DualshockPoll(buffer, settings, gamepadPlugged, padID == ID_DIGITAL);

	buttons = buffer[0];
	buttonsStick = buffer[1];
	analogL = buffer[2];
	analogR = buffer[3];
	triggerR = buffer[4] & 0xFF;
	triggerL = (buffer[4]>>8) & 0xFF;
}

void DualShock::vibration(u8 smalldata, u8 bigdata)
{
	Input::DualshockRumble(smalldata, bigdata, settings, gamepadPlugged);
}

void DualShock::SaveState(PlayStationDeviceState &state)
{
	state.buttons = buttons;
	state.buttonsStick = buttonsStick;
	state.analogL = analogL;
	state.analogR = analogR;
	state.padID = padID;
	state.bConfig = bConfig;
	state.bModeLock = bModeLock;
	state.motorMapS = motorMapS;
	state.motorMapL = motorMapL;
	state.triggerL = triggerL;
	state.triggerR = triggerR;
}

void DualShock::LoadState(PlayStationDeviceState state)
{
	buttons = state.buttons;
	buttonsStick = state.buttonsStick;
	analogL = state.analogL;
	analogR = state.analogR;
	padID = state.padID;
	bConfig = state.bConfig;
	bModeLock = state.bModeLock;
	motorMapS = state.motorMapS;
	motorMapL = state.motorMapL;
	triggerL = state.triggerL;
	triggerR = state.triggerR;
}
