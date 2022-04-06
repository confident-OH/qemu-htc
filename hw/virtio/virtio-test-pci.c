#include "qemu/osdep.h"

#include "virtio-pci.h"
#include "hw/qdev-properties.h"
#include "hw/virtio/virtio-test.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/error-report.h"

typedef struct VirtIOTestPCI VirtIOTestPCI;
/*
 * virtio-test-pci: This extends VirtioPCIProxy.
 */
#define TYPE_VIRTIO_TEST_PCI "virtio-test-pci-base"
#define VIRTIO_TEST_PCI(obj) \
        OBJECT_CHECK(VirtIOTestPCI, (obj), TYPE_VIRTIO_TEST_PCI)

struct VirtIOTestPCI {
    VirtIOPCIProxy parent_obj;
    VirtIOTest vdev;
};

/* virtio-test-pci */
static Property virtio_test_pci_properties[] = {
    DEFINE_PROP_UINT32("class", VirtIOPCIProxy, class_code, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void virtio_test_pci_realize(VirtIOPCIProxy *vpci_dev, Error **errp)
{
    VirtIOTestPCI *dev = VIRTIO_TEST_PCI(vpci_dev);
    DeviceState *vdev = DEVICE(&dev->vdev);

    if (vpci_dev->class_code != PCI_CLASS_OTHERS &&
        vpci_dev->class_code != PCI_CLASS_MEMORY_RAM) { /* qemu < 1.1 */
        vpci_dev->class_code = PCI_CLASS_OTHERS;
    }

    qdev_set_parent_bus(vdev, BUS(&vpci_dev->bus));
    object_property_set_bool(OBJECT(vdev), true, "realized", errp);
}

static void virtio_test_pci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioPCIClass *k = VIRTIO_PCI_CLASS(klass);
    PCIDeviceClass *pcidev_k = PCI_DEVICE_CLASS(klass);
    k->realize = virtio_test_pci_realize;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    dc->props = virtio_test_pci_properties;
    pcidev_k->vendor_id = PCI_VENDOR_ID_REDHAT_QUMRANET;
    pcidev_k->device_id = PCI_DEVICE_ID_VIRTIO_TEST;
    pcidev_k->revision = VIRTIO_PCI_ABI_VERSION;
    pcidev_k->class_id = PCI_CLASS_OTHERS;
}

static void virtio_test_pci_instance_init(Object *obj)
{
    VirtIOTestPCI *dev = VIRTIO_TEST_PCI(obj);

    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_TEST);
}

static const VirtioPCIDeviceTypeInfo virtio_test_pci_info = {
    .base_name             = TYPE_VIRTIO_TEST_PCI,
    .generic_name          = "virtio-balloon-pci",
    .instance_size = sizeof(VirtIOTestPCI),
    .instance_init = virtio_test_pci_instance_init,
    .class_init    = virtio_test_pci_class_init,
};

static void virtio_test_pci_register(void)
{
    error_printf("zyq start register virtio_pci\n");
    virtio_pci_types_register(&virtio_test_pci_info);
}

type_init(virtio_test_pci_register)