/*
 * libvyro-linux — public API
 *
 * The Linux port of libvyro. Same function signatures as the microkernel
 * libvyro, but the implementation calls into musl/Linux instead of the
 * Vyro int 0x80 syscall table.
 */

#ifndef VYRO_H
#define VYRO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Window --- */
typedef struct vyro_window vyro_window_t;

vyro_window_t *vyro_window_create(const char *title, int w, int h);
void           vyro_window_destroy(vyro_window_t *w);
void           vyro_window_present(vyro_window_t *w);

/* --- Drawing --- */
void vyro_fill(vyro_window_t *w, uint32_t bgrx);
void vyro_rect(vyro_window_t *w, int x, int y, int cx, int cy, uint32_t bgrx);
void vyro_text(vyro_window_t *w, int x, int y, const char *s, uint32_t bgrx);

/* --- Input --- */
typedef enum {
    VYRO_EV_NONE,
    VYRO_EV_KEY_DOWN,
    VYRO_EV_KEY_UP,
    VYRO_EV_MOUSE_MOVE,
    VYRO_EV_MOUSE_DOWN,
    VYRO_EV_MOUSE_UP,
    VYRO_EV_CLOSE,
} vyro_event_kind_t;

typedef struct {
    vyro_event_kind_t kind;
    int x, y;
    int code;
} vyro_event_t;

int vyro_poll(vyro_window_t *w, vyro_event_t *out);

/* --- Lifecycle --- */
int  vyro_init(void);
void vyro_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* VYRO_H */
