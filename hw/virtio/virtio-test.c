#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/iov.h"
#include "qemu/timer.h"
#include "qemu-common.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-test.h"
#include "sysemu/kvm.h"
#include "sysemu/hax.h"
#include "exec/address-spaces.h"
#include "qapi/error.h"
#include "qapi/qapi-events-misc.h"
#include "qapi/visitor.h"
#include "qemu/error-report.h"

#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-access.h"
#include "migration/migration.h"


static void virtio_test_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtQueueElement *elem;

    for (;;) {
        size_t offset = 0;
        uint32_t pfn;
        elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
        if (!elem) {
            return;
        }

        while (iov_to_buf(elem->out_sg, elem->out_num, offset, &pfn, 4) == 4) {
            int p = virtio_ldl_p(vdev, &pfn);

            offset += 4;
            qemu_log("=========get virtio num:%d\n", p);
        }

        virtqueue_push(vq, elem, offset);
        virtio_notify(vdev, vq);
        g_free(elem);
    }
}


static void virtio_test_get_config(VirtIODevice *vdev, uint8_t *config_data)
{
    VirtIOTest *dev = VIRTIO_TEST(vdev);
    struct virtio_test_config config;

    config.actual = cpu_to_le32(dev->actual);
    config.event = cpu_to_le32(dev->event);

    memcpy(config_data, &config, sizeof(struct virtio_test_config));

}

static void virtio_test_set_config(VirtIODevice *vdev,
                                      const uint8_t *config_data)
{
    VirtIOTest *dev = VIRTIO_TEST(vdev);
    struct virtio_test_config config;

    memcpy(&config, config_data, sizeof(struct virtio_test_config));
    dev->actual = le32_to_cpu(config.actual);
    dev->event = le32_to_cpu(config.event);
}

static uint64_t virtio_test_get_features(VirtIODevice *vdev, uint64_t f,
                                            Error **errp)
{
    VirtIOTest *dev = VIRTIO_TEST(vdev);
    f |= dev->host_features;
    virtio_add_feature(&f, VIRTIO_TEST_F_CAN_PRINT);

    return f;
}

static int virtio_test_post_load_device(void *opaque, int version_id)
{
    return 0;
}

static const VMStateDescription vmstate_virtio_test_device = {
    .name = "virtio-test-device",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = virtio_test_post_load_device,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(actual, VirtIOTest),
        VMSTATE_END_OF_LIST()
    },
};

static void test_stats_change_timer(VirtIOTest *s, int64_t secs)
{
    timer_mod(s->stats_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + secs * 1000);
}

static void test_stats_poll_cb(void *opaque)
{
    VirtIOTest *s = opaque;
    VirtIODevice *vdev = VIRTIO_DEVICE(s);

    qemu_log("==============set config:%d\n", s->set_config++);
    virtio_notify_config(vdev);
    test_stats_change_timer(s, 1);
}

static void virtio_test_device_realize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    VirtIOTest *s = VIRTIO_TEST(dev);

    virtio_init(vdev, "virtio-test", VIRTIO_ID_TEST,
                sizeof(struct virtio_test_config));

    s->ivq = virtio_add_queue(vdev, 128, virtio_test_handle_output);

    /* create a new timer */
    g_assert(s->stats_timer == NULL);
    s->stats_timer = timer_new_ms(QEMU_CLOCK_VIRTUAL, test_stats_poll_cb, s);
    test_stats_change_timer(s, 30);
}

static void virtio_test_device_unrealize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);

    virtio_cleanup(vdev);
}

static void virtio_test_device_reset(VirtIODevice *vdev)
{
    return;
}

static void virtio_test_set_status(VirtIODevice *vdev, uint8_t status)
{
    return;
}

static void virtio_test_instance_init(Object *obj)
{
    return;
}

static const VMStateDescription vmstate_virtio_test = {
    .name = "virtio-test",
    .minimum_version_id = 1,
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_VIRTIO_DEVICE,
        VMSTATE_END_OF_LIST()
    },
};

static Property virtio_test_properties[] = {
    {
        .name = NULL
    }
};

static void virtio_test_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);

    dc->props = virtio_test_properties;
    dc->vmsd = &vmstate_virtio_test;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    vdc->realize = virtio_test_device_realize;
    vdc->unrealize = virtio_test_device_unrealize;
    vdc->reset = virtio_test_device_reset;
    vdc->get_config = virtio_test_get_config;
    vdc->set_config = virtio_test_set_config;
    vdc->get_features = virtio_test_get_features;
    vdc->set_status = virtio_test_set_status;
    vdc->vmsd = &vmstate_virtio_test_device;
}

static const TypeInfo virtio_test_info = {
    .name = TYPE_VIRTIO_TEST,
    .parent = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtIOTest),
    .instance_init = virtio_test_instance_init,
    .class_init = virtio_test_class_init,
};

static void virtio_register_types(void)
{
    type_register_static(&virtio_test_info);
}

type_init(virtio_register_types)