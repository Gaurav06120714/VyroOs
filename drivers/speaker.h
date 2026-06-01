#ifndef SPEAKER_H
#define SPEAKER_H

#include "../include/types.h"

// Musical note frequencies (Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

void speaker_on(uint32_t frequency);
void speaker_off();
void beep(uint32_t frequency, uint32_t duration_ms);

#endif
