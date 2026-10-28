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
#include <MessageRunner.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include "CategoryBar.h"
#include "Helper.h"
#include "Messages.h"
#include "Style.h"
#include "VideoGrid.h"
#include "Worker.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Window"


static const char* kMediaPlayer = "application/x-vnd.Haiku-MediaPlayer";
static const int32 kResultCount = 24;
static const bigtime_t kPrefetchDelay = 1200000;
	// the time a video stays chosen before its file is looked for
static const bigtime_t kStreamLifetime = 3LL * 60 * 60 * 1000000;
	// YouTube's addresses expire after about six hours


CinemaWindow::CinemaWindow()
	:
	BWindow(BRect(50, 50, 1000, 650), B_TRANSLATE_SYSTEM_NAME("Cinema"),
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fCategories(LoadCategories()),
	fGeneration(0),
	fPrefetchRunner(NULL)
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

	fWorker = new Worker(BMessenger(this));
	fWorker->Run();

	// the helper is next to the application
	fHelper = new Helper(BMessenger(this));
	app_info info;
	BPath script;
	if (be_app->GetAppInfo(&info) == B_OK
		&& script.SetTo(&info.ref) == B_OK && script.GetParent(&script) == B_OK)
		script.Append("cinema-helper.py");
	fHelper->Start(script.Path(), fWorker->Cache());

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


CinemaWindow::~CinemaWindow()
{
	delete fPrefetchRunner;
	delete fHelper;
}


bool
CinemaWindow::QuitRequested()
{
	// it may be waiting for curl: no need to wait for it
	fWorker->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
CinemaWindow::_Search(const char* query)
{
	fGeneration++;
	fGrid->SetMessage(B_TRANSLATE("Looking for the videos" B_UTF8_ELLIPSIS));
	fHelper->Search(fGeneration, kResultCount, query);
}


void
CinemaWindow::_ShowResults(BMessage* message)
{
	std::vector<Video> videos;
	BMessage thumbnails(kMsgThumbnails);
	thumbnails.AddInt32("generation", fGeneration);
	const char* id;
	for (int32 i = 0; message->FindString("id", i, &id) == B_OK; i++) {
		Video video;
		video.id = id;
		video.title = message->GetString("title", i, "");
		video.duration = message->GetInt32("duration", i, 0);
		video.picture = NULL;
		videos.push_back(video);
		thumbnails.AddString("id", id);
	}
	if (videos.empty()) {
		fGrid->SetMessage(B_TRANSLATE("No videos found"));
		return;
	}
	fGrid->SetVideos(videos);
	fWorker->PostMessage(&thumbnails);

	// the first one is chosen already
	fWanted = videos[0].id;
	_PrefetchWanted();
}


bool
CinemaWindow::_Ready(const BString& id) const
{
	auto found = fStreams.find(id);
	return found != fStreams.end()
		&& system_time() - found->second.found < kStreamLifetime;
}


void
CinemaWindow::_Find(const BString& id)
{
	if (fFinding.count(id) != 0)
		return;
	fFinding.insert(id);
	fHelper->Find(id.String());
}


/*!	Looks for the file of the chosen video ahead, one at a time, so that it
	plays as soon as it is clicked.
*/
void
CinemaWindow::_PrefetchWanted()
{
	if (fWanted.IsEmpty() || _Ready(fWanted) || !fFinding.empty())
		return;
	_Find(fWanted);
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
		case kMsgSearchFailed:
			if (message->GetInt32("generation", -1) == fGeneration) {
				fGrid->SetMessage(B_TRANSLATE("YouTube could not be reached: "
					"is the network on?"));
			}
			break;
		case kMsgThumbnail:
			if (message->GetInt32("generation", -1) == fGeneration) {
				fGrid->SetPicture(message->GetString("id", ""),
					BTranslationUtils::GetBitmap(
						message->GetString("path", "")));
			}
			break;

		case kMsgSelected:
			// worth looking for, if it stays chosen a moment
			fWanted = message->GetString("id", "");
			delete fPrefetchRunner;
			fPrefetchRunner = new BMessageRunner(BMessenger(this),
				new BMessage(kMsgPrefetch), kPrefetchDelay, 1);
			break;
		case kMsgPrefetch:
			_PrefetchWanted();
			break;

		case kMsgPlay:
		{
			BString id = message->GetString("id", "");
			BString title = message->GetString("title", "");
			if (_Ready(id)) {
				_Play(fStreams[id].url.String(), title.String());
				break;
			}
			fWaiting = id;
			fWaitingTitle = title;
			fGrid->SetBusy(title.String());
			_Find(id);
			break;
		}
		case kMsgFound:
		{
			BMessage follow(*message);
			follow.what = kMsgFollow;
			fWorker->PostMessage(&follow);
			break;
		}
		case kMsgStreamReady:
		{
			BString id = message->GetString("id", "");
			fFinding.erase(id);
			Stream stream = { message->GetString("url", ""), system_time() };
			fStreams[id] = stream;
			if (id == fWaiting) {
				fWaiting = "";
				fGrid->SetBusy(NULL);
				_Play(stream.url.String(), fWaitingTitle.String());
			}
			_PrefetchWanted();
			break;
		}
		case kMsgLost:
		{
			BString id = message->GetString("id", "");
			fFinding.erase(id);
			if (id == fWanted)
				fWanted = "";
			if (id == fWaiting) {
				fWaiting = "";
				fGrid->SetBusy(NULL);
				(new BAlert(fWaitingTitle.String(),
					B_TRANSLATE("This video can't be played here, try another "
						"one."), B_TRANSLATE("OK")))->Go(NULL);
			}
			_PrefetchWanted();
			break;
		}
		case kMsgHelperGone:
			fGrid->SetBusy(NULL);
			fGrid->SetMessage(B_TRANSLATE("Cinema needs yt-dlp and python3, "
				"and cinema-helper.py next to it"));
			break;

		default:
			BWindow::MessageReceived(message);
	}
}
