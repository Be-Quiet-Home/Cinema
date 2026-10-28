/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef MESSAGES_H
#define MESSAGES_H


enum {
	kMsgSearch			= 'srch',	// "query", "generation"
	kMsgResults			= 'rslt',	// "generation", "id", "title", "duration"
	kMsgThumbnail		= 'thmb',	// "generation", "id", "path"
	kMsgSearchFailed	= 'srfl',	// "generation"
	kMsgPlay			= 'play',	// "id", "title"
	kMsgResolve			= 'rslv',	// "id", "title"
	kMsgStreamReady		= 'strm',	// "id", "title", "url"
	kMsgStreamFailed	= 'stfl',	// "id"
	kMsgCategory		= 'catg',	// "index"
	kMsgSearchField		= 'sfld',	// the search field was confirmed
	kMsgAnimate			= 'anim',
};


#endif	// MESSAGES_H
