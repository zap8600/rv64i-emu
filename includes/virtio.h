#ifndef VIRTIO_H
#define VIRTIO_H

#include <stdbool.h>

#define VIRTIO_BASE 0x10001000
#define VIRTIO_SIZE 0x1000

#define VIRTIO_DISK_SIZE 2*1024*1024 // 2 MB disk

#define VIRTIO_IRQ 1

#define VRING_DESC_SIZE 16
#define DESC_NUM 8

#define VIRTIO_MAGIC (VIRTIO_BASE + 0x000)
#define VIRTIO_VERSION (VIRTIO_BASE + 0x004)
#define VIRTIO_DEVICE_ID (VIRTIO_BASE + 0x008)
#define VIRTIO_VENDOR_ID (VIRTIO_BASE + 0x00c)
#define VIRTIO_DEVICE_FEATURES (VIRTIO_BASE + 0x010)
#define VIRTIO_DRIVER_FEATURES (VIRTIO_BASE + 0x020)
#define VIRTIO_GUEST_PAGE_SIZE (VIRTIO_BASE + 0x028)
#define VIRTIO_QUEUE_SEL (VIRTIO_BASE + 0x030)
#define VIRTIO_QUEUE_NUM_MAX (VIRTIO_BASE + 0x034)
#define VIRTIO_QUEUE_NUM (VIRTIO_BASE + 0x038)
#define VIRTIO_QUEUE_PFN (VIRTIO_BASE + 0x040)
#define VIRTIO_QUEUE_NOTIFY (VIRTIO_BASE + 0x050)
#define VIRTIO_STATUS (VIRTIO_BASE + 0x070)

typedef struct VIRTIO {
    uint64_t id;
    uint32_t driver_features;
    uint32_t page_size;
    uint32_t queue_sel;
    uint32_t queue_num;
    uint32_t queue_pfn;
    uint32_t queue_notify;
    uint32_t status;
    uint8_t disk[VIRTIO_DISK_SIZE];
} VIRTIO;

void virtio_init(VIRTIO* virtio);
bool virtio_interrupting(VIRTIO* virtio);
uint64_t virtio_load(VIRTIO* virtio, uint64_t addr, uint64_t size);
void virtio_store(VIRTIO* virtio, uint64_t addr, uint64_t size, uint64_t value);
uint64_t virtio_get_new_id(VIRTIO* virtio);
uint64_t virtio_desc_addr(VIRTIO* virtio);
uint64_t virtio_read_disk(VIRTIO* virtio, uint64_t addr);
void virtio_write_disk(VIRTIO* virtio, uint64_t addr, uint64_t value);

#endif
