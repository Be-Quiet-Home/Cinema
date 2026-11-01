/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CATEGORY_BAR_H
#define CATEGORY_BAR_H


#include <View.h>

#include <vector>

#include "Categories.h"


/*!	The tabs of the top bar; a click sends kMsgCategory with its "index" to
	the window, the menu of a right click kMsgRemoveCategory. After them comes
	the tab of the downloads (kMsgShowDownloads), then the channel being looked
	at, if any, with a button that sends kMsgAddChannel.
*/
class CategoryBar : public BView {
public:
								CategoryBar(
									const std::vector<Category>& categories);

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	BSize				MinSize();
	virtual	BSize				PreferredSize();

			void				SetCategories(
									const std::vector<Category>& categories);
			void				SetSelected(int32 index);
			int32				Selected() const { return fSelected; }
			int32				DownloadsIndex() const
									{ return fCategories.size(); }
			void				SetChannel(const char* name);
									// an extra tab, chosen; NULL: none

private:
			int32				_Count() const;
			const char*			_Name(int32 index) const;
			BRect				_ButtonFrame(int32 index) const;
			BRect				_AddFrame() const;
			void				_ShowMenu(int32 index, BPoint where);

			std::vector<Category> fCategories;
			int32				fSelected;	// -1: a search of one's own
			BString				fChannel;
			bool				fAddHover;
};


#endif	// CATEGORY_BAR_H
