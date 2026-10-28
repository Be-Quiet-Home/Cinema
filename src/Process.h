/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef PROCESS_H
#define PROCESS_H


#include <String.h>

#include <vector>


/*!	Runs a program (found in PATH, no shell, so arguments need no quoting) and
	returns its exit status, or -1 if it could not be run. Its standard output
	goes to \a output when given.
*/
int		RunProgram(const std::vector<BString>& arguments,
			BString* output = NULL);


#endif	// PROCESS_H
