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

extern const rgb_color kNight;		// background
extern const rgb_color kNightLight;	// cards
extern const rgb_color kNightDark;	// bars
extern const rgb_color kSun;		// selection, accents
extern const rgb_color kRed;		// the play button
extern const rgb_color kWhite;
extern const rgb_color kMist;		// secondary text

BFont		Font(float size, bool bold = true);
void		FillRound(BView* view, BRect frame, float radius,
				rgb_color color);
void		DrawCentered(BView* view, const char* text, BRect frame,
				const BFont& font, rgb_color color);
void		DrawText(BView* view, const char* text, BPoint leftBaseline,
				const BFont& font, rgb_color color);
void		DrawPlay(BView* view, BRect frame, rgb_color color);

}	// namespace Style


#endif	// STYLE_H
