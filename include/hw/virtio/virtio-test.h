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
    long int id;
    char htc_str[256];
}HtcZyqData;

typedef struct HtcMemStatus
{
    unsigned long totalram;	/* Total usable main memory size */
	unsigned long freeram;	/* Available memory size */
	unsigned long sharedram;	/* Amount of shared memory */
	unsigned long bufferram;	/* Memory used by buffers */
	unsigned long totalhigh;	/* Total high memory size */
	unsigned long freehigh;	/* Available high memory size */
	uint32_t mem_unit;			/* Memory unit size in bytes */
}HtcMemStatus;

typedef struct HtcReturnHost
{
    long int id;
    union
    {
        HtcZyqData htc_command;
        HtcMemStatus htc_meminfo;
    };
}HtcReturnHost;

typedef struct VirtIOTest {
    VirtIODevice parent_obj;
    VirtQueue *ivq, *rvq, *evq;
    VirtQueueElement *stats_vq_elem;
    uint32_t set_config; 
    uint32_t actual; 
    size_t stats_vq_offset;
    QEMUTimer *stats_timer;
    uint32_t host_features;
    uint32_t event; 
    HtcZyqData set_data;
} VirtIOTest;




#endif