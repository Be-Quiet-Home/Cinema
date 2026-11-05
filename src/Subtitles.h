/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef SUBTITLES_H
#define SUBTITLES_H


#include <Locker.h>
#include <OS.h>
#include <String.h>
#include <SupportDefs.h>

#include <atomic>
#include <set>
#include <vector>


struct SubtitleTrackInfo {
			BString				language;	// "it", "eng", empty if unknown
			BString				name;		// as the menu shows it
			bool				automatic;
			bool				loaded;		// its cues are here or coming
};


/*!	The subtitles of a video: files next to it, text tracks inside it, and
	tracks the helper gives (online), all in one list. The track of the
	system's language is chosen by itself until somebody chooses another.

	The cues of a file next to the video are read at once; those inside a
	container are read in the background, as FFmpeg demuxes the file, and
	arrive while it plays. The tracks of the helper are only named at first:
	Load() asks for their text.
*/
class Subtitles {
public:
								Subtitles();
								~Subtitles();

			void				Clear();
									// stops the reading, forgets every track

			int32				Count() const;
			bool				TrackAt(int32 index,
									SubtitleTrackInfo* info) const;

			int32				AddTrack(const char* language,
									const char* name, bool automatic,
									bool loaded);
			int32				AddText(const char* language,
									const char* name, const char* text,
									bool automatic);
			bool				Load(const char* language, bool automatic,
									const char* text);
									// the text of a track of the helper

			int32				Selected() const;
			void				Select(int32 index);
			void				SetPreference(const char* language);
									// chosen once, before the languages of
									// the system: a code such as "en",
									// "off" for none, empty for the system's
			BString				Choice() const;
									// what is chosen now, for SetPreference()
			void				Toggle();

			BString				TextAt(bigtime_t time) const;

			void				LoadSidecars(const char* video);
			void				StartEmbedded(const char* video);
			void				StopEmbedded();

private:
	struct Cue {
		bigtime_t			start;
		bigtime_t			end;
		BString				text;
	};

	struct Track {
		BString				language;
		BString				name;
		bool				automatic;
		bool				loaded;
		std::vector<Cue>	cues;
		std::vector<bigtime_t> maxEnd;
	};

	static	void				_Parse(const char* text,
									std::vector<Cue>& cues);
	static	void				_Index(std::vector<Cue>& cues,
									std::vector<bigtime_t>& maxEnd);
			void				_SelectPreferred();
			void				_AddCues(int32 index,
									const std::vector<Cue>& cues);
			void				_LoadFile(const char* path,
									const BString& videoBase);
			void				_ScanFolder(const char* folder,
									const BString& videoBase, bool exact,
									int32* added, int32 limit,
									std::set<BString>& seen);

	static	status_t			_EmbeddedEntry(void* data);
			void				_Embedded(const char* video);

			mutable BLocker		fLock;
			std::vector<Track>	fTracks;
			int32				fSelected;
			bool				fChosen;	// somebody chose, not the locale
			std::vector<BString> fPreferred;
			BString				fPreference;

			std::atomic<bool>	fStop;
			thread_id			fThread;
};


#endif	// SUBTITLES_H
