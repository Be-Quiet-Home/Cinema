/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CATEGORY_BAR_H
#define CATEGORY_BAR_H


#include <View.h>

#include <vector>

#include "Categories.h"


/*!	The rows chosen by the parents, as big buttons in the top bar; a click
	sends kMsgCategory with its "index" to the window.
*/
class CategoryBar : public BView {
public:
								CategoryBar(
									const std::vector<Category>& categories);

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	BSize				MinSize();
	virtual	BSize				PreferredSize();

			void				SetSelected(int32 index);
			int32				Selected() const { return fSelected; }

private:
			BRect				_ButtonFrame(int32 index) const;

			std::vector<Category> fCategories;
			int32				fSelected;	// -1: a search of one's own
};


#endif	// CATEGORY_BAR_H
