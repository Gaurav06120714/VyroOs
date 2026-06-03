#ifndef TUNES_H
#define TUNES_H

#include "../include/types.h"

// Built-in PC-speaker tunes. Each tune is a sequence of (note frequency Hz,
// duration ms) pairs played back via the speaker.h API.

void tunes_play_boot(void);     // Boot chime
void tunes_play_alert(void);    // Short alert
void tunes_play_scale(void);    // C major scale demo
void tunes_play_arpeggio(void); // C-E-G-C ascending
void tunes_play_error(void);    // Low warble

#endif
