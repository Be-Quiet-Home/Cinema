/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef STYLE_H
#define STYLE_H


#include <Font.h>
#include <GraphicsDefs.h>
#include <Rect.h>


class BMenu;
class BView;


namespace Style {

extern const rgb_color kBackground;
extern const rgb_color kCard;		// pictures still loading
extern const rgb_color kBar;		// the top bar
extern const rgb_color kAccent;		// selection
extern const rgb_color kText;
extern const rgb_color kTextDim;	// secondary text

float		Scale();
						// 2 on a 4K screen, 1 otherwise
float		S(float value);
						// a size in pixels, for the screen
BFont		Font(float size, bool bold = false);
void		ScaleView(BView* view);
						// the system font of the view and its children,
						// as big as the screen needs
void		ScaleMenu(BMenu* menu);
						// the menus too: theirs is the system font
void		FillRound(BView* view, BRect frame, float radius,
				rgb_color color, bool composite = false);
void		DrawCentered(BView* view, const char* text, BRect frame,
				const BFont& font, rgb_color color,
				bool composite = false);
void		DrawText(BView* view, const char* text, BPoint leftBaseline,
				const BFont& font, rgb_color color,
				bool composite = false);
void		StrokeRound(BView* view, BRect frame, float radius,
				float width, rgb_color color);
void		DrawPlay(BView* view, BRect frame, rgb_color color,
				bool composite = false);

}	// namespace Style


#endif	// STYLE_H
