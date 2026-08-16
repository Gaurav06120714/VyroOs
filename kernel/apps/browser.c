#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../net.h"
#include "../html.h"
#include "../dns.h"
#include "../dns_real.h"
#include "../http.h"

#define TOOLBAR_H  40
#define TABBAR_H   32
#define STATUSBAR_H 20
#define URL_MAX    128

static char url[URL_MAX] = "vyro://home";
static int  url_focus = 0;
static int  url_len   = 0;
static int  initted   = 0;
static int  loading   = 0;

typedef struct {
    char  title[32];
    char  url[URL_MAX];
} tab_t;

#define MAX_TABS 4
static tab_t tabs[MAX_TABS];
static int   ntabs  = 1;
static int   curtab = 0;

static int slen(const char* s){int i=0;while(s[i])i++;return i;}
static void scpy(char* d,const char* s,int m){int i=0;while(s[i]&&i<m-1){d[i]=s[i];i++;}d[i]=0;}
static void sapp(char* d,const char* s,int m){int i=slen(d),j=0;while(s[j]&&i<m-1){d[i++]=s[j++];}d[i]=0;}

/* ---- real page fetch state ----
 * page_buf holds the plain-text view of the most recently fetched web page.
 * page_ready is set once a non-vyro:// URL has been fetched (success or error). */
#define PAGE_MAX 8192
static char page_buf[PAGE_MAX];
static int  page_ready = 0;

static int is_vyro(const char* u){
    return u[0]=='v'&&u[1]=='y'&&u[2]=='r'&&u[3]=='o'&&
           u[4]==':'&&u[5]=='/'&&u[6]=='/';
}

/* Strip HTML tags + <script>/<style> blocks and collapse whitespace into
 * page_buf, so a fetched page renders as readable text. */
static void html_to_text(const uint8_t* in, uint32_t len, char* out, int outmax){
    int o = 0, intag = 0, skip = 0, lastnl = 1, lastsp = 1;
    for (uint32_t i = 0; i < len && o < outmax - 1; i++){
        char ch = (char)in[i];
        if (ch == '<'){
            intag = 1;
            /* detect <script / <style / </script / </style to drop their text */
            const char* r = (const char*)(in + i + 1);
            if (i + 7 < len){
                if ((r[0]=='s'||r[0]=='S') && (r[1]=='c'||r[1]=='C')) skip = 1;
                else if ((r[0]=='s'||r[0]=='S') && (r[1]=='t'||r[1]=='T')) skip = 1;
                else if (r[0]=='/' ) skip = 0;
            }
            continue;
        }
        if (ch == '>'){ intag = 0; continue; }
        if (intag || skip) continue;
        if (ch == '\r') continue;
        if (ch == '\n'){ if(!lastnl){out[o++]='\n'; lastnl=1; lastsp=1;} continue; }
        if (ch == ' ' || ch == '\t'){ if(!lastsp){out[o++]=' '; lastsp=1;} continue; }
        out[o++] = ch; lastsp = 0; lastnl = 0;
    }
    out[o] = 0;
}

/* Resolve host, HTTP GET over the real TCP stack, and store readable text. */
static void browser_fetch(const char* u){
    const char* p = u;
    /* skip http:// or https:// scheme if present */
    if ((p[0]=='h'||p[0]=='H')){
        const char* s = p; while (*s && *s != ':' && *s != '/') s++;
        if (s[0]==':' && s[1]=='/' && s[2]=='/') p = s + 3;
    }
    char host[128]; int hi = 0;
    while (*p && *p!='/' && *p!=':' && hi < 127) host[hi++] = *p++;
    host[hi] = 0;
    uint16_t port = 80;
    if (*p == ':'){ p++; int pr=0; while(*p>='0'&&*p<='9'){pr=pr*10+(*p-'0');p++;} if(pr>0) port=(uint16_t)pr; }
    char path[160]; int pi = 0;
    if (*p != '/') path[pi++] = '/';
    while (*p && pi < 159) path[pi++] = *p++;
    path[pi] = 0;

    if (hi == 0){ scpy(page_buf, "EMPTY ADDRESS\n\n  Type a host like example.com", PAGE_MAX); page_ready=1; return; }

    uint8_t ip[4];
    /* Fast path: static hosts table. Fallback: real UDP DNS via the
     * QEMU/user-net resolver at 10.0.2.3 (3s timeout). */
    int resolved = (dns_resolve(host, ip) == 0);
    if (!resolved){
        uint8_t dns_ip[4] = { 10, 0, 2, 3 };
        resolved = dns_real_resolve(host, dns_ip, 3000, ip) ? 1 : 0;
    }
    if (!resolved){
        scpy(page_buf, "COULD NOT RESOLVE HOST\n\n  DNS lookup failed for: ", PAGE_MAX);
        sapp(page_buf, host, PAGE_MAX);
        sapp(page_buf, "\n\n  The network may be down, or the host does\n  not exist. Plain http:// sites work best;\n  https:// and JS-heavy sites (YouTube, etc.)\n  are not supported.", PAGE_MAX);
        page_ready = 1; return;
    }
    static uint8_t resp[PAGE_MAX];
    int n = http_get(ip, port, host, path, resp, sizeof(resp) - 1, 8000);
    if (n <= 0){
        scpy(page_buf, "REQUEST FAILED\n\n  Connected DNS but the HTTP GET to\n  ", PAGE_MAX);
        sapp(page_buf, host, PAGE_MAX);
        sapp(page_buf, " timed out or was refused.", PAGE_MAX);
        page_ready = 1; return;
    }
    int status = 0; const uint8_t* body = 0; uint32_t blen = 0; int32_t cl = -1;
    if (http_parse_response(resp, (uint32_t)n, &status, &body, &blen, &cl) && body && blen > 0)
        html_to_text(body, blen, page_buf, PAGE_MAX);
    else
        html_to_text(resp, (uint32_t)n, page_buf, PAGE_MAX);
    page_ready = 1;
}

static const char* HOME_CONTENT =
    "VYRO BROWSER 2.0\n\n"
    "  Welcome to VyroBrowser.\n\n"
    "BOOKMARKS\n"
    "  [1] vyro://home         -- Home page\n"
    "  [2] vyro://about        -- About VyroOS\n"
    "  [3] vyro://net          -- Network info\n"
    "  [4] github.com/Gaurav06120714/VyroOs\n\n"
    "QUICK INFO\n"
    "  Kernel:  custom from-scratch (x86_64)\n"
    "  Net:     TCP/IP + TLS 1.3\n"
    "  Crypto:  ChaCha20-Poly1305, X25519, ECDSA\n"
    "  Stack:   Eth->ARP->IPv4->TCP/UDP->TLS\n\n"
    "Type a URL in the address bar and press Enter.";

static const char* ABOUT_CONTENT =
    "ABOUT VYRO OS\n\n"
    "  Version:    2.0.0 (v8)\n"
    "  Author:     Gaurav Ganesh Teegulla\n"
    "  License:    MIT  ($0 budget)\n"
    "  Arch:       x86-64, 64-bit\n"
    "  Lines:      ~53,000\n"
    "  Files:      201 source files\n"
    "  Tags:       155+ git tags\n\n"
    "SUBSYSTEMS\n"
    "  Kernel:     from-scratch microkernel-style\n"
    "  GUI:        double-buffered compositor\n"
    "  Network:    TCP/IP + TLS 1.3\n"
    "  Crypto:     9 primitives (RFC verified)\n"
    "  Security:   SHA-256, ring 0/3, TSS\n"
    "  Storage:    ATA PIO + FAT32 + VyFS";

static const char* NET_CONTENT =
    "NETWORK INFORMATION\n\n"
    "  NIC:        RTL8139 (QEMU emulated)\n"
    "  Driver:     real TX/RX ring buffers\n"
    "  IP:         DHCP assigned\n"
    "  Stack:      Ethernet -> ARP -> IPv4 -> TCP/UDP\n"
    "  IPv6:       link-local (EUI-64), ICMPv6 echo\n"
    "  TLS:        1.3 (RFC 8448 key schedule)\n"
    "  AEAD:       ChaCha20-Poly1305 (RFC 8439)\n"
    "  ECDH:       X25519 (RFC 7748)\n"
    "  HKDF:       HMAC-SHA256 + HKDF (RFC 5869)\n"
    "  DNS:        UDP port 53 client\n"
    "  DHCP:       UDP port 67/68 client";

static const char* get_page_content(const char* u) {
    if (is_vyro(u)) {
        const char* path = u + 7;
        if (path[0]=='h') return HOME_CONTENT;
        if (path[0]=='a') return ABOUT_CONTENT;
        if (path[0]=='n') return NET_CONTENT;
        return HOME_CONTENT;
    }
    /* real fetched page (or an error report) lives in page_buf */
    if (page_ready) return page_buf;
    return HOME_CONTENT;
}

static void navigate(const char* u) {
    scpy(tabs[curtab].url, u, URL_MAX);
    scpy(url, u, URL_MAX); url_len = slen(url);
    if (is_vyro(u)) {
        page_ready = 0;
        if (u[7]=='h') scpy(tabs[curtab].title, "Home", 32);
        else if (u[7]=='a') scpy(tabs[curtab].title, "About VyroOS", 32);
        else if (u[7]=='n') scpy(tabs[curtab].title, "Network", 32);
        else { scpy(tabs[curtab].title, u, 28); }
        loading = 0;
        return;
    }
    /* Real network fetch. NOTE: this blocks the UI until the request
     * completes or times out (single-threaded cooperative kernel). */
    scpy(tabs[curtab].title, u, 28);
    loading = 1;
    browser_fetch(u);
    loading = 0;
}

static void render_browser(app_ctx_t* c) {
    if (!initted) {
        scpy(tabs[0].title, "Home", 32);
        scpy(tabs[0].url,   "vyro://home", URL_MAX);
        url_len = slen(url);
        initted = 1;
    }
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    /* ---- tab bar ---- */
    uint32_t tabbg = t->is_dark ? 0x1C2230 : 0xE0E4EE;
    comp_rect(c->origin_x, c->origin_y, c->width, TABBAR_H, tabbg);
    comp_rect(c->origin_x, c->origin_y + TABBAR_H - 1, c->width, 1, t->win_border);

    for (int i = 0; i < ntabs; i++) {
        int tx = c->origin_x + 4 + i * 148;
        int is_cur = (i == curtab);
        uint32_t tbg2 = is_cur ? (t->is_dark ? 0x0D1117 : 0xFFFFFF) : tabbg;
        comp_rect(tx, c->origin_y + 4, 144, 25, tbg2);
        comp_border(tx, c->origin_y + 4, 144, 25, t->win_border);
        int tl = slen(tabs[i].title); if (tl > 14) tl = 14;
        comp_text(tx + 8, c->origin_y + 10, tabs[i].title,
                  is_cur ? t->accent_hi : t->text_dim, tbg2);
        /* close X */
        int xbx = tx + 126, xby = c->origin_y + 9;
        int xhov = (abs_mx >= xbx && abs_mx < xbx+14 && abs_my >= xby && abs_my < xby+14);
        comp_rect(xbx, xby, 14, 14, xhov ? 0xFF5050 : tbg2);
        comp_text(xbx + 3, xby + 1, "x", xhov ? 0xFFFFFF : t->text_dim, xhov ? 0xFF5050 : tbg2);
        if (xhov && c->clicked && ntabs > 1) {
            for (int j = i; j < ntabs - 1; j++) tabs[j] = tabs[j+1];
            ntabs--;
            if (curtab >= ntabs) curtab = ntabs - 1;
            navigate(tabs[curtab].url);
        }
        int hov2 = !xhov && (abs_mx >= tx && abs_mx < tx+144 && abs_my >= c->origin_y+4 && abs_my < c->origin_y+29);
        if (hov2 && c->clicked) { curtab = i; navigate(tabs[curtab].url); }
        (void)tl;
    }
    /* new tab */
    if (ntabs < MAX_TABS) {
        int nbx = c->origin_x + 4 + ntabs * 148;
        if (w_button(nbx, c->origin_y + 6, 28, 22, "+", abs_mx, abs_my, c->clicked)) {
            scpy(tabs[ntabs].title, "New Tab", 32);
            scpy(tabs[ntabs].url,   "vyro://home", URL_MAX);
            curtab = ntabs++;
            navigate("vyro://home");
        }
    }

    /* ---- toolbar ---- */
    uint32_t toolbg = t->is_dark ? 0x161B22 : 0xEEF1F8;
    int ty = c->origin_y + TABBAR_H;
    comp_rect(c->origin_x, ty, c->width, TOOLBAR_H, toolbg);
    comp_rect(c->origin_x, ty + TOOLBAR_H - 1, c->width, 1, t->win_border);

    w_button(c->origin_x + 6,  ty + 8, 28, 24, "<",  abs_mx, abs_my, c->clicked);
    w_button(c->origin_x + 38, ty + 8, 28, 24, ">",  abs_mx, abs_my, c->clicked);
    if (w_button(c->origin_x + 70, ty + 8, 52, 24, "Reload", abs_mx, abs_my, c->clicked))
        navigate(url);

    /* URL bar */
    int ubx = c->origin_x + 128, ubw = c->width - 136;
    uint32_t ubg = url_focus ? (t->is_dark ? 0x1A2236 : 0xFFFFFF)
                             : (t->is_dark ? 0x0D1117 : 0xF0F0F0);
    comp_rect(ubx, ty + 8, ubw, 24, ubg);
    comp_border(ubx, ty + 8, ubw, 24, url_focus ? t->accent : t->win_border);
    /* lock icon */
    comp_text(ubx + 4, ty + 12, "[S]", 0x3FB950, ubg);
    comp_text(ubx + 30, ty + 12, url, t->accent_hi, ubg);

    int uhov = (abs_mx >= ubx && abs_mx < ubx+ubw && abs_my >= ty+8 && abs_my < ty+32);
    if (uhov && c->clicked) url_focus = 1;
    else if (c->clicked)    url_focus = 0;

    if (url_focus && c->key) {
        if (c->key == '\n') { navigate(url); url_focus = 0; }
        else if (c->key == '\b' && url_len > 0) url[--url_len] = 0;
        else if (c->key >= 32 && c->key < 127 && url_len < URL_MAX-1) {
            url[url_len++] = (char)c->key; url[url_len] = 0;
        }
    }

    /* ---- content area ---- */
    int content_y = ty + TOOLBAR_H;
    int content_h = c->height - TABBAR_H - TOOLBAR_H - STATUSBAR_H;
    uint32_t cbg = t->is_dark ? 0x0D1117 : 0xFFFFFF;
    comp_rect(c->origin_x, content_y, c->width, content_h, cbg);

    const char* content = get_page_content(tabs[curtab].url);
    int px = c->origin_x + 20, py = content_y + 16;
    uint32_t col = t->text;
    for (const char* p = content; *p && py < content_y + content_h - 16; p++) {
        char ch = *p;
        if (ch == '\n') { px = c->origin_x + 20; py += 18; col = t->text; continue; }
        /* detect section headers (ALL CAPS lines) */
        if (px == c->origin_x + 20) {
            int all_caps = 1;
            const char* q = p;
            while (*q && *q != '\n') {
                if (*q >= 'a' && *q <= 'z') { all_caps = 0; break; }
                q++;
            }
            col = all_caps ? t->accent_hi : t->text;
        }
        if (px > c->origin_x + c->width - 20) { px = c->origin_x + 36; py += 18; }
        comp_glyph(px, py, ch, col, cbg);
        px += 8;
    }

    /* ---- status bar ---- */
    int sty = content_y + content_h;
    uint32_t stbg = t->is_dark ? 0x161B22 : 0xE4E8F0;
    comp_rect(c->origin_x, sty, c->width, STATUSBAR_H, stbg);
    comp_rect(c->origin_x, sty, c->width, 1, t->win_border);
    comp_text(c->origin_x + 8, sty + 4,
              loading ? "Loading..." : "VyroBrowser 2.0  |  Vyro OS Secure",
              t->text_dim, stbg);
    (void)abs_my;
}

const app_def_t APP_BROWSER2 = { "Browser", 'W', 0x40C0E0, render_browser, 720, 600 };
