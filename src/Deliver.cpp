/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "Deliver.h"

#include <Message.h>


status_t
Deliver(const BMessenger& target, BMessage* message,
	const std::atomic<bool>* stop)
{
	while (true) {
		status_t status = target.SendMessage(message, (BHandler*)NULL,
			200000);
		if (status != B_TIMED_OUT && status != B_WOULD_BLOCK)
			return status;
		if (stop != NULL && *stop)
			return status;
	}
}


status_t
Deliver(const BMessenger& target, uint32 what, const std::atomic<bool>* stop)
{
	BMessage message(what);
	return Deliver(target, &message, stop);
}
