/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef MESSAGES_H
#define MESSAGES_H


enum {
	// from the helper
	kMsgResults			= 'rslt',	// "generation", "id", "title", "duration"
	kMsgSearchFailed	= 'srfl',	// "generation"
	kMsgFound			= 'fnd ',	// "id", "url"
	kMsgLost			= 'lost',	// "id"
	kMsgHelperGone		= 'gone',

	// to and from the worker
	kMsgThumbnails		= 'thms',	// "generation", "id"...
	kMsgThumbnail		= 'thmb',	// "generation", "id", "path"
	kMsgFollow			= 'folw',	// "id", "url"
	kMsgStreamReady		= 'strm',	// "id", "url"

	// in the window
	kMsgPlay			= 'play',	// "id", "title"
	kMsgSelected		= 'slct',	// "id": chosen, not played yet
	kMsgPrefetch		= 'pref',	// "id"
	kMsgCategory		= 'catg',	// "index"
	kMsgSearchField		= 'sfld',	// the search field was confirmed
	kMsgAnimate			= 'anim',
};


#endif	// MESSAGES_H
