/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "CinemaWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <CardLayout.h>
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
#include <View.h>

#include "CategoryBar.h"
#include "DetailView.h"
#include "Downloads.h"
#include "Gamepad.h"
#include "Helper.h"
#include "History.h"
#include "HomeView.h"
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
	fProvider("youtube"),
	fDetailsShown(false),
	fHelper(NULL),
	fGamepad(NULL),
	fGeneration(0),
	fHomeGeneration(0),
	fHomeLoaded(false),
	fHomeActive(0),
	fDetailsReturn(0),
	fCard(2),
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
	fDetail = new DetailView(BMessenger(this));
	fHome = new HomeView;
	BScrollView* homeScroll = new BScrollView("home-scroll", fHome, 0,
		false, true, B_NO_BORDER);

	// the home, the grid and the detail view take turns in the same place
	BView* pages = new BView("pages", B_WILL_DRAW);
	pages->SetViewColor(Style::kBackground);
	fCards = new BCardLayout();
	pages->SetLayout(fCards);
	pages->AddChild(scroll);
	pages->AddChild(fDetail);
	pages->AddChild(homeScroll);
	fCards->SetVisibleItem(2);
	fCard = 2;

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
		.Add(pages, 10);

	fWorker = new Worker(BMessenger(this));
	fWorker->Run();
	// to the preferred handler, so that the window gives the keys to the
	// focus view as it does with the keyboard's
	fGamepad = new Gamepad(BMessenger(NULL, this));

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
	for (MediaItem& video : Downloads::Load(&fDownloaded))
		delete video.picture;
	_ShowHome();
	fGamepad->Start();
}


CinemaWindow::~CinemaWindow()
{
	delete fGamepad;
	delete fPrefetchRunner;
	delete fHelper;
}


void
CinemaWindow::DispatchMessage(BMessage* message, BHandler* handler)
{
	// Esc goes back from the details, then leaves the full screen, and F
	// still toggles it on the details, wherever the focus is. F6 and Tab
	// move between the tabs and the videos, also with the gamepad.
	if (message->what == B_KEY_DOWN) {
		int32 key = 0;
		if (message->FindInt32("key", &key) == B_OK && key == B_F6_KEY) {
			_ToggleCategoryFocus();
			return;
		}
		const char* bytes;
		if (message->FindString("bytes", &bytes) == B_OK) {
			if (bytes[0] == B_TAB) {
				_ToggleCategoryFocus();
				return;
			}
			if (bytes[0] == B_ESCAPE) {
				if (fCategoryBar->IsFocus()) {
					_FocusContent();
					return;
				}
				if (fDetailsShown) {
					_CloseDetails();
					return;
				}
				if (fFullScreen) {
					_SetFullScreen(false);
					return;
				}
			}
			if (fDetailsShown && (bytes[0] == 'f' || bytes[0] == 'F')) {
				_SetFullScreen(!fFullScreen);
				return;
			}
		}
	}
	BWindow::DispatchMessage(message, handler);
}


bool
CinemaWindow::QuitRequested()
{
	fGamepad->Stop();
	// it may be waiting for curl: no need to wait for it
	fWorker->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
CinemaWindow::_Search(const char* query)
{
	_CloseDetails();
	fCards->SetVisibleItem(0);
	fCard = 0;
	fGeneration++;
	fGrid->SetMessage(B_TRANSLATE("Looking for the videos" B_UTF8_ELLIPSIS));
	fHelper->Search(fGeneration, kResultCount, query);
}


/*!	YouTube if the helper has it, otherwise the first provider that searches:
	the others are there for when there are more.
*/
void
CinemaWindow::_SetProviders(BMessage* message)
{
	fProviders = ProvidersFromMessage(*message);

	BString chosen;
	for (const Provider& provider : fProviders)
		if (provider.id == "youtube")
			chosen = provider.id;
	if (chosen.IsEmpty())
		for (const Provider& provider : fProviders)
			if (provider.Supports("search")) {
				chosen = provider.id;
				break;
			}
	if (chosen.IsEmpty() || chosen == fProvider)
		return;
	fProvider = chosen;
	fHelper->SetProvider(fProvider.String());
}


void
CinemaWindow::_ShowCategory(int32 index)
{
	if (index < 0 || index >= (int32)fCategories.size())
		return;
	_CloseDetails();
	fCards->SetVisibleItem(0);
	fCard = 0;
	fCategoryBar->SetSelected(fCategoryBar->CategoryTab(index));
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
	_CloseDetails();
	fCards->SetVisibleItem(0);
	fCard = 0;
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
	fCategoryBar->SetCategories(fCategories);
	if (fChannelId == id) {
		// the tab of its own becomes one of the others
		fCategoryBar->SetSelected(
			fCategoryBar->CategoryTab(fCategories.size() - 1));
		fChannelId = "";
	} else if (selected >= 0)
		fCategoryBar->SetSelected(selected);
	fHomeLoaded = false;
	fHomeRequests.clear();
	fHomeQueue.clear();
	fHomeActive = 0;
	if (fCard == 2 && !fDetailsShown)
		_ShowHome();
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
	int32 tab = fCategoryBar->CategoryTab(index);
	fCategoryBar->SetCategories(fCategories);
	if (selected == tab)
		fCategoryBar->SetSelected(-1);
	else if (selected > tab)
		fCategoryBar->SetSelected(selected - 1);
	fHomeLoaded = false;
	fHomeRequests.clear();
	fHomeQueue.clear();
	fHomeActive = 0;
	if (fCard == 2 && !fDetailsShown)
		_ShowHome();
}


std::vector<MediaItem>
CinemaWindow::_Videos(BMessage* message)
{
	std::vector<MediaItem> videos;
	const char* id;
	for (int32 i = 0; message->FindString("id", i, &id) == B_OK; i++) {
		MediaItem video;
		video.id = id;
		video.provider = message->GetString("provider", i, fProvider);
		video.kind = message->GetString("kind", i, "video");
		video.title = message->GetString("title", i, "");
		video.description = message->GetString("description", i, "");
		video.duration = message->GetInt32("duration", i, 0);
		video.channel = message->GetString("channel", i, "");
		video.channelId = message->GetString("channel id", i, "");
		video.year = message->GetInt32("year", i, 0);
		video.rating = message->GetDouble("rating", i, 0);
		video.poster = message->GetString("poster", i, "");
		video.thumbnail = message->GetString("thumbnail", i, "");
		video.downloaded = fDownloaded.count(video.id) > 0;
		auto downloading = fDownloading.find(video.id);
		if (downloading != fDownloading.end())
			video.progress = downloading->second.GetInt32("percent", 0);
		video.picture = NULL;
		videos.push_back(video);
	}
	return videos;
}


void
CinemaWindow::_Thumbnails(const std::vector<MediaItem>& videos,
	int32 generation)
{
	BMessage thumbnails(kMsgThumbnails);
	thumbnails.AddInt32("generation", generation);
	for (const MediaItem& video : videos)
		thumbnails.AddString("id", video.id);
	fWorker->PostMessage(&thumbnails);
}


void
CinemaWindow::_ShowResults(BMessage* message)
{
	std::vector<MediaItem> videos = _Videos(message);
	if (videos.empty()) {
		fGrid->SetMessage(B_TRANSLATE("No videos found"));
		return;
	}
	fGrid->SetVideos(videos);
	_Thumbnails(videos, fGeneration);

	// the first one is chosen already
	fWanted = videos[0].id;
	_PrefetchWanted();
}


void
CinemaWindow::_ShowHome()
{
	_CloseDetails();
	fCategoryBar->SetSelected(fCategoryBar->HomeIndex());
	fChannelId = "";
	if (!fHomeLoaded)
		_LoadHome();
	fCards->SetVisibleItem(2);
	fCard = 2;
	fHome->MakeFirstFocus();
}


void
CinemaWindow::_FocusCategoryBar()
{
	fCategoryBar->FocusSelected();
}


void
CinemaWindow::_FocusContent()
{
	if (fDetailsShown) {
		fDetail->MakeFocus(true);
		return;
	}
	if (fCard == 2) {
		fHome->MakeLastFocus();
		return;
	}
	fGrid->MakeFocus(true);
}


void
CinemaWindow::_ToggleCategoryFocus()
{
	if (fCategoryBar->IsFocus())
		_FocusContent();
	else
		_FocusCategoryBar();
}


void
CinemaWindow::_LoadHome()
{
	fHomeRequests.clear();
	fHomeQueue.clear();
	fHomeActive = 0;
	std::vector<ShelfSpec> shelves;

	std::vector<MediaItem> history = History::Load(fWorker->Cache());
	if (!history.empty()) {
		ShelfSpec spec;
		spec.title = B_TRANSLATE("Continue watching");
		spec.videos = history;
		shelves.push_back(spec);
	}

	fDownloaded.clear();
	std::vector<MediaItem> downloads = Downloads::Load(&fDownloaded);
	if (!downloads.empty()) {
		ShelfSpec spec;
		spec.title = B_TRANSLATE("Downloaded");
		spec.seeAll = BMessage(kMsgShowDownloads);
		spec.videos = downloads;
		shelves.push_back(spec);
	}

	for (size_t i = 0; i < fCategories.size(); i++) {
		ShelfSpec spec;
		spec.title = fCategories[i].name;
		BMessage seeAll(kMsgCategory);
		seeAll.AddInt32("index", i);
		spec.seeAll = seeAll;
		spec.message = B_TRANSLATE("Looking for the videos"
			B_UTF8_ELLIPSIS);
		int32 shelf = shelves.size();
		shelves.push_back(spec);
		int32 generation = --fHomeGeneration;
		fHomeRequests[generation] = shelf;
		fHomeQueue.push_back({generation, shelf, (int32)i});
	}

	if (shelves.empty())
		fHome->SetMessage(B_TRANSLATE("Type what to look for at the top "
			"right"));
	else
		fHome->SetShelves(shelves);
	fHomeLoaded = true;
	_StartHomeRequests();
}


/*!	The shelves are filled a few at a time: a search costs a yt-dlp, and
	they would all run together otherwise.
*/
void
CinemaWindow::_StartHomeRequests()
{
	static const int32 kAtOnce = 2;
	while (fHomeActive < kAtOnce && !fHomeQueue.empty()) {
		HomeRequest request = fHomeQueue.front();
		fHomeQueue.pop_front();
		if (fHomeRequests.find(request.generation) == fHomeRequests.end()
			|| request.category < 0
			|| request.category >= (int32)fCategories.size())
			continue;
		const Category& category = fCategories[request.category];
		if (category.IsChannel())
			fHelper->Channel(request.generation, kResultCount,
				category.channel);
		else
			fHelper->Search(request.generation, kResultCount,
				category.query);
		fHomeActive++;
	}
}


void
CinemaWindow::_HomeResults(BMessage* message, int32 shelf)
{
	std::vector<MediaItem> videos = _Videos(message);
	if (videos.empty())
		fHome->SetShelfMessage(shelf, B_TRANSLATE("No videos found"));
	else {
		fHome->SetShelfVideos(shelf, videos);
		_Thumbnails(videos, message->GetInt32("generation", -1));
	}
	if (fHomeActive > 0)
		fHomeActive--;
	_StartHomeRequests();
}


void
CinemaWindow::_HomeFailed(BMessage* message, int32 shelf)
{
	fHome->SetShelfMessage(shelf, B_TRANSLATE("YouTube could not be reached: "
		"is the network on?"));
	(void)message;
	if (fHomeActive > 0)
		fHomeActive--;
	_StartHomeRequests();
}


/*!	The page of one video: what the grid already knows, while the helper
	looks for the rest.
*/
void
CinemaWindow::_ShowDetails(BMessage* message)
{
	MediaItem item;
	item.provider = message->GetString("provider", fProvider);
	item.kind = message->GetString("kind", "video");
	item.id = message->GetString("id", "");
	item.title = message->GetString("title", "");
	item.description = message->GetString("description", "");
	item.channel = message->GetString("channel", "");
	item.channelId = message->GetString("channel id", "");
	item.duration = message->GetInt32("duration", 0);
	item.year = message->GetInt32("year", 0);
	item.date = message->GetString("date", "");
	item.views = message->GetInt64("views", 0);
	item.rating = message->GetDouble("rating", 0);
	item.poster = message->GetString("poster", "");
	item.thumbnail = message->GetString("thumbnail", "");
	item.path = message->GetString("path", "");
	item.downloaded = message->GetBool("downloaded", false);
	item.progress = message->GetInt32("progress", -1);
	item.picture = NULL;
	if (item.id.IsEmpty())
		return;

	if (!fDetailsShown)
		fDetailsReturn = fCard;
	fDetailsId = item.id;
	fDetailsShown = true;
	fDetail->SetItem(item);
	fDetail->SetLoading(true);

	// the small picture at once, while the big one is on its way
	BString path(fWorker->Cache());
	path << "/" << item.id << ".jpg";
	if (BEntry(path.String()).Exists())
		fDetail->SetPicture(BTranslationUtils::GetBitmap(path.String()));

	fCards->SetVisibleItem(1);
	fCard = 1;
	fDetail->MakeFocus(true);
	fHelper->Details(item.provider.String(), item.id.String(),
		item.kind.String());
}


/*!	What the helper found about the video on the detail page: the rest of
	its description, when it was published, its big picture.
*/
void
CinemaWindow::_Details(BMessage* message)
{
	BString id = message->GetString("id", "");
	if (!fDetailsShown || id != fDetailsId)
		return;
	if (!message->GetBool("found", false)) {
		fDetail->SetLoading(false);
		return;
	}

	MediaItem item = fDetail->Item();
	item.title = message->GetString("title", item.title);
	item.description = message->GetString("description", item.description);
	item.channel = message->GetString("channel", item.channel);
	item.channelId = message->GetString("channel id", item.channelId);
	item.duration = message->GetInt32("duration", item.duration);
	item.year = message->GetInt32("year", item.year);
	item.date = message->GetString("date", item.date);
	item.views = message->GetInt64("views", item.views);
	item.rating = message->GetDouble("rating", item.rating);
	item.poster = message->GetString("poster", item.poster);
	item.thumbnail = message->GetString("thumbnail", item.thumbnail);
	fDetail->SetItem(item);
	fDetail->SetLoading(false);

	if (!item.poster.IsEmpty()) {
		BMessage fetch(kMsgFetchPicture);
		fetch.AddString("id", item.id);
		fetch.AddString("url", item.poster);
		fWorker->PostMessage(&fetch);
	}
}


void
CinemaWindow::_Picture(BMessage* message)
{
	BString id = message->GetString("id", "");
	if (!fDetailsShown || id != fDetailsId)
		return;
	BBitmap* picture = BTranslationUtils::GetBitmap(
		message->GetString("path", ""));
	if (picture != NULL)
		fDetail->SetPicture(picture);
}


void
CinemaWindow::_CloseDetails()
{
	if (!fDetailsShown)
		return;
	fDetailsShown = false;
	fCards->SetVisibleItem(fDetailsReturn);
	fCard = fDetailsReturn;
	fDetail->SetItem(MediaItem());
	if (fDetailsReturn == 2) {
		if (!fHome->MoveFocus(fDetailsId.String(), 0))
			fHome->MakeFirstFocus();
	} else
		fGrid->MakeFocus(true);
	fDetailsId = "";
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
	if (fWanted.IsEmpty() || _Ready(fWanted) || !fFinding.empty()
		|| fDownloaded.count(fWanted) > 0)
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

		case kMsgFocusContent:
			_FocusContent();
			break;

		case kMsgHome:
			_ShowHome();
			break;
		case kMsgShelfMove:
			fHome->MoveFocus(message->GetString("id", ""),
				message->GetInt32("delta", 0));
			break;
		case kMsgShelfFocus:
			fHome->RememberFocus();
			fHome->Invalidate();
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
		case kMsgProviders:
			_SetProviders(message);
			break;
		case kMsgResults:
		{
			int32 generation = message->GetInt32("generation", -1);
			auto home = fHomeRequests.find(generation);
			if (home != fHomeRequests.end())
				_HomeResults(message, home->second);
			else if (generation == fGeneration)
				_ShowResults(message);
			break;
		}
		case kMsgSearchFailed:
		{
			int32 generation = message->GetInt32("generation", -1);
			auto home = fHomeRequests.find(generation);
			if (home != fHomeRequests.end())
				_HomeFailed(message, home->second);
			else if (generation == fGeneration) {
				fGrid->SetMessage(B_TRANSLATE("YouTube could not be reached: "
					"is the network on?"));
			}
			break;
		}
		case kMsgThumbnail:
		{
			int32 generation = message->GetInt32("generation", -1);
			BBitmap* picture = BTranslationUtils::GetBitmap(
				message->GetString("path", ""));
			auto home = fHomeRequests.find(generation);
			if (home != fHomeRequests.end())
				fHome->SetPicture(message->GetString("id", ""), picture);
			else if (generation == fGeneration)
				fGrid->SetPicture(message->GetString("id", ""), picture);
			else
				delete picture;
			break;
		}

		case kMsgShowDetails:
			_ShowDetails(message);
			break;
		case kMsgDetails:
			_Details(message);
			break;
		case kMsgPicture:
			_Picture(message);
			break;
		case kMsgCloseDetails:
			_CloseDetails();
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
			fHome->SetProgress(id, percent);
			fDetail->SetProgress(id.String(), percent);
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

			MediaItem recent;
			recent.id = id;
			recent.title = title;
			recent.provider = message->GetString("provider", fProvider);
			recent.kind = message->GetString("kind", "video");
			recent.channel = message->GetString("channel", "");
			recent.channelId = message->GetString("channel id", "");
			recent.duration = message->GetInt32("duration", 0);
			recent.path = path != NULL ? path : "";
			recent.downloaded = path != NULL;
			History::Remember(recent, fWorker->Cache());
			fHomeLoaded = false;

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
			fHome->SetBusy(id.String(), title.String());
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
				fHome->ClearBusy();
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
				fHome->ClearBusy();
				(new BAlert(fWaitingTitle.String(),
					B_TRANSLATE("This video can't be played here, try another "
						"one."), B_TRANSLATE("OK")))->Go(NULL);
			}
			_PrefetchWanted();
			break;
		}
		case kMsgHelperGone:
			fHomeQueue.clear();
			fHomeActive = 0;
			fGrid->SetBusy(NULL);
			fHome->ClearBusy();
			fHome->FailPending(B_TRANSLATE("Cinema needs yt-dlp and "
				"python3, and cinema-helper.py next to it"));
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
	_CloseDetails();
	fCards->SetVisibleItem(0);
	fCard = 0;
	fCategoryBar->SetSelected(fCategoryBar->DownloadsIndex());
	fChannelId = "";
	// what is still on its way for a search is not wanted any more
	fGeneration++;
	fDownloaded.clear();
	std::vector<MediaItem> videos = Downloads::Load(&fDownloaded);
	if (videos.empty()) {
		fGrid->SetMessage(B_TRANSLATE("No videos downloaded yet: right click "
			"a video, or press D"));
	} else
		fGrid->SetVideos(videos);
	fHomeLoaded = false;
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
	fHome->SetProgress(id, 0);
	fDetail->SetProgress(id.String(), 0);
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
	fHome->SetDownloaded(id.String(), true);
	fDetail->SetDownloaded(id.String(), true);
	fHomeLoaded = false;
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
	fHome->SetProgress(id, -1);
	fDetail->SetProgress(id.String(), -1);

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
	fHomeLoaded = false;
	if (_ShowingDownloads())
		_ShowDownloads();
	else {
		fDownloaded.erase(message->GetString("id", ""));
		if (fCard == 2 && !fDetailsShown)
			_ShowHome();
	}
}
