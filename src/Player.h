/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef PLAYER_H
#define PLAYER_H


#include <Locker.h>
#include <MediaDefs.h>
#include <Messenger.h>
#include <String.h>

#include <atomic>
#include <vector>


class BBitmap;
class BMediaFile;
class BMediaTrack;
class BSoundPlayer;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct SwrContext;
struct SwsContext;


/*!	Plays a file or an address with the Media Kit: a thread decodes the
	sound into a ring the sound player takes it from, another the pictures,
	shown when the sound gets there (the sound is the clock; without sound,
	the system time is). Pictures too late are not shown, which keeps up on
	slow machines.

	It tells the target kMsgPlayerOpened ("duration", "width", "height"),
	kMsgPlayerFrame when there is a new picture to show, kMsgPlayerEnded,
	and kMsgPlayerError when it cannot play the file at all.

	A stream of the relay of the helper (http://127.0.0.1:<port>/<token>/
	<seconds>) cannot go back and on: it starts where it is asked
	(?t=<seconds>), and Seek() asks the target with kMsgPlayerReopen
	("time") to open it again from there.

	A live stream (LiveAddress, "cinema-live:?url=...") is played with
	FFmpeg instead (PlayerLive.cpp): it opens HLS by itself, with its
	sound apart or not, and keeps up on a slow machine by decoding less.
	It goes out the same way, by the ring of the sound and the bitmaps.
*/
class Player {
public:
								Player(const BMessenger& target);
								~Player();

			void				Open(const char* address,
									bigtime_t startAt = 0);
									// in a thread of its own: an address
									// may take a while
			void				Close();

			void				SetPaused(bool paused);
			bool				IsPaused() const { return fPaused; }
			void				Seek(bigtime_t time);
			bigtime_t			Position() const;
			bigtime_t			Duration() const { return fDuration; }
			bool				IsOpen() const { return fOpened; }

			void				SetVolume(float volume);
			float				Volume() const { return fVolume; }

			BBitmap*			LockFrame();
									// the picture to show, NULL if none yet;
									// UnlockFrame() afterwards in any case
			void				UnlockFrame();
			BBitmap*			Overlay() const { return fOverlay; }
									// the picture the screen shows by
									// itself, when the overlay was free:
									// LockFrame() is not needed then

private:
	static	status_t			_OpenEntry(void* data);
			void				_Open();
	static	status_t			_VideoEntry(void* data);
			void				_VideoLoop();
	static	status_t			_AudioEntry(void* data);
			void				_AudioLoop();
	static	void				_PlayBuffer(void* cookie, void* buffer,
									size_t size,
									const media_raw_audio_format& format);
			void				_FillSound(void* buffer, size_t size);
			bigtime_t			_Clock() const;
			void				_SeekTracks(bigtime_t time);
			void				_CheckEnded();
			void				_Failed();
			bool				_SetUpVideo();
			void				_ShowFrame();
			bigtime_t			_StreamTime(bigtime_t time);
			bool				_SetUpBitmaps(int32 width, int32 height);

			// live, with FFmpeg (PlayerLive.cpp)
			void				_OpenLive();
			void				_CloseLive();
		static	status_t			_LiveEntry(void* data);
			void				_LiveLoop();
			int					_LiveVideoStream(int32 maxHeight);
			void				_LiveSkip(int level);
			void				_LiveLateness(bool late);
			bool				_LiveRender(AVFrame* frame);
			void				_LiveQueueSound(const void* data,
									size_t bytes);
			void				_LiveAlignSound(int64 micros);
			void				_LivePushSound(AVFrame* frame);
			int64				_LiveFrameMicros(int index,
									const AVFrame* frame) const;
			int64				_LiveClock();
		static	int					_LiveInterrupt(void* data);

			BMessenger			fTarget;
			BString				fAddress;
			bigtime_t			fStartAt;
			bool				fRelay;			// a stream of the relay
			bigtime_t			fRelayDuration;
			std::atomic<bigtime_t> fFirstTime;	// of the stream, or -1

			bool				fLive;			// played with FFmpeg

			AVFormatContext*	fLiveFormat;
			AVCodecContext*		fLiveVideo;
			AVCodecContext*		fLiveAudio;
			SwrContext*			fLiveResampler;
			SwsContext*			fLiveScaler;
			int					fLiveVideoIndex;
			int					fLiveAudioIndex;
			int					fLiveSkipLevel;
			int					fLiveLate;
			int					fLiveOnTime;
			int					fLivePixels;	// AVPixelFormat of the bitmaps
			int64				fLiveBase;		// stream time of the clock's 0
			int64				fLiveSoundEnd;	// stream time of the sound queued
			std::vector<uint8>	fLivePcm;
			std::atomic<bool>	fLiveStopRead;	// the reader of the stream stops

			BMediaFile*			fFile;
			BMediaTrack*		fVideoTrack;
			BMediaTrack*		fAudioTrack;
			BLocker				fVideoLock;		// around the reading of a track
			BLocker				fAudioLock;
			bigtime_t			fDuration;

			BBitmap*			fFront;			// shown
			BBitmap*			fBack;			// decoded into
			BBitmap*			fOverlay;		// what the screen shows, or NULL
			BLocker				fFrameLock;
			bool				fHasFrame;

			BSoundPlayer*		fSoundPlayer;
			media_raw_audio_format fSound;
			size_t				fFrameSize;		// of a sound frame, in bytes
			bigtime_t			fLatency;
			std::atomic<bigtime_t> fLastFill;	// when the sound was last taken
			bigtime_t			fFillTime;		// what a sound buffer lasts
			std::vector<uint8>	fRing;
			size_t				fRingRead;
			size_t				fRingFill;
			BLocker				fRingLock;

			// the clock: fClockBase when fPlayed sound frames are 0, or when
			// fClockStart was the system time without sound
			std::atomic<bigtime_t> fClockBase;
			std::atomic<int64>	fPlayed;
			bigtime_t			fClockStart;
			bigtime_t			fPausedClock;

			std::atomic<bool>	fPaused;
			std::atomic<bool>	fQuit;
			std::atomic<bool>	fOpened;
			std::atomic<bool>	fShowNext;		// even when paused: after a seek
			std::atomic<int32>	fGeneration;	// changes with a seek
			std::atomic<bool>	fVideoEnded;
			std::atomic<bool>	fAudioEnded;
			std::atomic<bool>	fEndSent;
			std::atomic<bool>	fFramePending;	// a kMsgPlayerFrame on its way
			float				fVolume;

			thread_id			fOpenThread;
			thread_id			fVideoThread;
			thread_id			fAudioThread;
};


#endif	// PLAYER_H
