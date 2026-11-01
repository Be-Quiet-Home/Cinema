/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef STYLE_H
#define STYLE_H


#include <Font.h>
#include <GraphicsDefs.h>
#include <Rect.h>


class BView;


namespace Style {

extern const rgb_color kBackground;
extern const rgb_color kCard;		// pictures still loading
extern const rgb_color kBar;		// the top bar
extern const rgb_color kAccent;		// selection
extern const rgb_color kText;
extern const rgb_color kTextDim;	// secondary text

BFont		Font(float size, bool bold = false);
void		FillRound(BView* view, BRect frame, float radius,
				rgb_color color);
void		DrawCentered(BView* view, const char* text, BRect frame,
				const BFont& font, rgb_color color);
void		DrawText(BView* view, const char* text, BPoint leftBaseline,
				const BFont& font, rgb_color color);
void		StrokeRound(BView* view, BRect frame, float radius,
				float width, rgb_color color);
void		DrawPlay(BView* view, BRect frame, rgb_color color);

}	// namespace Style


#endif	// STYLE_H
