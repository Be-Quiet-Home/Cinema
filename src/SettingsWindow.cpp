/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */


#include "SettingsWindow.h"

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <Url.h>

#include "Messages.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Settings"


static const uint32 kMsgKeyChanged = 'skey';
static const uint32 kMsgGetKey = 'sget';
static const uint32 kMsgCheck = 'schk';
static const uint32 kMsgAdd = 'sadd';
static const uint32 kMsgRemove = 'srem';
static const uint32 kMsgFolderSelected = 'ssel';
static const uint32 kMsgPlayInCinema = 'splc';
static const uint32 kMsgResume = 'sres';
static const uint32 kMsgSource = 'ssrc';
static const uint32 kMsgQuality = 'squa';
static const uint32 kMsgActivate = 'sact';


static const char* kKeyPage = "https://www.themoviedb.org/settings/api";


SettingsWindow::SettingsWindow(const BMessenger& target,
	const std::vector<Provider>& providers,
	const std::vector<BString>& folders)
	:
	BWindow(BRect(0, 0, 480, 420), B_TRANSLATE("Cinema settings"),
		B_TITLED_WINDOW, B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fTarget(target),
	fSettings(Settings::Load()),
	fPanel(NULL)
{
	// the library: the key of TMDB
	fKey = new BTextControl("key", B_TRANSLATE("TMDB key:"),
		Settings::TmdbKey().String(), new BMessage(kMsgKeyChanged));
	fKey->SetModificationMessage(new BMessage(kMsgKeyChanged));
	fCheck = new BButton("check", B_TRANSLATE("Check"),
		new BMessage(kMsgCheck));
	BButton* getKey = new BButton("get", B_TRANSLATE("Get a key" B_UTF8_ELLIPSIS),
		new BMessage(kMsgGetKey));
	fKeyStatus = new BStringView("status", B_TRANSLATE("With a key, the "
		"library gets posters, plots and casts. It is free."));
	fKeyStatus->SetExplicitMinSize(BSize(300, B_SIZE_UNSET));

	BBox* library = new BBox("library");
	library->SetLabel(B_TRANSLATE("Library"));
	BLayoutBuilder::Group<>(library, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fKey)
			.Add(fCheck)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fKeyStatus, 10)
			.Add(getKey)
		.End();

	// the folders of the library
	fFolders = new BListView("folders");
	fFolders->SetSelectionMessage(new BMessage(kMsgFolderSelected));
	BScrollView* scroll = new BScrollView("folders scroll", fFolders, 0,
		false, true);
	scroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, 90));
	BButton* add = new BButton("add", B_TRANSLATE("Add" B_UTF8_ELLIPSIS),
		new BMessage(kMsgAdd));
	fRemove = new BButton("remove", B_TRANSLATE("Remove"),
		new BMessage(kMsgRemove));
	fRemove->SetEnabled(false);

	BBox* foldersBox = new BBox("folders box");
	foldersBox->SetLabel(B_TRANSLATE("Folders of the library"));
	BLayoutBuilder::Group<>(foldersBox, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(scroll, 10)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fRemove)
			.Add(add)
		.End();

	// how the videos play
	fPlayInCinema = new BCheckBox("cinema", B_TRANSLATE("Play the videos in "
		"Cinema, not in MediaPlayer"), new BMessage(kMsgPlayInCinema));
	fPlayInCinema->SetValue(fSettings.playInCinema);
	fResume = new BCheckBox("resume", B_TRANSLATE("Go on from where a video "
		"was left"), new BMessage(kMsgResume));
	fResume->SetValue(fSettings.resume);
	fResume->SetEnabled(fSettings.playInCinema);

	// how big the pictures of YouTube and PeerTube are
	BPopUpMenu* qualities = new BPopUpMenu("qualities");
	static const int32 kHeights[] = { 0, 360, 480, 720, 1080 };
	for (int32 height : kHeights) {
		BString label;
		if (height == 0)
			label = B_TRANSLATE("Automatic");
		else
			label.SetToFormat("%" B_PRId32 "p", height);
		BMessage* message = new BMessage(kMsgQuality);
		message->AddInt32("height", height);
		BMenuItem* item = new BMenuItem(label.String(), message);
		item->SetMarked(height == fSettings.videoHeight);
		qualities->AddItem(item);
	}
	if (qualities->FindMarked() == NULL)
		qualities->ItemAt(0)->SetMarked(true);
	fQuality = new BMenuField("quality", B_TRANSLATE("Video quality:"),
		qualities);
	BStringView* qualityNote = new BStringView("quality note",
		B_TRANSLATE("Above 360p, YouTube takes longer to start."));

	BBox* playback = new BBox("playback");
	playback->SetLabel(B_TRANSLATE("Playback"));
	BLayoutBuilder::Group<>(playback, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(fPlayInCinema)
		.Add(fResume)
		.Add(fQuality)
		.Add(qualityNote);

	// where a search goes first
	BPopUpMenu* sources = new BPopUpMenu("sources");
	for (const Provider& provider : providers) {
		if (!provider.Supports("search"))
			continue;
		BMessage* message = new BMessage(kMsgSource);
		message->AddString("provider", provider.id);
		BMenuItem* item = new BMenuItem(provider.name.String(), message);
		item->SetMarked(provider.id == fSettings.searchSource);
		sources->AddItem(item);
	}
	if (sources->FindMarked() == NULL && sources->CountItems() > 0)
		sources->ItemAt(0)->SetMarked(true);
	fSource = new BMenuField("source", B_TRANSLATE("Search first in:"),
		sources);

	BBox* search = new BBox("search");
	search->SetLabel(B_TRANSLATE("Search"));
	BLayoutBuilder::Group<>(search, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(fSource);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(library)
		.Add(foldersBox, 10)
		.Add(playback)
		.Add(search);

	BMessage list;
	for (const BString& folder : folders)
		list.AddString("path", folder);
	_SetFolders(&list);
	CenterOnScreen();
}


SettingsWindow::~SettingsWindow()
{
	delete fPanel;
}


void
SettingsWindow::Show()
{
	BWindow::Show();
	// opened from a menu, which gives the focus back to the main window
	// as it closes: take it again a moment later
	BMessageRunner::StartSending(BMessenger(this),
		new BMessage(kMsgActivate), 200000, 1);
}


bool
SettingsWindow::QuitRequested()
{
	// the key as it is, even without Enter
	Settings::SetTmdbKey(fKey->Text());
	fTarget.SendMessage(kMsgSettingsClosed);
	return true;
}


void
SettingsWindow::_SetFolders(const BMessage* message)
{
	while (BListItem* item = fFolders->RemoveItem((int32)0))
		delete item;
	const char* path;
	for (int32 i = 0; message->FindString("path", i, &path) == B_OK; i++)
		fFolders->AddItem(new BStringItem(path));
	fRemove->SetEnabled(false);
}


void
SettingsWindow::_Changed()
{
	fSettings.Save();
	fTarget.SendMessage(kMsgSettingsChanged);
}


void
SettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgActivate:
			Activate(true);
			if (fKey->Text()[0] == '\0')
				fKey->MakeFocus(true);
			break;
		case kMsgKeyChanged:
			Settings::SetTmdbKey(fKey->Text());
			fKeyStatus->SetText("");
			break;
		case kMsgGetKey:
			BUrl(kKeyPage, true).OpenWithPreferredApplication();
			break;
		case kMsgCheck:
			Settings::SetTmdbKey(fKey->Text());
			fKeyStatus->SetText(B_TRANSLATE("Checking" B_UTF8_ELLIPSIS));
			fCheck->SetEnabled(false);
			fTarget.SendMessage(kMsgCheckTmdb);
			break;
		case kMsgTmdbChecked:
			fCheck->SetEnabled(true);
			fKeyStatus->SetText(message->GetBool("ok", false)
				? B_TRANSLATE("The key works.")
				: B_TRANSLATE("TMDB does not accept this key."));
			break;

		case kMsgFolderSelected:
			fRemove->SetEnabled(fFolders->CurrentSelection() >= 0);
			break;
		case kMsgAdd:
			if (fPanel == NULL) {
				fPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this),
					NULL, B_DIRECTORY_NODE, false, NULL, NULL, true);
			}
			fPanel->Show();
			break;
		case B_REFS_RECEIVED:
		{
			entry_ref ref;
			for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
				BPath path(&ref);
				BMessage add(kMsgAddFolder);
				add.AddString("path", path.Path());
				fTarget.SendMessage(&add);
			}
			break;
		}
		case kMsgRemove:
		{
			BStringItem* item = dynamic_cast<BStringItem*>(
				fFolders->ItemAt(fFolders->CurrentSelection()));
			if (item == NULL)
				break;
			BMessage remove(kMsgRemoveFolder);
			remove.AddString("path", item->Text());
			fTarget.SendMessage(&remove);
			break;
		}
		case kMsgSettingsFolders:
			_SetFolders(message);
			break;

		case kMsgPlayInCinema:
			fSettings.playInCinema = fPlayInCinema->Value() == B_CONTROL_ON;
			fResume->SetEnabled(fSettings.playInCinema);
			_Changed();
			break;
		case kMsgResume:
			fSettings.resume = fResume->Value() == B_CONTROL_ON;
			_Changed();
			break;
		case kMsgSource:
			fSettings.searchSource = message->GetString("provider", "");
			_Changed();
			break;
		case kMsgQuality:
			fSettings.videoHeight = message->GetInt32("height", 0);
			_Changed();
			break;

		default:
			BWindow::MessageReceived(message);
	}
}
