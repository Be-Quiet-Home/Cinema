/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef DELIVER_H
#define DELIVER_H


#include <Messenger.h>

#include <atomic>


/*!	Sends a message from a thread of its own to a window, a little at a
	time: the window may be waiting for that thread, to close a video say,
	while its queue is full; the thread then gives up when stop is set,
	rather than both waiting for ever. B_OK when it was sent.
*/
status_t	Deliver(const BMessenger& target, BMessage* message,
				const std::atomic<bool>* stop);
status_t	Deliver(const BMessenger& target, uint32 what,
				const std::atomic<bool>* stop);


#endif	// DELIVER_H
