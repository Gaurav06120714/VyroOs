/*
 * vyro-init — Vyro PID 1
 *
 * The world's smallest service supervisor. Does the absolute minimum to
 * get a Linux system to the point where vyro-compositor can take over
 * the screen:
 *
 *   1. Mount essential pseudo-filesystems (/proc, /sys, /dev, /run, /tmp)
 *   2. Set hostname to "vyro"
 *   3. Fork+exec /usr/bin/vyro-compositor
 *   4. Reap zombies; respawn the compositor if it dies
 *
 * Not a systemd replacement. Real init systems handle sockets, cgroups,
 * service dependencies, logging, etc. — none of that here.
 */

#define _GNU_SOURCE
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

static void try_mount(const char *src, const char *dst, const char *fs, unsigned long flags) {
    mkdir(dst, 0755);
    if (mount(src, dst, fs, flags, NULL) < 0 && errno != EBUSY)
        fprintf(stderr, "vyro-init: mount %s on %s: %s\n", fs, dst, strerror(errno));
}

static pid_t spawn_compositor(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("vyro-init: fork");
        return -1;
    }
    if (pid == 0) {
        setsid();
        execl("/usr/bin/vyro-compositor", "vyro-compositor", (char *)NULL);
        perror("vyro-init: exec vyro-compositor");
        _exit(127);
    }
    fprintf(stderr, "vyro-init: spawned compositor pid=%d\n", (int)pid);
    return pid;
}

int main(void) {
    if (getpid() != 1) {
        fprintf(stderr, "vyro-init: must run as PID 1\n");
        return 1;
    }

    /* Block signals; we'll wait() ourselves */
    signal(SIGINT,  SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    try_mount("proc",     "/proc",     "proc",     MS_NOSUID | MS_NOEXEC | MS_NODEV);
    try_mount("sysfs",    "/sys",      "sysfs",    MS_NOSUID | MS_NOEXEC | MS_NODEV);
    try_mount("devtmpfs", "/dev",      "devtmpfs", MS_NOSUID);
    try_mount("tmpfs",    "/run",      "tmpfs",    MS_NOSUID | MS_NODEV);
    try_mount("tmpfs",    "/tmp",      "tmpfs",    MS_NOSUID | MS_NODEV);

    sethostname("vyro", 4);

    fputs("\n  Vyro OS Core — vyro-init online.\n\n", stderr);

    pid_t comp = spawn_compositor();

    for (;;) {
        int status = 0;
        pid_t r = wait(&status);
        if (r < 0) {
            if (errno == ECHILD) {
                sleep(1);
                comp = spawn_compositor();
            }
            continue;
        }
        if (r == comp) {
            fprintf(stderr, "vyro-init: compositor exited (status=%d), respawning in 2s\n", status);
            sleep(2);
            comp = spawn_compositor();
        }
    }
}
