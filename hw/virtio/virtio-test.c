#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/iov.h"
#include "qemu/timer.h"
#include "qemu-common.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-test.h"
#include "sysemu/kvm.h"
#include "sysemu/hax.h"
#include "sysemu/htc_zyq.h"
#include "exec/address-spaces.h"
#include "qapi/error.h"
#include "qapi/qapi-events-misc.h"
#include "qapi/visitor.h"
#include "qemu/error-report.h"
#include "monitor/monitor.h"
#include <sys/ipc.h>
#include <sys/sem.h>

#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-access.h"
#include "migration/migration.h"

/*
union HTCSendSem {
    int              val;    
    struct semid_ds *buf;    
    unsigned short  *array;  
    struct seminfo  *__buf;  
};
*/

// int HTC_sem_id;

int64_t HTC_return_qmp;

/*
static int create_Sem(int key, int size)
{
	int id;
	id = semget(key, size, IPC_CREAT|0666);
	if(id < 0) {                          
		qemu_log("create sem %d,%d error\n", key, size);
	}
	return id;
}

static void destroy_Sem(int semid)
{
	int res = semctl(semid,0,IPC_RMID,0);
	if (res < 0) {
		qemu_log("destroy sem %d error\n", semid);
	}
	return;
}

static void set_N(int semid, int index, int n)
{
	union HTCSendSem semopts; 
	semopts.val = n;//设定信号量的初值为n
	semctl(semid,index,SETVAL,semopts);//初始化信号量，信号量编号为index
	return;
}

static void HTCSendP(int semid, int index)
{
	struct sembuf sem;//信号量操作数组
	sem.sem_num = index;//信号量编号
	sem.sem_op = -1;//信号量操作，-1为P操作
	sem.sem_flg = 0;//操作标记：0或IPC_NOWAIT等
	semop(semid,&sem,1);//1:表示执行命令的个数
	return;
}

static void HTCSendV(int semid, int index)
{
	struct sembuf sem;//信号量操作数组
	sem.sem_num = index;//信号量编号
	sem.sem_op =  1;//信号量操作，1为V操作
	sem.sem_flg = 0; //操作标记
    semop(semid,&sem,1);//1:表示执行命令的个数
	return;
}
*/

static void virtio_htc_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtIOTest *s = VIRTIO_TEST(vdev);
    VirtQueueElement *elem;
    //qemu_log("zyq start virtio_htc_handle_output\n");

    for (;;) {
        HtcZyqData item;
        size_t retSize = 0;
        elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
        if (!elem) {
            //qemu_log("virtio_htc_handle_output finished\n");
            return;
        }

        item.id = s->set_data.id;
        strcpy(item.htc_str, s->set_data.htc_str);
        retSize = iov_from_buf(elem->in_sg, elem->in_num, 0, &item, sizeof(HtcZyqData));
        if (retSize != sizeof(HtcZyqData)) {
            qemu_log("error iov_from_buf in_num: %u retSize: %lu, but i want to send id: %ld, str: %s\n", elem->in_num, retSize, item.id, item.htc_str);
        }

        virtqueue_push(vq, elem, sizeof(HtcZyqData));
        //qemu_log("htc pushed queue\n");
        virtio_notify(vdev, vq);
        g_free(elem);
    }
}

static void virtio_htc_handle_status(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtQueueElement *elem;
    HtcMemStatus *ret_mem_p;

    for (;;) {
        HtcReturnHost item;
        size_t retSize = 0;
        elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
        if (!elem) {
            return;
        }

        retSize = iov_to_buf(elem->out_sg, elem->out_num, 0, &item, sizeof(HtcReturnHost));
        if (retSize != sizeof(HtcReturnHost)) {
            qemu_log("error recieve\n");
        }
        else {
            switch (item.id)
            {
            case 1:
            {
                ret_mem_p = &(item.htc_meminfo);
                qemu_log("Total Ram: %lf\n", (double)(ret_mem_p->totalram)/1024/1024);
                qemu_log("Free Ram: %lf\n", (double)(ret_mem_p->freeram)/1024/1024);
                qemu_log("Shared Ram: %lf\n", (double)(ret_mem_p->sharedram)/1024/1024);
                qemu_log("Buffered Ram: %lf\n", (double)(ret_mem_p->bufferram)/1024/1024);
                //qemu_log("Total High Mem: %lu\n", ret_mem_p->totalhigh);
                //qemu_log("Free High Mem: %lu\n", ret_mem_p->freehigh);
                qemu_log("Page Size: %u\n", ret_mem_p->mem_unit);
                break;
            }

            case 3:
            {
                qemu_log("command: %s has been finished\n", item.htc_command.htc_str);
                break;
            }

            case 4:
            {
                qemu_log("page_fault: %ld\n", item.htc_command.id);
                HTC_return_qmp = item.htc_command.id;
                break;
            }

            case 6:
            {
                qemu_log("%s\n", item.htc_command.htc_str);
                break;
            }
            
            default:
            {
                //qemu_log("id: %ld, command: %s has been finished\n", item.id, item.htc_command.htc_str);
                break;
            }
            }
        }
        virtqueue_push(vq, elem, sizeof(HtcReturnHost));
        virtio_notify(vdev, vq);
        g_free(elem);
    }
}

/*
static void* pri_virtio(void* arg)
{
    virtio_notify_config((VirtIODevice *)arg);
    return arg;
}

static void* pri1(void* arg)
{
    return arg;
}
*/

static int64_t virtio_htczyq_send(void *opaque, int64_t id, const char * str)
{
    VirtIOTest *dev = VIRTIO_TEST(opaque);
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);

    //qemu_log("config send......... ");
    dev->set_data.id = id;
    strcpy(dev->set_data.htc_str, str);

    if (id >= 1 && id <= 10) {
        /* now function :
         * 1: search some info of guest
         * 2: execute path
         * 3: execute status
         * 4: module command
         * 5: module status
         * 6: execute info
         */
        //qemu_log("send id: %ld, str: %s\n", id, str);
        virtio_notify_config(vdev);
        return HTC_return_qmp;
    }
    else {
        qemu_log("error id! see help for the command\n");
        return -1;
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

static void virtio_test_device_realize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    VirtIOTest *s = VIRTIO_TEST(dev);
    int ret;

    virtio_init(vdev, "virtio-test", VIRTIO_ID_TEST,
                sizeof(struct virtio_test_config));

    ret = qemu_add_htczyq_handler(virtio_htczyq_send, s);

    if (ret == -1) {
        qemu_log("htc_zyq register failed!\n");
        virtio_cleanup(vdev);
        return;
    }
    else {
        // qemu_log("htc_zyq register success!\n");
    }

    // HTC_sem_id = create_Sem(0, 1);
    // set_N(HTC_sem_id, 0, 0);

    s->ivq = virtio_add_queue(vdev, 1024, virtio_htc_handle_output);
    s->rvq = virtio_add_queue(vdev, 1024, virtio_htc_handle_status);
}

static void virtio_test_device_unrealize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    VirtIOTest *s = VIRTIO_TEST(dev);
    // destroy_Sem(HTC_sem_id);

    qemu_remove_htczyq_handler(s);
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