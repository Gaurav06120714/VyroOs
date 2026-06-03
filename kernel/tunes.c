#include "tunes.h"
#include "../drivers/speaker.h"
#include "../drivers/timer.h"

typedef struct { uint32_t hz; uint32_t ms; } note_t;

static void play_seq(const note_t* notes, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (notes[i].hz) {
            speaker_on(notes[i].hz);
            sleep_ms(notes[i].ms);
            speaker_off();
        } else {
            sleep_ms(notes[i].ms);
        }
    }
}

void tunes_play_boot(void) {
    static const note_t s[] = {
        { 523, 100 }, { 659, 100 }, { 784, 200 }, { 0, 60 }, { 1047, 250 }
    };
    play_seq(s, 5);
}

void tunes_play_alert(void) {
    static const note_t s[] = { { 880, 80 }, { 1175, 120 } };
    play_seq(s, 2);
}

void tunes_play_scale(void) {
    // C major: C, D, E, F, G, A, B, C
    static const note_t s[] = {
        { 262, 120 }, { 294, 120 }, { 330, 120 }, { 349, 120 },
        { 392, 120 }, { 440, 120 }, { 494, 120 }, { 523, 200 }
    };
    play_seq(s, 8);
}

void tunes_play_arpeggio(void) {
    static const note_t s[] = {
        { 262, 80 }, { 330, 80 }, { 392, 80 }, { 523, 200 }
    };
    play_seq(s, 4);
}

void tunes_play_error(void) {
    static const note_t s[] = { { 220, 200 }, { 175, 200 }, { 147, 300 } };
    play_seq(s, 3);
}
