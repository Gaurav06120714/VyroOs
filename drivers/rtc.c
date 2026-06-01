#include "rtc.h"
#include "pic.h"   // for inb/outb

// ─────────────────────────────────────────────────
// cmos_read: read one CMOS/RTC register
// ─────────────────────────────────────────────────
static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

// ─────────────────────────────────────────────────
// rtc_updating: returns 1 if RTC is mid-update
// Reading during an update gives inconsistent values
// ─────────────────────────────────────────────────
static uint8_t rtc_updating() {
    return cmos_read(RTC_STATUS_A) & 0x80;
}

// ─────────────────────────────────────────────────
// bcd_to_bin: convert BCD-encoded byte to binary
// ─────────────────────────────────────────────────
static uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)(((bcd >> 4) * 10) + (bcd & 0x0F));
}

// ─────────────────────────────────────────────────
// rtc_read: read full date/time from the RTC
//
// Reads twice and compares to guard against a value
// changing between the start and end of our reads.
// Decodes BCD → binary if Status Register B says so.
// ─────────────────────────────────────────────────
void rtc_read(rtc_time_t* t) {
    rtc_time_t last;

    // Wait until not mid-update, then read
    while (rtc_updating());
    t->second = cmos_read(RTC_SECONDS);
    t->minute = cmos_read(RTC_MINUTES);
    t->hour   = cmos_read(RTC_HOURS);
    t->day    = cmos_read(RTC_DAY);
    t->month  = cmos_read(RTC_MONTH);
    t->year   = cmos_read(RTC_YEAR);

    // Read again until two consecutive reads match (stable value)
    do {
        last = *t;
        while (rtc_updating());
        t->second = cmos_read(RTC_SECONDS);
        t->minute = cmos_read(RTC_MINUTES);
        t->hour   = cmos_read(RTC_HOURS);
        t->day    = cmos_read(RTC_DAY);
        t->month  = cmos_read(RTC_MONTH);
        t->year   = cmos_read(RTC_YEAR);
    } while (last.second != t->second || last.minute != t->minute ||
             last.hour   != t->hour   || last.day    != t->day    ||
             last.month  != t->month  || last.year   != t->year);

    uint8_t status_b = cmos_read(RTC_STATUS_B);

    // Bit 2 of Status B: 0 = BCD encoding, 1 = binary
    if (!(status_b & 0x04)) {
        t->second = bcd_to_bin(t->second);
        t->minute = bcd_to_bin(t->minute);
        // Preserve the 12/24h flag (bit 7) on hour during BCD decode
        t->hour   = (uint8_t)(((t->hour & 0x0F) + (((t->hour & 0x70) >> 4) * 10))
                              | (t->hour & 0x80));
        t->day    = bcd_to_bin(t->day);
        t->month  = bcd_to_bin(t->month);
        t->year   = bcd_to_bin((uint8_t)t->year);
    }

    // Bit 1 of Status B: 0 = 12h mode. Convert PM hours to 24h.
    if (!(status_b & 0x02) && (t->hour & 0x80)) {
        t->hour = (uint8_t)(((t->hour & 0x7F) + 12) % 24);
    }

    // Year is two digits — assume 2000s
    t->year += 2000;
}
