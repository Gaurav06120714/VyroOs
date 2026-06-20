#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../vfs.h"
#include "../security.h"
#include "../heap.h"
#include "../pmm.h"
#include "../../drivers/timer.h"
#include "../../drivers/rtc.h"

#define MAX_LINES    48
#define MAX_LINELEN  80
#define HIST_SIZE    32
#define LINE_H       16

static char  buf[MAX_LINES][MAX_LINELEN + 2];
static int   nlines   = 0;
static char  inp[MAX_LINELEN + 2];
static int   inplen   = 0;
static int   initted  = 0;
static int   scroll   = 0;

static char  hist[HIST_SIZE][MAX_LINELEN + 2];
static int   hist_n   = 0;
static int   hist_pos = -1;

static vfs_node_t* term_cwd = 0;

static void scpy(char* d, const char* s, int max) {
    int i = 0; while (s[i] && i < max-1) { d[i]=s[i]; i++; } d[i]=0;
}
static int slen(const char* s) { int i=0; while(s[i]) i++; return i; }
static int seq(const char* a, const char* b) {
    while (*a && *b && *a==*b){a++;b++;} return *a==*b;
}

static void push(const char* s, uint8_t is_err) {
    (void)is_err;
    if (nlines >= MAX_LINES) {
        for (int i = 1; i < MAX_LINES; i++) scpy(buf[i-1], buf[i], MAX_LINELEN+2);
        nlines = MAX_LINES - 1;
    }
    scpy(buf[nlines++], s, MAX_LINELEN+2);
    scroll = 0;
}

static void push_prompt() {
    char p[MAX_LINELEN+2];
    char path[64];
    if (term_cwd) vfs_full_path(term_cwd, path, sizeof(path));
    else { path[0]='/'; path[1]=0; }
    const char* u = current_user();
    int i=0;
    for(int j=0;u[j]&&i<24;j++) p[i++]=u[j];
    p[i++]='@'; p[i++]='v'; p[i++]='y'; p[i++]='r'; p[i++]='o';
    p[i++]=':';
    for(int j=0;path[j]&&i<MAX_LINELEN-4;j++) p[i++]=path[j];
    p[i++]='$'; p[i++]=' '; p[i]=0;
    push(p, 0);
}

static void num_to_str(char* out, uint64_t v) {
    if (v == 0) { out[0]='0'; out[1]=0; return; }
    char tmp[24]; int n=0;
    while (v) { tmp[n++]='0'+(v%10); v/=10; }
    for (int i=0;i<n;i++) out[i]=tmp[n-1-i]; out[n]=0;
}

static void run(const char* cmd) {
    if (!cmd || !cmd[0]) return;

    if (hist_n < HIST_SIZE) {
        scpy(hist[hist_n++], cmd, MAX_LINELEN+2);
    } else {
        for (int i=1;i<HIST_SIZE;i++) scpy(hist[i-1],hist[i],MAX_LINELEN+2);
        scpy(hist[HIST_SIZE-1], cmd, MAX_LINELEN+2);
    }
    hist_pos = -1;

    char line[MAX_LINELEN+2];
    line[0]='$'; line[1]=' ';
    int li=2; const char* c=cmd;
    while(*c && li<MAX_LINELEN) line[li++]=*c++;
    line[li]=0;
    push(line, 0);

    if (seq(cmd,"help")) {
        push("  ls pwd cd cat echo uname uptime whoami", 0);
        push("  mem clear history version date exit", 0);
    } else if (seq(cmd,"clear")) {
        nlines=0;
    } else if (seq(cmd,"whoami")) {
        push(current_user(), 0);
    } else if (seq(cmd,"version") || seq(cmd,"uname")) {
        push("Vyro OS 2.0.0  x86_64  custom kernel", 0);
    } else if (seq(cmd,"date")) {
        rtc_time_t rt; rtc_read(&rt);
        char d[40];
        int i=0;
        d[i++]='2'; d[i++]='0'; d[i++]='2';d[i++]='6';d[i++]='-';
        d[i++]='0'+(rt.month/10); d[i++]='0'+(rt.month%10); d[i++]='-';
        d[i++]='0'+(rt.day/10);   d[i++]='0'+(rt.day%10);   d[i++]=' ';
        d[i++]='0'+(rt.hour/10);  d[i++]='0'+(rt.hour%10);  d[i++]=':';
        d[i++]='0'+(rt.minute/10);d[i++]='0'+(rt.minute%10);d[i++]=':';
        d[i++]='0'+(rt.second/10);d[i++]='0'+(rt.second%10);d[i]=0;
        push(d, 0);
    } else if (seq(cmd,"uptime")) {
        uint64_t ms = timer_uptime_ms();
        uint64_t s2=ms/1000, m2=s2/60, h2=m2/60;
        char u2[40]; int i=0;
        char tmp[16]; num_to_str(tmp,h2); int l=slen(tmp);
        for(int j=0;j<l;j++) u2[i++]=tmp[j];
        u2[i++]='h'; u2[i++]=' ';
        num_to_str(tmp,m2%60); l=slen(tmp);
        for(int j=0;j<l;j++) u2[i++]=tmp[j];
        u2[i++]='m'; u2[i++]=' ';
        num_to_str(tmp,s2%60); l=slen(tmp);
        for(int j=0;j<l;j++) u2[i++]=tmp[j];
        u2[i++]='s'; u2[i]=0;
        push(u2, 0);
    } else if (seq(cmd,"mem")) {
        uint64_t free_p = pmm_free_pages();
        uint64_t total_p = pmm_total_pages();
        char m[64]; int i=0;
        char tmp[16];
        num_to_str(tmp, (total_p - free_p) * 4); int l=slen(tmp);
        for(int j=0;j<l;j++) m[i++]=tmp[j];
        m[i++]='K'; m[i++]='/';
        num_to_str(tmp, total_p * 4); l=slen(tmp);
        for(int j=0;j<l;j++) m[i++]=tmp[j];
        m[i++]='K'; m[i++]=' '; m[i++]='(';
        num_to_str(tmp, free_p * 4); l=slen(tmp);
        for(int j=0;j<l;j++) m[i++]=tmp[j];
        m[i++]='K'; m[i++]=' '; m[i++]='f'; m[i++]='r'; m[i++]='e'; m[i++]='e'; m[i++]=')'; m[i]=0;
        push(m, 0);
    } else if (seq(cmd,"pwd")) {
        char path[80];
        if (term_cwd) vfs_full_path(term_cwd, path, sizeof(path));
        else { path[0]='/'; path[1]=0; }
        push(path, 0);
    } else if (seq(cmd,"ls")) {
        if (!term_cwd) term_cwd = vfs_root();
        vfs_node_t* n = term_cwd->first_child;
        if (!n) { push("  (empty)", 0); }
        else {
            char row[MAX_LINELEN+2]; int ri=0; int cnt=0;
            while (n) {
                char tmp2[24]; int tl=0;
                if (n->type==VFS_DIRECTORY) tmp2[tl++]='[';
                for(int j=0;n->name[j]&&tl<20;j++) tmp2[tl++]=n->name[j];
                if (n->type==VFS_DIRECTORY) tmp2[tl++]=']';
                tmp2[tl]=0;
                if (ri + tl + 2 > MAX_LINELEN - 2) {
                    row[ri]=0; push(row,0); ri=0;
                }
                row[ri++]=' ';
                for(int j=0;tmp2[j];j++) row[ri++]=tmp2[j];
                cnt++; n=n->next_sibling;
            }
            if (ri>0) { row[ri]=0; push(row,0); }
            if (!cnt) push("  (empty)",0);
        }
    } else if (cmd[0]=='c' && cmd[1]=='d' && (cmd[2]==' '||cmd[2]==0)) {
        const char* arg = (cmd[2]==' ') ? cmd+3 : 0;
        if (!arg || arg[0]==0 || (arg[0]=='/'&&arg[1]==0)) {
            term_cwd = vfs_root();
        } else if (arg[0]=='.' && arg[1]=='.' && arg[2]==0) {
            if (term_cwd && term_cwd->parent) term_cwd = term_cwd->parent;
        } else {
            if (!term_cwd) term_cwd = vfs_root();
            vfs_node_t* n2 = term_cwd->first_child;
            int found=0;
            while (n2) {
                if (n2->type==VFS_DIRECTORY) {
                    int k=0; const char* nm=n2->name;
                    while(arg[k]&&nm[k]&&arg[k]==nm[k]) k++;
                    if (!arg[k]&&!nm[k]) { term_cwd=n2; found=1; break; }
                }
                n2=n2->next_sibling;
            }
            if (!found) push("cd: no such directory", 1);
        }
    } else if (cmd[0]=='e'&&cmd[1]=='c'&&cmd[2]=='h'&&cmd[3]=='o'&&(cmd[4]==' '||cmd[4]==0)) {
        push(cmd[4]==' ' ? cmd+5 : "", 0);
    } else if (cmd[0]=='c'&&cmd[1]=='a'&&cmd[2]=='t'&&cmd[3]==' ') {
        const char* fname = cmd+4;
        if (!term_cwd) term_cwd=vfs_root();
        vfs_node_t* n3=term_cwd->first_child;
        int found2=0;
        while(n3){
            if(n3->type==VFS_FILE){
                int k=0; const char* nm=n3->name;
                while(fname[k]&&nm[k]&&fname[k]==nm[k]) k++;
                if(!fname[k]&&!nm[k]){
                    found2=1;
                    if(n3->content&&n3->size>0){
                        const char* data=(const char*)n3->content;
                        int pos=0;
                        while(pos<(int)n3->size){
                            char row2[MAX_LINELEN+2]; int ri2=0;
                            while(pos<(int)n3->size&&data[pos]!='\n'&&ri2<MAX_LINELEN)
                                row2[ri2++]=data[pos++];
                            if(pos<(int)n3->size&&data[pos]=='\n') pos++;
                            row2[ri2]=0; push(row2,0);
                        }
                    } else push("(empty file)",0);
                    break;
                }
            }
            n3=n3->next_sibling;
        }
        if(!found2) push("cat: file not found",1);
    } else if (seq(cmd,"history")) {
        for(int i=0;i<hist_n;i++){
            char hl[MAX_LINELEN+6]; int hi=0;
            hl[hi++]=' ';
            char ni[4]; num_to_str(ni,i+1);
            for(int j=0;ni[j];j++) hl[hi++]=ni[j];
            hl[hi++]=' '; hl[hi++]=' ';
            for(int j=0;hist[i][j]&&hi<MAX_LINELEN;j++) hl[hi++]=hist[i][j];
            hl[hi]=0; push(hl,0);
        }
    } else if (seq(cmd,"exit")) {
        push("(type 'gui' in shell to return to desktop)", 0);
    } else {
        char err[MAX_LINELEN+2];
        int ei=0;
        err[ei++]='\'';
        for(int j=0;cmd[j]&&ei<MAX_LINELEN-8;j++) err[ei++]=cmd[j];
        err[ei++]='\''; err[ei++]=':'; err[ei++]=' ';
        err[ei++]='n'; err[ei++]='o'; err[ei++]='t'; err[ei++]=' ';
        err[ei++]='f'; err[ei++]='o'; err[ei++]='u'; err[ei++]='n'; err[ei++]='d';
        err[ei]=0;
        push(err,1);
    }
    push_prompt();
}

static void render_terminal(app_ctx_t* c) {
    if (!initted) {
        term_cwd = vfs_root();
        push("Vyro OS Terminal  v2.0", 0);
        push("Type 'help' for available commands.", 0);
        push("", 0);
        push_prompt();
        initted = 1;
    }

    int visible_lines = (c->height - 28) / LINE_H;
    int total = nlines + 1;
    int max_scroll = total > visible_lines ? total - visible_lines : 0;
    if (scroll > max_scroll) scroll = max_scroll;

    comp_rect(c->origin_x, c->origin_y, c->width, c->height, 0x0D1117);

    comp_rect(c->origin_x, c->origin_y, c->width, 24, 0x161B22);
    comp_rect(c->origin_x, c->origin_y + 23, c->width, 1, 0x30363D);
    comp_text(c->origin_x + 10, c->origin_y + 5, "Terminal  --  Vyro OS", 0x8B949E, 0x161B22);

    int start = scroll;
    int oy = c->origin_y + 28;
    for (int i = start; i < nlines && (oy - c->origin_y - 28) < c->height - 44; i++) {
        uint32_t col = 0xA0FF90;
        if (buf[i][0]=='$'&&buf[i][1]==' ') col = 0x79C0FF;
        else if (buf[i][0]=='\'') col = 0xFF7B72;
        comp_text_bg_alpha(c->origin_x + 8, oy, buf[i], col);
        oy += LINE_H;
    }

    int iy = oy;
    if (iy < c->origin_y + c->height - 20) {
        comp_text_bg_alpha(c->origin_x + 8, iy, "> ", 0x58A6FF);
        comp_text_bg_alpha(c->origin_x + 24, iy, inp, 0xE6EDF3);
        uint64_t t = timer_ticks();
        if ((t / 30) % 2 == 0)
            comp_rect(c->origin_x + 24 + inplen * 8, iy, 7, 14, 0xE6EDF3);
    }

    if (c->key) {
        if (c->key == '\n') {
            char tmp3[MAX_LINELEN+2]; scpy(tmp3, inp, MAX_LINELEN+2);
            inplen = 0; inp[0] = 0; hist_pos = -1;
            run(tmp3);
        } else if (c->key == '\b') {
            if (inplen > 0) inp[--inplen] = 0;
        } else if (c->key == 0x11) {
            if (hist_pos < hist_n - 1) {
                hist_pos++;
                scpy(inp, hist[hist_n - 1 - hist_pos], MAX_LINELEN+2);
                inplen = slen(inp);
            }
        } else if (c->key == 0x12) {
            if (hist_pos > 0) {
                hist_pos--;
                scpy(inp, hist[hist_n - 1 - hist_pos], MAX_LINELEN+2);
                inplen = slen(inp);
            } else if (hist_pos == 0) {
                hist_pos = -1; inp[0]=0; inplen=0;
            }
        } else if (c->key >= 32 && c->key < 127 && inplen < MAX_LINELEN) {
            inp[inplen++] = (char)c->key; inp[inplen] = 0;
        }
    }
}

const app_def_t APP_TERMINAL = { "Terminal", '>', 0x40FF60, render_terminal, 640, 440 };
