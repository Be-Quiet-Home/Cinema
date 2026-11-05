/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef WATCHDOG_H
#define WATCHDOG_H


#include <Messenger.h>
#include <SupportDefs.h>


/*!	Watches the window: when one message holds it more than three seconds,
	or it takes none of the pings sent every second for four (blocked
	outside a message, on its lock say), what every thread of Cinema is
	doing then goes to freeze-reports.txt in the cache (and to the standard
	error), so that a hang says where it is.
*/
enum { kMsgWatchdogPing = 'wdpg' };
namespace Watchdog {

void	Start(const char* folder, const BMessenger& window);
void	Stop();

// around each message of the window
void	Enter(uint32 what);
void	Leave();

// what was held before, for a message handled within another one (an
// update while a handler waits, say)
void	Held(uint32* what, bigtime_t* since);
void	Restore(uint32 what, bigtime_t since);

class Scope {
public:
	Scope(uint32 what) { Held(&fWhat, &fSince); Enter(what); }
	~Scope() { Restore(fWhat, fSince); }
private:
	uint32		fWhat;
	bigtime_t	fSince;
};

}


#endif	// WATCHDOG_H
