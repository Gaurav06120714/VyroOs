#ifndef NOTIFY_H
#define NOTIFY_H

#include "../include/types.h"

#define MAX_NOTIFICATIONS 8

typedef struct {
    char     title[48];
    char     body[80];
    uint64_t shown_at_tick;
    uint8_t  alive;
} notification_t;

void  notify_post(const char* title, const char* body);
void  notify_tick();                  // expire old notifications
int   notify_active(notification_t** out);
void  notify_history(notification_t** out, int* count);

#endif
