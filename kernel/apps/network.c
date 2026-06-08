#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../net.h"
#include "../dhcp.h"
#include "../dns.h"
#include "../pci.h"
#include "../sockets.h"
#include "../notify.h"

static int wifi_on = 1, vpn_on = 0;
static int tab = 0;

static void render_network(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;


    const char* tabs[] = { "Status", "Wi-Fi", "Diagnostics" };
    for (int i = 0; i < 3; i++) {
        int tx = c->origin_x + 20 + i * 100;
        if (w_button(tx, c->origin_y + 10, 90, 28, tabs[i], abs_mx, abs_my, c->clicked))
            tab = i;
        if (tab == i)
            for (int u = 0; u < 90; u++)
                comp_pixel(tx + u, c->origin_y + 42, t->accent_hi);
    }

    int y = c->origin_y + 60;
    if (tab == 0) {

        comp_text(c->origin_x + 20, y, "Wi-Fi", t->text, t->win_body);
        wifi_on = w_toggle(c->origin_x + c->width - 70, y - 2, wifi_on, abs_mx, abs_my, c->clicked);
        y += 30;
        comp_text(c->origin_x + 20, y, "VPN", t->text, t->win_body);
        vpn_on = w_toggle(c->origin_x + c->width - 70, y - 2, vpn_on, abs_mx, abs_my, c->clicked);
        y += 40;
        w_separator(c->origin_x + 20, y, c->width - 40); y += 12;


        const uint8_t* ip = dhcp_offered_ip();
        const uint8_t* gw = dhcp_gateway();
        const uint8_t* dn = dhcp_dns();
        const uint8_t* mac = net_mac();
        char buf[80];

        int p = 0;
        for (int i = 0; i < 4; i++) {
            int v = ip[i]; char tmp[4]; int n = 0;
            if (v == 0) tmp[n++] = '0';
            else { char rev[4]; int r=0; while(v){rev[r++]='0'+v%10;v/=10;} while(r) tmp[n++]=rev[--r]; }
            for (int j = 0; j < n; j++) buf[p++] = tmp[j];
            if (i < 3) buf[p++] = '.';
        } buf[p] = 0;
        comp_text(c->origin_x + 20, y, "IP", t->text_dim, t->win_body);
        comp_text(c->origin_x + 120, y, buf, t->text, t->win_body); y += 24;


        p = 0;
        for (int i = 0; i < 4; i++) {
            int v = gw[i]; char tmp[4]; int n = 0;
            if (v == 0) tmp[n++] = '0';
            else { char rev[4]; int r=0; while(v){rev[r++]='0'+v%10;v/=10;} while(r) tmp[n++]=rev[--r]; }
            for (int j = 0; j < n; j++) buf[p++] = tmp[j];
            if (i < 3) buf[p++] = '.';
        } buf[p] = 0;
        comp_text(c->origin_x + 20, y, "Gateway", t->text_dim, t->win_body);
        comp_text(c->origin_x + 120, y, buf, t->text, t->win_body); y += 24;


        p = 0;
        for (int i = 0; i < 4; i++) {
            int v = dn[i]; char tmp[4]; int n = 0;
            if (v == 0) tmp[n++] = '0';
            else { char rev[4]; int r=0; while(v){rev[r++]='0'+v%10;v/=10;} while(r) tmp[n++]=rev[--r]; }
            for (int j = 0; j < n; j++) buf[p++] = tmp[j];
            if (i < 3) buf[p++] = '.';
        } buf[p] = 0;
        comp_text(c->origin_x + 20, y, "DNS", t->text_dim, t->win_body);
        comp_text(c->origin_x + 120, y, buf, t->text, t->win_body); y += 24;


        const char* hx = "0123456789ABCDEF";
        p = 0;
        for (int i = 0; i < 6; i++) {
            buf[p++] = hx[mac[i]>>4]; buf[p++] = hx[mac[i]&0xF];
            if (i < 5) buf[p++] = ':';
        } buf[p] = 0;
        comp_text(c->origin_x + 20, y, "MAC", t->text_dim, t->win_body);
        comp_text(c->origin_x + 120, y, buf, t->text, t->win_body);
    } else if (tab == 1) {

        const char* nets[] = { "Vyro-Home", "OfficeWiFi", "GuestNet", "Phone Hotspot" };
        int sig[]    = { 90, 70, 50, 30 };
        for (int i = 0; i < 4; i++) {
            int row_y = y + i * 40;
            comp_rect(c->origin_x + 20, row_y, c->width - 40, 36, t->dock_bg);
            comp_border(c->origin_x + 20, row_y, c->width - 40, 36, t->win_border);
            comp_text(c->origin_x + 32, row_y + 10, nets[i], t->text, t->dock_bg);
            w_progress(c->origin_x + c->width - 150, row_y + 12, 100, 12, sig[i]);
            if (w_button(c->origin_x + c->width - 240, row_y + 4, 80, 28,
                         (i == 0) ? "Connected" : "Connect",
                         abs_mx, abs_my, c->clicked) && i != 0)
                notify_post("Wi-Fi", nets[i]);
        }
    } else {

        comp_text(c->origin_x + 20, y, "Active sockets:", t->text, t->win_body);
        char nbuf[8]; int sn = sock_count();
        if (sn == 0) nbuf[0] = '0', nbuf[1] = 0;
        else { int p=0; char r[4]; int rn=0; int v=sn; while(v){r[rn++]='0'+v%10;v/=10;}
               while(rn) nbuf[p++]=r[--rn]; nbuf[p]=0; }
        comp_text(c->origin_x + 160, y, nbuf, t->accent_hi, t->win_body); y += 24;
        comp_text(c->origin_x + 20, y, "PCI NIC:", t->text, t->win_body);
        pci_device_t* nic = pci_find_network();
        if (nic) {
            char b[40]; const char* hx = "0123456789ABCDEF";
            b[0]='0'; b[1]='x';
            for (int i = 0; i < 4; i++) b[2+i] = hx[(nic->vendor_id >> (12 - i*4)) & 0xF];
            b[6]=':'; b[7]='0'; b[8]='x';
            for (int i = 0; i < 4; i++) b[9+i] = hx[(nic->device_id >> (12 - i*4)) & 0xF];
            b[13]=0;
            comp_text(c->origin_x + 160, y, b, t->text, t->win_body);
        } else {
            comp_text(c->origin_x + 160, y, "none", t->text_dim, t->win_body);
        }
        y += 30;
        if (w_button(c->origin_x + 20, y, 120, 28, "Test DNS",
                     abs_mx, abs_my, c->clicked)) {
            uint8_t ip[4];
            if (dns_resolve("github.com", ip) == 0)
                notify_post("DNS", "github.com resolved");
            else
                notify_post("DNS", "lookup failed");
        }
    }
}

const app_def_t APP_NETWORK = { "Network", 'I', 0x40C0E0, render_network, 540, 400 };
