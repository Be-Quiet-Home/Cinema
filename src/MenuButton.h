/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef MENU_BUTTON_H
#define MENU_BUTTON_H


#include <View.h>


/*!	Three lines at the end of the top bar: its menu has the settings and
	what Cinema is.
*/
class MenuButton : public BView {
public:
								MenuButton();

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	BSize				MinSize();
	virtual	BSize				MaxSize();

private:
			bool				fHover;
};


#endif	// MENU_BUTTON_H
