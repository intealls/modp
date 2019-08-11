// Copyright intealls
// License: GPL v3

#ifndef SRC_PLAYER_H_
#define SRC_PLAYER_H_

#include <stdbool.h>

#include <portaudio.h>

#include "Directory.h"
#include "RingBuffer.h"
#include "AudioManager.h"

typedef struct Player_State {
	Directory* dir;
	int dir_ofs;

	int min_length;
	bool auto_inc,
	     auto_rnd;

	AudioManager* am;
	Uint32 last_input;
} Player_State;

int           Player_Perform         (Player_State*);
int           Player_DirUp           (Player_State*);
int           Player_Home            (Player_State*);
int           Player_End             (Player_State*);
int           Player_PlayNext        (Player_State*, bool);
int           Player_PlayPrev        (Player_State*, bool);
int           Player_PlayRandom      (Player_State*);
int           Player_AlterOffset     (Player_State*, int);
int           Player_AlterMinLength  (Player_State*, int);
void          Player_ToggleAutoInc   (Player_State*);
void          Player_ToggleAutoRnd   (Player_State*);
void          Player_UpdateAutoInc   (Player_State*, bool);
void          Player_PlayPause       (Player_State*);
void          Player_AlterSubTrack   (Player_State*, int);
int           Player_GetPlaybackData (Player_State*, T*, int, bool);
void          Player_Destroy         (Player_State*);
Player_State* Player_Init            (int, int, int,
                                      int, bool, bool, const char*);

#endif /* SRC_PLAYER_H_ */
