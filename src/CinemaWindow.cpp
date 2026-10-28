/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "CinemaWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include "CategoryBar.h"
#include "Messages.h"
#include "Style.h"
#include "VideoGrid.h"
#include "Worker.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Window"


static const char* kMediaPlayer = "application/x-vnd.Haiku-MediaPlayer";


CinemaWindow::CinemaWindow()
	:
	BWindow(BRect(50, 50, 1000, 650), B_TRANSLATE_SYSTEM_NAME("Cinema"),
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fCategories(LoadCategories()),
	fGeneration(0)
{
	fCategoryBar = new CategoryBar(fCategories);
	fSearchField = new BTextControl("search", NULL, "",
		new BMessage(kMsgSearchField));
	fSearchField->SetExplicitMinSize(BSize(220, B_SIZE_UNSET));
	fSearchField->SetExplicitMaxSize(BSize(320, B_SIZE_UNSET));
	BButton* searchButton = new BButton("find", B_TRANSLATE("Search"),
		new BMessage(kMsgSearchField));

	fGrid = new VideoGrid;
	BScrollView* scroll = new BScrollView("scroll", fGrid, 0, false, true,
		B_NO_BORDER);

	BGroupView* top = new BGroupView(B_HORIZONTAL, B_USE_SMALL_SPACING);
	top->SetViewColor(Style::kNightDark);
	BLayoutBuilder::Group<>(top)
		.SetInsets(12, 6, 12, 6)
		.Add(fCategoryBar, 10)
		.Add(fSearchField->CreateTextViewLayoutItem())
		.Add(searchButton);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(top)
		.Add(scroll, 10);

	fSearcher = new Worker("searcher", BMessenger(this));
	fSearcher->Run();
	fResolver = new Worker("resolver", BMessenger(this));
	fResolver->Run();

	// nearly the whole screen
	BRect frame = BScreen(this).Frame();
	MoveTo(frame.left + 20, frame.top + 30);
	ResizeTo(frame.Width() - 40, frame.Height() - 60);

	if (!fCategories.empty()) {
		_Search(fCategories[0].query.String());
	} else {
		fGrid->SetMessage(B_TRANSLATE("Type what to look for at the top "
			"right"));
	}
	fGrid->MakeFocus(true);
}


bool
CinemaWindow::QuitRequested()
{
	// they may be waiting for yt-dlp: no need to wait for them
	fSearcher->PostMessage(B_QUIT_REQUESTED);
	fResolver->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
CinemaWindow::_Search(const char* query)
{
	fGeneration++;
	fGrid->SetMessage(B_TRANSLATE("Looking for the videos" B_UTF8_ELLIPSIS));
	BMessage search(kMsgSearch);
	search.AddString("query", query);
	search.AddInt32("generation", fGeneration);
	fSearcher->PostMessage(&search);
}


void
CinemaWindow::_ShowResults(BMessage* message)
{
	std::vector<Video> videos;
	const char* id;
	for (int32 i = 0; message->FindString("id", i, &id) == B_OK; i++) {
		Video video;
		video.id = id;
		video.title = message->GetString("title", i, "");
		video.duration = message->GetInt32("duration", i, 0);
		video.picture = NULL;
		videos.push_back(video);
	}
	if (videos.empty())
		fGrid->SetMessage(B_TRANSLATE("No videos found"));
	else
		fGrid->SetVideos(videos);
}


void
CinemaWindow::_Play(const char* url, const char* title)
{
	const char* argv[] = { url };
	status_t status = be_roster->Launch(kMediaPlayer, 1, argv);
	if (status != B_OK && status != B_ALREADY_RUNNING) {
		BString text(B_TRANSLATE("MediaPlayer could not be started: %error%"));
		text.ReplaceFirst("%error%", strerror(status));
		(new BAlert(title, text.String(), B_TRANSLATE("OK")))->Go(NULL);
	}
}


void
CinemaWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgCategory:
		{
			int32 index = message->GetInt32("index", -1);
			if (index < 0 || index >= (int32)fCategories.size())
				break;
			fCategoryBar->SetSelected(index);
			_Search(fCategories[index].query.String());
			fGrid->MakeFocus(true);
			break;
		}
		case kMsgSearchField:
		{
			BString query(fSearchField->Text());
			query.Trim();
			if (query.IsEmpty())
				break;
			fCategoryBar->SetSelected(-1);
			_Search(query.String());
			fGrid->MakeFocus(true);
			break;
		}
		case kMsgResults:
			if (message->GetInt32("generation", -1) == fGeneration)
				_ShowResults(message);
			break;
		case kMsgThumbnail:
			if (message->GetInt32("generation", -1) == fGeneration) {
				fGrid->SetPicture(message->GetString("id", ""),
					BTranslationUtils::GetBitmap(
						message->GetString("path", "")));
			}
			break;
		case kMsgSearchFailed:
			if (message->GetInt32("generation", -1) == fGeneration) {
				fGrid->SetMessage(B_TRANSLATE("YouTube could not be reached: "
					"is the network on?"));
			}
			break;
		case kMsgPlay:
		{
			fGrid->SetBusy(message->GetString("title", ""));
			BMessage resolve(*message);
			resolve.what = kMsgResolve;
			fResolver->PostMessage(&resolve);
			break;
		}
		case kMsgStreamReady:
			fGrid->SetBusy(NULL);
			_Play(message->GetString("url", ""),
				message->GetString("title", ""));
			break;
		case kMsgStreamFailed:
		{
			fGrid->SetBusy(NULL);
			BAlert* alert = new BAlert(message->GetString("title", ""),
				B_TRANSLATE("This video can't be played here, try another "
					"one."), B_TRANSLATE("OK"));
			alert->Go(NULL);
			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}
