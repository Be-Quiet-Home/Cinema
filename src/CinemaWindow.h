/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef CINEMA_WINDOW_H
#define CINEMA_WINDOW_H


#include <Message.h>
#include <String.h>
#include <Window.h>

#include <map>
#include <set>
#include <vector>
#include <deque>

#include "Categories.h"
#include "MediaItem.h"
#include "Provider.h"


class BCardLayout;
class BMenuField;
class BMessageRunner;
class Player;
class PlayerView;
class BTextControl;
class CategoryBar;
class DetailView;
class Gamepad;
class Helper;
class HomeView;
class VideoGrid;
class Worker;


class CinemaWindow : public BWindow {
public:
								CinemaWindow();
	virtual						~CinemaWindow();

	virtual	void				DispatchMessage(BMessage* message,
									BHandler* handler);
	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
	struct Stream {
		BString		url;
		bigtime_t	found;
	};

			void				_Search(const char* query,
									const char* provider = NULL);
			BString				_ProviderOf(const BString& id) const;
			void				_UpdateSources();
			void				_AddTorrent(const char* source);
			void				_AddFolder(const char* path);
			void				_Dropped(BMessage* message);
			BString				_DownloadStatus(const BMessage& download) const;
			void				_DownloadCancelled(BMessage* message);
			void				_SavePending();
			void				_OpenPlayer(const char* address,
									const char* title, const char* id);
			void				_ClosePlayer(bool save = true);
			void				_SavePosition();
			void				_PlayInMediaPlayer(const char* address,
									const char* title);
			void				_SetProviders(BMessage* message);
			void				_ShowCategory(int32 index);
			void				_ShowChannel(const char* name,
									const char* id);
			void				_AddChannel(const char* name, const char* id);
			void				_RemoveCategory(int32 index);
			void				_ShowDownloads();
			void				_ShowHome();
			void				_FocusCategoryBar();
			void				_FocusContent();
			void				_ToggleCategoryFocus();
			void				_LoadHome();
			void				_StartHomeRequests();
			void				_HomeResults(BMessage* message, int32 shelf);
			void				_HomeFailed(BMessage* message, int32 shelf);
			std::vector<MediaItem> _Videos(BMessage* message);
			void				_Thumbnails(
								const std::vector<MediaItem>& videos,
								int32 generation);
			bool				_ShowingDownloads() const;
			void				_ShowDetails(BMessage* message);
			void				_Details(BMessage* message);
			void				_Picture(BMessage* message);
			void				_CloseDetails();
			void				_Download(BMessage* message);
			void				_Downloaded(BMessage* message);
			void				_DownloadFailed(BMessage* message);
			void				_Trash(BMessage* message);
			void				_ShowResults(BMessage* message);
			bool				_Ready(const BString& id) const;
			void				_Find(const BString& id);
			void				_PrefetchWanted();
			void				_Play(const char* url, const char* title,
									const char* id = NULL);
			void				_SetFullScreen(bool fullScreen);

			std::vector<Category> fCategories;
			std::vector<Provider> fProviders;	// as the helper says
			BString				fProvider;	// the one searched
			CategoryBar*		fCategoryBar;
			BTextControl*		fSearchField;
			BMenuField*			fSourceField;
			BString				fSearchProvider;	// chosen in fSourceField
			VideoGrid*			fGrid;
			DetailView*			fDetail;
			HomeView*			fHome;
			BCardLayout*		fCards;
			Player*				fPlayer;
			PlayerView*			fPlayerView;
			bool				fPlayerShown;
			int32				fPlayerReturn;	// the card before it
			BString				fPlayingId;
			BString				fPlayingAddress;
			BString				fPlayingTitle;
			BMessageRunner*		fPlayerSaver;
			BString				fDetailsId;		// what the detail view shows
			bool				fDetailsShown;
			Helper*				fHelper;
			Gamepad*			fGamepad;
			Worker*				fWorker;
			int32				fGeneration;
			int32				fHomeGeneration;
			bool				fHomeLoaded;
			std::map<int32, int32> fHomeRequests;
			struct HomeRequest {
				int32			generation;
				int32			shelf;
				int32			category;
			};
			std::deque<HomeRequest> fHomeQueue;
			int32				fHomeActive;
			int32				fDetailsReturn;
			int32				fCard;

			std::map<BString, Stream> fStreams;	// the files found, by id
			std::set<BString>	fFinding;		// asked to the helper
			BString				fWaiting;		// clicked, not found yet
			BString				fWaitingTitle;
			BString				fWanted;		// chosen: worth finding
			BMessageRunner*		fPrefetchRunner;
			bool				fFullScreen;
			BString				fChannelName;	// shown in a tab of its own
			BString				fChannelId;
			BString				fChannelProvider;
			std::map<BString, BString> fItemProviders;
									// where each video shown comes from
			std::map<BString, BString> fChannelProviders;
			std::map<BString, BString> fDownloaded;
									// the files in the folder, by id
			std::map<BString, BMessage> fDownloading;
									// kMsgDownload, with "percent"
			BRect				fWindowFrame;	// to go back to
};


#endif	// CINEMA_WINDOW_H
