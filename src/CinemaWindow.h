/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CINEMA_WINDOW_H
#define CINEMA_WINDOW_H


#include <String.h>
#include <Window.h>

#include <map>
#include <set>
#include <vector>

#include "Categories.h"


class BMessageRunner;
class BTextControl;
class CategoryBar;
class Helper;
class VideoGrid;
class Worker;


class CinemaWindow : public BWindow {
public:
								CinemaWindow();
	virtual						~CinemaWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
	struct Stream {
		BString		url;
		bigtime_t	found;
	};

			void				_Search(const char* query);
			void				_ShowResults(BMessage* message);
			bool				_Ready(const BString& id) const;
			void				_Find(const BString& id);
			void				_PrefetchWanted();
			void				_Play(const char* url, const char* title);

			std::vector<Category> fCategories;
			CategoryBar*		fCategoryBar;
			BTextControl*		fSearchField;
			VideoGrid*			fGrid;
			Helper*				fHelper;
			Worker*				fWorker;
			int32				fGeneration;

			std::map<BString, Stream> fStreams;	// the files found, by id
			std::set<BString>	fFinding;		// asked to the helper
			BString				fWaiting;		// clicked, not found yet
			BString				fWaitingTitle;
			BString				fWanted;		// chosen: worth finding
			BMessageRunner*		fPrefetchRunner;
};


#endif	// CINEMA_WINDOW_H
