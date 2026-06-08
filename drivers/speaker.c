#include "speaker.h"
#include "pic.h"
#include "timer.h"

#define PIT_CHANNEL2  0x42
#define PIT_COMMAND   0x43
#define SPEAKER_PORT  0x61
#define PIT_FREQUENCY 1193180

void speaker_on(uint32_t frequency) {
    if (frequency == 0) return;

    uint32_t divisor = PIT_FREQUENCY / frequency;


    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CHANNEL2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2, (uint8_t)((divisor >> 8) & 0xFF));


    uint8_t tmp = inb(SPEAKER_PORT);
    if ((tmp & 3) != 3) {
        outb(SPEAKER_PORT, tmp | 3);
    }
}

void speaker_off() {
    uint8_t tmp = inb(SPEAKER_PORT) & 0xFC;
    outb(SPEAKER_PORT, tmp);
}

void beep(uint32_t frequency, uint32_t duration_ms) {
    speaker_on(frequency);
    sleep_ms(duration_ms);
    speaker_off();
}
