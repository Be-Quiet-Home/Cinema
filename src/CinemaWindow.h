/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CINEMA_WINDOW_H
#define CINEMA_WINDOW_H


#include <Window.h>

#include <vector>

#include "Categories.h"


class BTextControl;
class CategoryBar;
class VideoGrid;
class Worker;


class CinemaWindow : public BWindow {
public:
								CinemaWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_Search(const char* query);
			void				_ShowResults(BMessage* message);
			void				_Play(const char* url, const char* title);

			std::vector<Category> fCategories;
			CategoryBar*		fCategoryBar;
			BTextControl*		fSearchField;
			VideoGrid*			fGrid;
			Worker*				fSearcher;
			Worker*				fResolver;
			int32				fGeneration;
};


#endif	// CINEMA_WINDOW_H
