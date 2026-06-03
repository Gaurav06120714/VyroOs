/*
 * vyro-init — Vyro PID 1
 *
 * Brings up the Vyro session on Linux:
 *   1. Mount essential pseudo-filesystems (/proc, /sys, /dev, /run, /tmp)
 *   2. Create /run/vyro for the compositor socket
 *   3. Set hostname to "vyro"
 *   4. Spawn /usr/bin/vyro-compositor as the foreground display server
 *   5. After the compositor is listening, spawn each /etc/vyro/session.d/*.cmd
 *      autostart entry as a child
 *   6. Reap zombies, respawn the compositor if it dies, and let autostart
 *      children exit naturally
 *
 * /etc/vyro/session.d/ is a directory of plain-text files where each
 * file's contents is one shell-style command line (path + args). Easy to
 * version-control, easy to extend with a new app drop-in.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SESSION_DIR  "/etc/vyro/session.d"
#define COMPOSITOR   "/usr/bin/vyro-compositor"
#define SOCKET_PATH  "/run/vyro/compositor.sock"
#define MAX_SESSION_KIDS 16

static pid_t g_compositor = -1;
static pid_t g_session_kids[MAX_SESSION_KIDS];
static int   g_session_kid_count = 0;

/* ----- mount helpers ----- */
static void try_mount(const char *src, const char *dst, const char *fs, unsigned long flags) {
    mkdir(dst, 0755);
    if (mount(src, dst, fs, flags, NULL) < 0 && errno != EBUSY)
        fprintf(stderr, "vyro-init: mount %s on %s: %s\n", fs, dst, strerror(errno));
}

/* ----- spawn helpers ----- */
static pid_t spawn_compositor(void) {
    pid_t pid = fork();
    if (pid < 0) { perror("vyro-init: fork compositor"); return -1; }
    if (pid == 0) {
        setsid();
        execl(COMPOSITOR, "vyro-compositor", (char *)NULL);
        perror("vyro-init: exec compositor");
        _exit(127);
    }
    fprintf(stderr, "vyro-init: spawned compositor pid=%d\n", (int)pid);
    return pid;
}

/* Wait until the compositor's listening socket appears (or timeout) so
 * the autostart apps don't race the server. */
static int wait_for_socket(int timeout_secs) {
    for (int i = 0; i < timeout_secs * 10; i++) {
        struct stat st;
        if (stat(SOCKET_PATH, &st) == 0) return 1;
        usleep(100000);
    }
    return 0;
}

/* Tokenize a single command line (whitespace, no quoting for vB.0.9 — drop-in
 * files are expected to be one program path optionally followed by args). */
static int parse_argv(char *line, char *argv_out[], int max_argv) {
    int n = 0;
    char *p = line, *tok;
    while (n < max_argv - 1 && (tok = strsep(&p, " \t\n")) != NULL) {
        if (*tok == '\0') continue;
        argv_out[n++] = tok;
    }
    argv_out[n] = NULL;
    return n;
}

static pid_t spawn_session_entry(const char *cmd_path) {
    char line[1024];
    FILE *f = fopen(cmd_path, "r");
    if (!f) return -1;
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);

    char *argv[16];
    if (parse_argv(line, argv, 16) == 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { perror("vyro-init: fork session"); return -1; }
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "vyro-init: exec %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    fprintf(stderr, "vyro-init: spawned %s pid=%d (from %s)\n", argv[0], (int)pid, cmd_path);
    return pid;
}

static int session_cmp(const void *a, const void *b) {
    const char *const *sa = a, *const *sb = b;
    return strcmp(*sa, *sb);
}

static void spawn_session(void) {
    DIR *d = opendir(SESSION_DIR);
    if (!d) {
        fprintf(stderr, "vyro-init: no %s, skipping autostart\n", SESSION_DIR);
        return;
    }
    /* Collect file names, sort, run in order so session.d/10-foo runs
     * before 20-bar (standard drop-in numeric ordering). */
    char *names[MAX_SESSION_KIDS];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < MAX_SESSION_KIDS) {
        if (e->d_name[0] == '.') continue;
        size_t l = strlen(e->d_name);
        if (l < 4 || strcmp(e->d_name + l - 4, ".cmd") != 0) continue;
        names[n++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof(char *), session_cmp);

    for (int i = 0; i < n; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", SESSION_DIR, names[i]);
        pid_t pid = spawn_session_entry(path);
        if (pid > 0 && g_session_kid_count < MAX_SESSION_KIDS) {
            g_session_kids[g_session_kid_count++] = pid;
        }
        free(names[i]);
    }
}

/* ----- main ----- */
int main(void) {
    if (getpid() != 1) {
        fprintf(stderr, "vyro-init: must run as PID 1\n");
        return 1;
    }

    signal(SIGINT,  SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    try_mount("proc",     "/proc",     "proc",     MS_NOSUID | MS_NOEXEC | MS_NODEV);
    try_mount("sysfs",    "/sys",      "sysfs",    MS_NOSUID | MS_NOEXEC | MS_NODEV);
    try_mount("devtmpfs", "/dev",      "devtmpfs", MS_NOSUID);
    try_mount("tmpfs",    "/run",      "tmpfs",    MS_NOSUID | MS_NODEV);
    try_mount("tmpfs",    "/tmp",      "tmpfs",    MS_NOSUID | MS_NODEV);

    /* vB.0.9: pre-create /run/vyro so the compositor (which mkdirs it
     * with mode 0755) inherits a well-known parent. */
    mkdir("/run/vyro", 0755);

    sethostname("vyro", 4);

    fputs("\n  Vyro OS Core — vyro-init online.\n\n", stderr);

    g_compositor = spawn_compositor();

    if (wait_for_socket(10)) {
        fputs("vyro-init: compositor socket up, launching session\n", stderr);
        spawn_session();
    } else {
        fputs("vyro-init: compositor socket did not appear in 10s; skipping autostart\n", stderr);
    }

    for (;;) {
        int status = 0;
        pid_t r = wait(&status);
        if (r < 0) {
            if (errno == ECHILD) {
                sleep(1);
                g_compositor = spawn_compositor();
                if (wait_for_socket(10)) spawn_session();
            }
            continue;
        }
        if (r == g_compositor) {
            fprintf(stderr, "vyro-init: compositor exited (status=%d), respawning in 2s\n", status);
            sleep(2);
            g_compositor = spawn_compositor();
            if (wait_for_socket(10)) spawn_session();
        } else {
            /* A session child exited — log and let it stay gone. */
            fprintf(stderr, "vyro-init: session child pid=%d exited (status=%d)\n",
                    (int)r, status);
        }
    }
}
