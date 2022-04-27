#include "qemu/osdep.h"
#include "qemu/atomic.h"
#include "sysemu/kvm.h"
#include "sysemu/htc_zyq.h"
#include "trace-root.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-misc.h"
#include "qapi/qmp/qerror.h"

static QEMUHtczyqEvent *htc_event_fn;
static void *htc_opaque;
// static int htc_inhibit_count;

int qemu_add_htczyq_handler(QEMUHtczyqEvent *event_func, void *opaque)
{
    if (htc_event_fn) {
        // has been registered!
        return -1;
    }
    htc_event_fn = event_func;
    htc_opaque = opaque;
    return 0;
}

void qemu_remove_htczyq_handler(void *opaque)
{
    if (htc_opaque != opaque) {
        qemu_log("htc_opaque != opaque\n");
        return;
    }
    htc_event_fn = NULL;
    htc_opaque = NULL;
}

HtcQmpInfo * qmp_htc_zyq(int64_t id, const char *htc_str, Error **errp)
{
    HtcQmpInfo *info = NULL;
    info = g_malloc0(sizeof(*info));
    info->page_fault = htc_event_fn(htc_opaque, id, htc_str);
    info->id = 0;
    info->command = NULL;
    return info;
}
