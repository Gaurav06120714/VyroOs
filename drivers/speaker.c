#include "speaker.h"
#include "pic.h"      // inb/outb
#include "timer.h"    // sleep_ms

#define PIT_CHANNEL2  0x42
#define PIT_COMMAND   0x43
#define SPEAKER_PORT  0x61
#define PIT_FREQUENCY 1193180

// ─────────────────────────────────────────────────
// speaker_on: program PIT channel 2 and enable speaker
// ─────────────────────────────────────────────────
void speaker_on(uint32_t frequency) {
    if (frequency == 0) return;

    uint32_t divisor = PIT_FREQUENCY / frequency;

    // Channel 2, lo/hi byte, mode 3 (square wave), binary
    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CHANNEL2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2, (uint8_t)((divisor >> 8) & 0xFF));

    // Connect speaker: set bits 0 (gate) and 1 (data)
    uint8_t tmp = inb(SPEAKER_PORT);
    if ((tmp & 3) != 3) {
        outb(SPEAKER_PORT, tmp | 3);
    }
}

// ─────────────────────────────────────────────────
// speaker_off: disconnect the speaker
// ─────────────────────────────────────────────────
void speaker_off() {
    uint8_t tmp = inb(SPEAKER_PORT) & 0xFC;   // clear bits 0,1
    outb(SPEAKER_PORT, tmp);
}

// ─────────────────────────────────────────────────
// beep: play a tone for duration_ms milliseconds
// ─────────────────────────────────────────────────
void beep(uint32_t frequency, uint32_t duration_ms) {
    speaker_on(frequency);
    sleep_ms(duration_ms);
    speaker_off();
}
