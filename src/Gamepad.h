/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef GAMEPAD_H
#define GAMEPAD_H


#include <Messenger.h>
#include <OS.h>

#include <atomic>


class BJoystick;


class Gamepad {
public:
								Gamepad(const BMessenger& target);
								~Gamepad();

			void				Start();
			void				Stop();

private:
	static	int32				_Thread(void* data);
			void				_Run();
			bool				_Open(BJoystick& joystick);
			void				_SendKey(uint32 key);
			void				_SendFunctionKey(uint32 key);

			BMessenger			fTarget;
			thread_id			fThread;
			std::atomic<bool>	fQuit;
};


#endif	// GAMEPAD_H
