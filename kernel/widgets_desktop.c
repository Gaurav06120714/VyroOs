#include "widgets_desktop.h"
#include "compositor.h"
#include "../drivers/rtc.h"

static void two_digit(char* dst, int v) {
    dst[0] = (char)('0' + (v / 10) % 10);
    dst[1] = (char)('0' + v % 10);
    dst[2] = 0;
}

static const char* MONTH[12] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static int days_in_month(int month, int year) {
    static const uint8_t d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2) {
        int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return 28 + leap;
    }
    return d[month - 1];
}

// Zeller's congruence — Sunday=0, Saturday=6
static int day_of_week(int y, int m, int d) {
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100;
    int J = y / 100;
    int h = (d + 13 * (m + 1) / 5 + K + K/4 + J/4 + 5*J) % 7;
    return (h + 6) % 7;     // shift to Sunday=0
}

void widget_clock_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    comp_glass_panel(x, y, w, h, 0xFFFFFF, 70, 18, 0xFFFFFF);
    rtc_time_t t;
    rtc_read(&t);
    char buf[16];
    two_digit(buf, t.hour);    buf[2] = ':'; two_digit(buf+3, t.minute);
    buf[5] = ':'; two_digit(buf+6, t.second);
    comp_text(x + 16, y + 12, buf, 0xFFFFFF, 0x000000);
    char dateline[24];
    int p = 0;
    const char* mn = MONTH[(t.month - 1) % 12];
    dateline[p++] = mn[0]; dateline[p++] = mn[1]; dateline[p++] = mn[2];
    dateline[p++] = ' ';
    two_digit(dateline + p, t.day); p += 2;
    dateline[p++] = ',';
    dateline[p++] = ' ';
    int yr = 2000 + t.year;
    dateline[p++] = '0' + (yr/1000) % 10;
    dateline[p++] = '0' + (yr/100)  % 10;
    dateline[p++] = '0' + (yr/10)   % 10;
    dateline[p++] = '0' + (yr)      % 10;
    dateline[p]   = 0;
    comp_text(x + 16, y + 38, dateline, 0xE5E7EB, 0x000000);
}

void widget_calendar_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    comp_glass_panel(x, y, w, h, 0xFFFFFF, 55, 18, 0xFFFFFF);
    rtc_time_t t;
    rtc_read(&t);
    int year = 2000 + t.year;
    int month = t.month;
    int today = t.day;

    // Header
    char hdr[24]; int p = 0;
    const char* mn = MONTH[(month - 1) % 12];
    hdr[p++] = mn[0]; hdr[p++] = mn[1]; hdr[p++] = mn[2];
    hdr[p++] = ' ';
    hdr[p++] = '0' + (year/1000) % 10; hdr[p++] = '0' + (year/100) % 10;
    hdr[p++] = '0' + (year/10) % 10;   hdr[p++] = '0' + year % 10;
    hdr[p] = 0;
    comp_text(x + 12, y + 8, hdr, 0xFFFFFF, 0x000000);

    // Day labels
    comp_text(x + 12, y + 28, "Su Mo Tu We Th Fr Sa", 0xCBD5E1, 0x000000);

    // Grid
    int dow = day_of_week(year, month, 1);
    int dim = days_in_month(month, year);
    int row = 0, col = dow;
    for (int d = 1; d <= dim; d++) {
        char nbuf[3];
        two_digit(nbuf, d);
        uint32_t cx = x + 12 + col * 18;
        uint32_t cy = y + 44 + row * 12;
        uint32_t fg = (d == today) ? 0xFBBF24 : 0xE5E7EB;
        comp_text(cx, cy, nbuf, fg, 0x000000);
        col++;
        if (col == 7) { col = 0; row++; }
    }
}

void widget_weather_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    comp_glass_panel(x, y, w, h, 0x60A5FA, 80, 18, 0xFFFFFF);
    comp_text(x + 16, y + 10, "Weather",  0xFFFFFF, 0x000000);
    comp_text(x + 16, y + 30, "22 C",     0xFFFFFF, 0x000000);
    comp_text(x + 16, y + 50, "Partly cloudy", 0xE5E7EB, 0x000000);
    // (Real fetch needs HTTPS GET — see v3.24 / v3.27 for the building blocks.)
}

void widgets_desktop_render(void) {
    uint32_t sw = comp_width();
    uint32_t rx = sw - 220;
    widget_clock_render   (rx, 50,  200, 70);
    widget_calendar_render(rx, 140, 200, 140);
    widget_weather_render (rx, 300, 200, 80);
}
