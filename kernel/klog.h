#ifndef KLOG_H
#define KLOG_H

#include "../include/types.h"

#define KLOG_MAX 128

void klog(const char* msg);
int  klog_count();
const char* klog_get(int i);

#endif
