/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef MESSAGES_H
#define MESSAGES_H


enum {
	// from the helper
	kMsgProviders		= 'prvs',	// "provider" messages: "id", "name",
									// "kinds"
	kMsgResults			= 'rslt',	// "generation", then for each video
									// "provider", "kind", "id", "title",
									// "description", "channel", "channel id",
									// "duration", "year", "rating", "poster",
									// "thumbnail"
	kMsgSearchFailed	= 'srfl',	// "generation"
	kMsgFound			= 'fnd ',	// "id", "url"
	kMsgLost			= 'lost',	// "id"
	kMsgHelperGone		= 'gone',
	kMsgDownloadProgress = 'dwpr',	// "id", "percent"
	kMsgDownloaded		= 'dwok',	// "id", "path"
	kMsgDownloadFailed	= 'dwno',	// "id"

	// to and from the worker
	kMsgThumbnails		= 'thms',	// "generation", "id"...
	kMsgThumbnail		= 'thmb',	// "generation", "id", "path"
	kMsgFollow			= 'folw',	// "id", "url"
	kMsgStreamReady		= 'strm',	// "id", "url"

	// in the window
	kMsgPlay			= 'play',	// "id", "title", "path" if downloaded
	kMsgDownload		= 'dwnl',	// "provider", "kind", "id", "title",
									// "channel", "channel id", "duration"
	kMsgShowDownloads	= 'dwsh',
	kMsgTrash			= 'trsh',	// "id", "path": a download to the Trash
	kMsgSelected		= 'slct',	// "id": chosen, not played yet
	kMsgPrefetch		= 'pref',	// "id"
	kMsgCategory		= 'catg',	// "index"
	kMsgChannel			= 'chan',	// "channel", "channel id"
	kMsgAddChannel		= 'addc',	// the channel shown, to the tabs
	kMsgRemoveCategory	= 'rmct',	// "index"
	kMsgSearchField		= 'sfld',	// the search field was confirmed
	kMsgAnimate			= 'anim',
	kMsgFullScreen		= 'fuls',	// F: full screen, or back
};


#endif	// MESSAGES_H
