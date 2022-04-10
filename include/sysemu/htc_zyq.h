#ifndef QEMU_HTC_ZYQ_H
#define QEMU_HTC_ZYQ_H

#include "exec/cpu-common.h"
#include "qapi/qapi-types-misc.h"

typedef void (QEMUHtczyqEvent) (void *opaque, int64_t id, const char * str);

int qemu_add_htczyq_handler(QEMUHtczyqEvent *event_func, void *opaque);
void qemu_remove_htczyq_handler(void *opaque);


#endif