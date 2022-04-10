#ifndef QEMU_VIRTIO_TEST_H
#define QEMU_VIRTIO_TEST_H

#include "standard-headers/linux/virtio_test.h"
#include "hw/virtio/virtio.h"
#include "sysemu/iothread.h"

#define TYPE_VIRTIO_TEST "virtio-test-device"
#define VIRTIO_TEST(obj) \
        OBJECT_CHECK(VirtIOTest, (obj), TYPE_VIRTIO_TEST)

typedef struct HtcZyqData
{
    /* data */
    int64_t id;
    char htc_str[256];
}HtcZyqData;

typedef struct HtcReturnHost
{
    union
    {
        HtcZyqData htc_command;
    };
}HtcReturnHost;

typedef struct VirtIOTest {
    VirtIODevice parent_obj;
    VirtQueue *ivq, *rvq;
    VirtQueueElement *stats_vq_elem;
    uint32_t set_config; // test
    uint32_t actual;  // test
    size_t stats_vq_offset;
    QEMUTimer *stats_timer;
    uint32_t host_features;
    uint32_t event;   // test
    HtcZyqData set_data;
} VirtIOTest;




#endif