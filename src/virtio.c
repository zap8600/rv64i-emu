#include <stdint.h>
#include <stdio.h>
#include "../includes/virtio.h"

void virtio_init(VIRTIO* virtio) {
    virtio->id = 0;
    virtio->driver_features = 0;
    virtio->page_size = 0;
    virtio->queue_sel = 0;
    virtio->queue_num = 0;
    virtio->queue_pfn = 0;
    virtio->queue_notify = 9999;
    virtio->status = 0;
}

bool virtio_interrupting(VIRTIO* virtio) {
    if (virtio->queue_notify != 9999) {
        virtio->queue_notify = 9999;
        return true;
    }
    return false;
}

uint64_t virtio_load_32(VIRTIO* virtio, uint64_t addr) {
    switch(addr) {
        case VIRTIO_MAGIC: return 0x74726976; break;
        case VIRTIO_VERSION: return 0x1; break;
        case VIRTIO_DEVICE_ID: return 0x2; break;
        case VIRTIO_VENDOR_ID: return 0x554d4551; break;
        case VIRTIO_DEVICE_FEATURES: return 0; break;
        case VIRTIO_DRIVER_FEATURES: return virtio->driver_features; break;
        case VIRTIO_QUEUE_NUM_MAX: return 8; break;
        case VIRTIO_QUEUE_PFN: return (uint64_t)virtio->queue_pfn; break;
        case VIRTIO_STATUS: return (uint64_t)virtio->status; break;
        default: return 0; break;
    }
    return 1;
}

uint64_t virtio_load(VIRTIO* virtio, uint64_t addr, uint64_t size) {
    switch (size) {
        case 32: return virtio_load_32(virtio, addr); break;
        default: ;
    }
    return 1;
}

void virtio_store_32(VIRTIO* virtio, uint64_t addr, uint64_t value) {
    switch(addr) {
        case VIRTIO_DEVICE_FEATURES: virtio->driver_features = value; break;
        case VIRTIO_GUEST_PAGE_SIZE: virtio->page_size = value; break;
        case VIRTIO_QUEUE_SEL: virtio->queue_sel = value; break;
        case VIRTIO_QUEUE_NUM: virtio->queue_num = value; break;
        case VIRTIO_QUEUE_PFN: virtio->queue_pfn = value; break;
        case VIRTIO_QUEUE_NOTIFY: virtio->queue_notify = value; break;
        case VIRTIO_STATUS: virtio->status = value; break;
        default: ;
    }
}

void virtio_store(VIRTIO* virtio, uint64_t addr, uint64_t size, uint64_t value) {
    switch (size) {
        case 32: virtio_store_32(virtio, addr, value); break;
        default: ;
    }
}

uint64_t virtio_get_new_id(VIRTIO* virtio) {
    virtio->id = virtio->id + 1;
    return virtio->id;
}

uint64_t virtio_desc_addr(VIRTIO* virtio) {
    return (uint64_t)virtio->queue_pfn * (uint64_t)virtio->page_size;
}

uint64_t virtio_read_disk(VIRTIO* virtio, uint64_t addr) {
    return (uint64_t)virtio->disk[addr];
}

void virtio_write_disk(VIRTIO* virtio, uint64_t addr, uint64_t value) {
    virtio->disk[addr] = (uint8_t)value;
}
