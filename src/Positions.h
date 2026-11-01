/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef POSITIONS_H
#define POSITIONS_H


#include <OS.h>
#include <String.h>


/*!	Where each video was left, to go on from there, and whether it was
	watched: kept in the settings (Cinema/positions), one line per video,
	the most recent first. The library will read them too, for what was
	watched and what comes next.
*/
namespace Positions {

struct Position {
	bigtime_t	position;	// where it was left
	bigtime_t	duration;	// as long as it is, 0 if not known
	bool		watched;	// played to (nearly) the end
	time_t		when;		// when it was left

				Position() : position(0), duration(0), watched(false),
					when(0) {}
};

Position	Get(const char* id);
void		Set(const char* id, bigtime_t position, bigtime_t duration);
				// marks it watched when it is near the end
bigtime_t	ResumeAt(const char* id);
				// where to start again, or 0: not at the very beginning
				// nor at the very end

}	// namespace Positions


#endif	// POSITIONS_H
