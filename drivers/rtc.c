#include "rtc.h"
#include "pic.h"

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static uint8_t rtc_updating() {
    return cmos_read(RTC_STATUS_A) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)(((bcd >> 4) * 10) + (bcd & 0x0F));
}

void rtc_read(rtc_time_t* t) {
    rtc_time_t last;


    while (rtc_updating());
    t->second = cmos_read(RTC_SECONDS);
    t->minute = cmos_read(RTC_MINUTES);
    t->hour   = cmos_read(RTC_HOURS);
    t->day    = cmos_read(RTC_DAY);
    t->month  = cmos_read(RTC_MONTH);
    t->year   = cmos_read(RTC_YEAR);


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


    if (!(status_b & 0x04)) {
        t->second = bcd_to_bin(t->second);
        t->minute = bcd_to_bin(t->minute);

        t->hour   = (uint8_t)(((t->hour & 0x0F) + (((t->hour & 0x70) >> 4) * 10))
                              | (t->hour & 0x80));
        t->day    = bcd_to_bin(t->day);
        t->month  = bcd_to_bin(t->month);
        t->year   = bcd_to_bin((uint8_t)t->year);
    }


    if (!(status_b & 0x02) && (t->hour & 0x80)) {
        t->hour = (uint8_t)(((t->hour & 0x7F) + 12) % 24);
    }


    t->year += 2000;
}
