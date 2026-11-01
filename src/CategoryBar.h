/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CATEGORY_BAR_H
#define CATEGORY_BAR_H


#include <View.h>

#include <vector>

#include "Categories.h"


/*!	The tabs of the top bar. Home and the downloads come first, then the
	categories; a click on one sends kMsgCategory with its "index" to the
	window, the menu of a right click kMsgRemoveCategory. After them comes the
	channel being looked at, if any, with a button that sends kMsgAddChannel.
	Tabs that do not fit stay in a "more" menu at the right.
*/
class CategoryBar : public BView {
public:
								CategoryBar(
									const std::vector<Category>& categories);

	virtual	void				Draw(BRect updateRect);
	virtual	void				KeyDown(const char* bytes, int32 count);
	virtual	void				MakeFocus(bool focus = true);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	BSize				MinSize();
	virtual	BSize				PreferredSize();

			void				SetCategories(
									const std::vector<Category>& categories);
			void				SetSelected(int32 index);
			int32				Selected() const { return fSelected; }
			void				FocusSelected();
			int32				HomeIndex() const { return 0; }
			int32				DownloadsIndex() const { return 1; }
			int32				CategoryTab(int32 index) const
									{ return 2 + index; }
			int32				ChannelTab() const
									{ return 2 + fCategories.size(); }
			void				SetChannel(const char* name);
									// an extra tab, chosen; NULL: none

private:
			int32				_Count() const;
			const char*			_Name(int32 index) const;
			BRect				_ButtonFrame(int32 index) const;
			BRect				_AddFrame() const;
			BRect				_MoreFrame() const;
			bool				_HasMore() const;
			float				_FitLimit() const;
			void				_MoveFocus(int32 delta);
			void				_Activate();
			bool				_IsVisible(int32 index) const;
			void				_ClampFocus();
			void				_ShowMenu(int32 index, BPoint where);
			void				_ShowMore(BPoint where);

			std::vector<Category> fCategories;
			int32				fSelected;	// -1: a search of one's own
			int32				fFocused;
			BString				fChannel;
			bool				fAddHover;
			bool				fMoreHover;
};


#endif	// CATEGORY_BAR_H
