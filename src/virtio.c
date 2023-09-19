#include <stdint.h>
#include <stdio.h>
#include "../includes/virtio.h"
#include "../includes/cpu.h"
#include "../includes/bus.h"

virtio_init(VIRTIO* virtio) {
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

uint64_t plic_load(VIRTIO* virtio, uint64_t addr, uint64_t size) {
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

void virtio_disk_access(CPU* cpu) {
    uint64_t desc_addr = virtio_desc_addr(&(cpu->bus.virtio));
    uint64_t avail_addr = virtio_desc_addr(&(cpu->bus.virtio)) + 0x40;
    uint64_t used_addr = virtio_desc_addr(&(cpu->bus.virtio)) + 4096;

    uint64_t offset = bus_load(&(cpu->bus), avail_addr + 1, 16);
    uint64_t index = bus_load(&(cpu->bus), avail_addr + (offset % DESC_NUM) + 2, 16);

    uint64_t desc_addr0 = desc_addr + VRING_DESC_SIZE * index;
    uint64_t addr0 = bus_load(&(cpu->bus), desc_addr0, 64);
    uint64_t next0 = bus_load(&(cpu->bus), desc_addr0 + 14, 64);

    uint64_t desc_addr1 = desc_addr + VRING_DESC_SIZE * next0;
    uint64_t addr1 = bus_load(&(cpu->bus), desc_addr0, 64);
    uint64_t len1 = bus_load(&(cpu->bus), desc_addr1 + 8, 32);
    uint64_t flags1 = bus_load(&(cpu->bus), desc_addr1 + 12, 16);

    uint64_t blk_sector = bus_load(&(cpu->bus), addr0 + 8, 64);

    switch ((flags1 & 2) == 0) {
        case true:
            for (uint64_t i = 0; i < len1; i++) {
                uint64_t data = bus_load(&(cpu->bus), addr1 + i, 8);
                virtio_write_disk(&(cpu->bus.virtio), blk_sector * 512 + i, data);
            }
        case false:
            for (uint64_t i = 0; i < len1; i++) {
                uint64_t data = virtio_read_disk(&(cpu->bus.virtio), blk_sector * 512 + 1);
                bus_store(&(cpu->bus), addr1 + i, 8, data);
            }
        default: ;
    }

    uint64_t new_id = virtio_get_new_id(&(cpu->bus.virtio));
    bus_store(&(cpu->bus), used_addr + 2, 16, new_id % 8);
}
