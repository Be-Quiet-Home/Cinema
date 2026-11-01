/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Gamepad.h"

#include <InterfaceDefs.h>
#include <Joystick.h>
#include <Message.h>

#include <stdlib.h>
#include <vector>


static const bigtime_t kPollInterval = 25000;
static const bigtime_t kRepeatDelay = 400000;
static const bigtime_t kRepeatInterval = 120000;
static const bigtime_t kRetryDelay = 1000000;
static const int16 kDeadZone = 12000;


static int32
direction_from(const std::vector<uint8>& hats, const std::vector<int16>& axes)
{
	// A hat reports 1 as north, 3 as east, 5 as south and 7 as west.
	if (!hats.empty()) {
		switch (hats[0]) {
			case 1:
			case 2:
			case 8:
				return B_UP_ARROW;
			case 3:
				return B_RIGHT_ARROW;
			case 4:
			case 5:
			case 6:
				return B_DOWN_ARROW;
			case 7:
				return B_LEFT_ARROW;
		}
	}

	if (axes.size() < 2)
		return 0;

	int32 horizontal = 0;
	int32 vertical = 0;
	if (axes[0] < -kDeadZone)
		horizontal = B_LEFT_ARROW;
	else if (axes[0] > kDeadZone)
		horizontal = B_RIGHT_ARROW;
	if (axes[1] < -kDeadZone)
		vertical = B_UP_ARROW;
	else if (axes[1] > kDeadZone)
		vertical = B_DOWN_ARROW;

	if (horizontal != 0 && vertical != 0) {
		if (abs(axes[0]) > abs(axes[1]))
			vertical = 0;
		else
			horizontal = 0;
	}
	return horizontal != 0 ? horizontal : vertical;
}


Gamepad::Gamepad(const BMessenger& target)
	:
	fTarget(target),
	fThread(-1),
	fQuit(false)
{
}


Gamepad::~Gamepad()
{
	Stop();
}


void
Gamepad::Start()
{
	if (fThread >= 0)
		return;
	fQuit = false;
	fThread = spawn_thread(_Thread, "Cinema gamepad", B_NORMAL_PRIORITY, this);
	if (fThread >= 0)
		resume_thread(fThread);
}


void
Gamepad::Stop()
{
	fQuit = true;
	if (fThread < 0)
		return;
	status_t status;
	wait_for_thread(fThread, &status);
	fThread = -1;
}


int32
Gamepad::_Thread(void* data)
{
	static_cast<Gamepad*>(data)->_Run();
	return 0;
}


bool
Gamepad::_Open(BJoystick& joystick)
{
	// Older drivers may not know how to enumerate again, but the list from
	// the constructor is still usable.
	joystick.RescanDevices();

	int32 count = joystick.CountDevices();
	for (int32 i = 0; i < count; i++) {
		char name[B_OS_NAME_LENGTH];
		if (joystick.GetDeviceName(i, name, sizeof(name)) != B_OK)
			continue;
		// Open() returns the file descriptor on success, not B_OK.
		if (joystick.Open(name, true) >= 0
			|| joystick.Open(name, false) >= 0) {
			joystick.SetMaxLatency(0);
			return true;
		}
	}
	return false;
}


void
Gamepad::_SendKey(uint32 key)
{
	char bytes[2] = { (char)key, 0 };
	BMessage message(B_KEY_DOWN);
	message.AddInt32("key", key);
	message.AddInt32("raw_char", key);
	message.AddInt32("modifiers", 0);
	message.AddString("bytes", bytes);
	fTarget.SendMessage(&message);
}


void
Gamepad::_SendFunctionKey(uint32 key)
{
	char bytes[2] = { B_FUNCTION_KEY, 0 };
	BMessage message(B_KEY_DOWN);
	message.AddInt32("key", key);
	message.AddInt32("raw_char", 0);
	message.AddInt32("modifiers", 0);
	message.AddString("bytes", bytes);
	fTarget.SendMessage(&message);
}


void
Gamepad::_Run()
{
	BJoystick joystick;
	int32 direction = 0;
	bigtime_t repeatAt = 0;
	uint32 previousButtons = 0;
	bool opened = false;
	std::vector<int16> axes;
	std::vector<uint8> hats;

	while (!fQuit) {
		if (!opened) {
			if (!_Open(joystick)) {
				snooze(kRetryDelay);
				continue;
			}
			int32 axisCount = joystick.CountAxes();
			int32 hatCount = joystick.CountHats();
			axes.assign(axisCount > 0 ? axisCount : 0, 0);
			hats.assign(hatCount > 0 ? hatCount : 0, 0);
			direction = 0;
			previousButtons = 0;
			opened = true;
		}

		status_t updated = joystick.Update();
		if (updated == B_WOULD_BLOCK || updated == B_TIMED_OUT
			|| updated == B_INTERRUPTED) {
			// No new report from the device: this is normal while polling.
			snooze(kPollInterval);
			continue;
		}
		if (updated != B_OK) {
			joystick.Close();
			opened = false;
			direction = 0;
			previousButtons = 0;
			snooze(kRetryDelay);
			continue;
		}

		if (!axes.empty())
			joystick.GetAxisValues(axes.data(), 0);
		else
			axes.clear();
		if (!hats.empty())
			joystick.GetHatValues(hats.data(), 0);
		else
			hats.clear();

		int32 newDirection = direction_from(hats, axes);
		bigtime_t now = system_time();
		if (newDirection != direction) {
			if (newDirection != 0)
				_SendKey(newDirection);
			direction = newDirection;
			repeatAt = now + kRepeatDelay;
		} else if (direction != 0 && now >= repeatAt) {
			_SendKey(direction);
			repeatAt = now + kRepeatInterval;
		}

		uint32 buttons = joystick.ButtonValues();
		uint32 pressed = buttons & ~previousButtons;
		previousButtons = buttons;
		if ((pressed & 0x01) != 0)
			_SendKey(B_ENTER);
		if ((pressed & 0x02) != 0)
			_SendKey(B_ESCAPE);
		if ((pressed & 0x04) != 0)
			_SendKey('d');
		if ((pressed & 0x08) != 0)
			_SendKey('f');
		// L1, Select or Start (pads differ on the last two)
		if ((pressed & (0x10 | 0x40 | 0x80)) != 0)
			_SendFunctionKey(B_F6_KEY);

		snooze(kPollInterval);
	}

	joystick.Close();
}
