/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "CinemaWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Entry.h>
#include <Button.h>
#include <Catalog.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <Notification.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include "CategoryBar.h"
#include "Downloads.h"
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
	fPrefetchRunner(NULL),
	fFullScreen(false)
{
	fCategoryBar = new CategoryBar(fCategories);
	fSearchField = new BTextControl("search", NULL, "",
		new BMessage(kMsgSearchField));
	BButton* searchButton = new BButton("find", B_TRANSLATE("Search"),
		new BMessage(kMsgSearchField));

	fGrid = new VideoGrid;
	BScrollView* scroll = new BScrollView("scroll", fGrid, 0, false, true,
		B_NO_BORDER);

	// the field as high as the text, not as the bar
	BLayoutItem* textItem = fSearchField->CreateTextViewLayoutItem();
	textItem->SetExplicitMinSize(BSize(320, B_SIZE_UNSET));
	textItem->SetExplicitMaxSize(BSize(460,
		textItem->PreferredSize().height));
	textItem->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH,
		B_ALIGN_VERTICAL_CENTER));
	searchButton->SetExplicitAlignment(BAlignment(B_ALIGN_HORIZONTAL_UNSET,
		B_ALIGN_VERTICAL_CENTER));

	BGroupView* top = new BGroupView(B_HORIZONTAL, B_USE_SMALL_SPACING);
	top->SetViewColor(Style::kBar);
	BLayoutBuilder::Group<>(top)
		.SetInsets(12, 6, 12, 6)
		.Add(fCategoryBar, 10)
		.Add(textItem)
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

	// which videos can be watched offline
	for (Video& video : Downloads::Load(&fDownloaded))
		delete video.picture;
	if (!fCategories.empty()) {
		_ShowCategory(0);
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


void
CinemaWindow::DispatchMessage(BMessage* message, BHandler* handler)
{
	// Esc leaves the full screen wherever the focus is
	if (fFullScreen && message->what == B_KEY_DOWN) {
		const char* bytes;
		if (message->FindString("bytes", &bytes) == B_OK
			&& bytes[0] == B_ESCAPE) {
			_SetFullScreen(false);
			return;
		}
	}
	BWindow::DispatchMessage(message, handler);
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
CinemaWindow::_ShowCategory(int32 index)
{
	if (index < 0 || index >= (int32)fCategories.size())
		return;
	fCategoryBar->SetSelected(index);
	fChannelId = "";
	const Category& category = fCategories[index];
	if (category.IsChannel()) {
		fGeneration++;
		fGrid->SetMessage(B_TRANSLATE("Looking for the videos"
			B_UTF8_ELLIPSIS));
		fHelper->Channel(fGeneration, kResultCount, category.channel);
	} else
		_Search(category.query.String());
	fGrid->MakeFocus(true);
}


void
CinemaWindow::_ShowChannel(const char* name, const char* id)
{
	if (id[0] == '\0')
		return;
	// one of the tabs already?
	for (size_t i = 0; i < fCategories.size(); i++) {
		if (fCategories[i].channel == id) {
			_ShowCategory(i);
			return;
		}
	}
	fCategoryBar->SetChannel(name);
	fChannelName = name;
	fChannelId = id;
	fGeneration++;
	fGrid->SetMessage(B_TRANSLATE("Looking for the videos" B_UTF8_ELLIPSIS));
	fHelper->Channel(fGeneration, kResultCount, id);
	fGrid->MakeFocus(true);
}


void
CinemaWindow::_AddChannel(const char* name, const char* id)
{
	if (name[0] == '\0' || id[0] == '\0')
		return;
	for (const Category& category : fCategories) {
		if (category.channel == id)
			return;
	}
	Category category;
	category.name = name;
	category.channel = id;
	fCategories.push_back(category);
	SaveCategories(fCategories);

	int32 selected = fCategoryBar->Selected();
	if (selected == fCategoryBar->DownloadsIndex())
		selected++;	// it comes after the categories
	fCategoryBar->SetCategories(fCategories);
	if (fChannelId == id) {
		// the tab of its own becomes one of the others
		fCategoryBar->SetSelected(fCategories.size() - 1);
		fChannelId = "";
	} else if (selected >= 0)
		fCategoryBar->SetSelected(selected);
}


void
CinemaWindow::_RemoveCategory(int32 index)
{
	if (index < 0 || index >= (int32)fCategories.size())
		return;
	fCategories.erase(fCategories.begin() + index);
	SaveCategories(fCategories);

	// what is shown stays: only the tab goes
	int32 selected = fCategoryBar->Selected();
	fCategoryBar->SetCategories(fCategories);
	if (selected == index)
		fCategoryBar->SetSelected(-1);
	else if (selected > index)
		fCategoryBar->SetSelected(selected - 1);
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
		video.channel = message->GetString("channel", i, "");
		video.channelId = message->GetString("channel id", i, "");
		video.downloaded = fDownloaded.count(video.id) > 0;
		auto downloading = fDownloading.find(video.id);
		if (downloading != fDownloading.end())
			video.progress = downloading->second.GetInt32("percent", 0);
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
		case kMsgFullScreen:
			_SetFullScreen(!fFullScreen);
			break;

		case kMsgCategory:
			_ShowCategory(message->GetInt32("index", -1));
			break;
		case kMsgChannel:
			_ShowChannel(message->GetString("channel", ""),
				message->GetString("channel id", ""));
			break;
		case kMsgAddChannel:
			// the one given, or the one shown
			_AddChannel(message->GetString("channel", fChannelName),
				message->GetString("channel id", fChannelId));
			break;
		case kMsgRemoveCategory:
			_RemoveCategory(message->GetInt32("index", -1));
			break;
		case kMsgSearchField:
		{
			BString query(fSearchField->Text());
			query.Trim();
			if (query.IsEmpty())
				break;
			fCategoryBar->SetSelected(-1);
			fChannelId = "";
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
			if (_ShowingDownloads())
				break;
			// worth looking for, if it stays chosen a moment
			fWanted = message->GetString("id", "");
			delete fPrefetchRunner;
			fPrefetchRunner = new BMessageRunner(BMessenger(this),
				new BMessage(kMsgPrefetch), kPrefetchDelay, 1);
			break;
		case kMsgPrefetch:
			_PrefetchWanted();
			break;

		case kMsgShowDownloads:
			_ShowDownloads();
			break;
		case kMsgDownload:
			_Download(message);
			break;
		case kMsgDownloadProgress:
		{
			BString id = message->GetString("id", "");
			auto downloading = fDownloading.find(id);
			if (downloading == fDownloading.end())
				break;
			int32 percent = message->GetInt32("percent", 0);
			downloading->second.SetInt32("percent", percent);
			fGrid->SetProgress(id, percent);
			break;
		}
		case kMsgDownloaded:
			_Downloaded(message);
			break;
		case kMsgDownloadFailed:
			_DownloadFailed(message);
			break;
		case kMsgTrash:
			_Trash(message);
			break;

		case kMsgPlay:
		{
			BString id = message->GetString("id", "");
			BString title = message->GetString("title", "");
			const char* path = message->GetString("path", NULL);
			auto downloaded = fDownloaded.find(id);
			if (path == NULL && downloaded != fDownloaded.end()
				&& BEntry(downloaded->second.String()).Exists())
				path = downloaded->second.String();
			if (path != NULL) {
				// downloaded: no need for the network
				_Play(path, title.String());
				break;
			}
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


void
CinemaWindow::_SetFullScreen(bool fullScreen)
{
	if (fullScreen == fFullScreen)
		return;
	fFullScreen = fullScreen;

	if (fullScreen) {
		fWindowFrame = Frame();
		BRect screen = BScreen(this).Frame();
		SetLook(B_NO_BORDER_WINDOW_LOOK);
		MoveTo(screen.LeftTop());
		ResizeTo(screen.Width(), screen.Height());
		SetFlags(Flags() | B_NOT_MOVABLE | B_NOT_RESIZABLE);
	} else {
		SetFlags(Flags() & ~(B_NOT_MOVABLE | B_NOT_RESIZABLE));
		SetLook(B_TITLED_WINDOW_LOOK);
		MoveTo(fWindowFrame.LeftTop());
		ResizeTo(fWindowFrame.Width(), fWindowFrame.Height());
	}
	Activate();
}


bool
CinemaWindow::_ShowingDownloads() const
{
	return fCategoryBar->Selected() == fCategoryBar->DownloadsIndex();
}


void
CinemaWindow::_ShowDownloads()
{
	fCategoryBar->SetSelected(fCategoryBar->DownloadsIndex());
	fChannelId = "";
	// what is still on its way for a search is not wanted any more
	fGeneration++;
	fDownloaded.clear();
	std::vector<Video> videos = Downloads::Load(&fDownloaded);
	if (videos.empty()) {
		fGrid->SetMessage(B_TRANSLATE("No videos downloaded yet: right click "
			"a video, or press D"));
	} else
		fGrid->SetVideos(videos);
	fGrid->MakeFocus(true);
}


void
CinemaWindow::_Download(BMessage* message)
{
	BString id = message->GetString("id", "");
	if (id.IsEmpty() || fDownloaded.count(id) > 0
		|| fDownloading.count(id) > 0)
		return;
	BString folder = Downloads::Folder();
	if (folder.IsEmpty())
		return;

	// the picture now, while it is in the cache
	Downloads::KeepPicture(fWorker->Cache(), id.String());
	BMessage download(*message);
	download.SetInt32("percent", 0);
	fDownloading[id] = download;
	fGrid->SetProgress(id, 0);
	fHelper->Download(id.String(), folder.String());
}


void
CinemaWindow::_Downloaded(BMessage* message)
{
	BString id = message->GetString("id", "");
	auto downloading = fDownloading.find(id);
	if (downloading == fDownloading.end())
		return;
	BMessage video = downloading->second;
	fDownloading.erase(downloading);

	Downloads::Describe(message->GetString("path", ""), video);
	fDownloaded[id] = message->GetString("path", "");
	fGrid->SetDownloaded(id, true);
	if (_ShowingDownloads())
		_ShowDownloads();

	BNotification notification(B_INFORMATION_NOTIFICATION);
	notification.SetGroup(B_TRANSLATE_SYSTEM_NAME("Cinema"));
	notification.SetTitle(B_TRANSLATE("Video downloaded"));
	notification.SetContent(video.GetString("title", ""));
	notification.Send();
}


void
CinemaWindow::_DownloadFailed(BMessage* message)
{
	BString id = message->GetString("id", "");
	auto downloading = fDownloading.find(id);
	if (downloading == fDownloading.end())
		return;
	BString title = downloading->second.GetString("title", "");
	fDownloading.erase(downloading);
	fGrid->SetProgress(id, -1);

	BNotification notification(B_ERROR_NOTIFICATION);
	notification.SetGroup(B_TRANSLATE_SYSTEM_NAME("Cinema"));
	notification.SetTitle(B_TRANSLATE("The video could not be downloaded"));
	notification.SetContent(title);
	notification.Send();
}


void
CinemaWindow::_Trash(BMessage* message)
{
	status_t status = Downloads::MoveToTrash(message->GetString("path", ""));
	if (status != B_OK) {
		BString text(B_TRANSLATE("The video could not be moved to the Trash: "
			"%error%"));
		text.ReplaceFirst("%error%", strerror(status));
		(new BAlert("Cinema", text.String(), B_TRANSLATE("OK")))->Go(NULL);
		return;
	}
	if (_ShowingDownloads())
		_ShowDownloads();
	else
		fDownloaded.erase(message->GetString("id", ""));
}
