#ifndef PROBE_H
#define PROBE_H

#include <stdint.h>
#include <stddef.h>
#include <libusb-1.0/libusb.h>

typedef enum {
    PROBE_UNKNOWN,
    PROBE_NULINK1,
    PROBE_COUNT
} probe_t;

struct probe {
    probe_t type;
    /* libusb */
    libusb_device *dev;
    libusb_context *usb;
    libusb_device_handle *handle;
    /* protocol */
    int seqno;
};

struct nulink1_version_info {
    uint32_t firmware_version;
    uint32_t product_id;
    uint32_t flags;
} __attribute__((__packed__));

#define PRODUCT_ID_NULINKME 0x550501
#define PRODUCT_ID_NULINK1  0x012009

struct nulink1_config {
    uint32_t command;
    uint32_t clock;
    uint32_t chip_family;
    uint32_t voltage;
    uint32_t power_target;
    uint32_t usb_func_e;
} __attribute__((__packed__));

#define CHIP_FAMILY_M2351  0x321
#define CHIP_FAMILY_1T8051 0x800

#define RESET_TYPE_AUTO             0
#define RESET_TYPE_HW               1
#define RESET_TYPE_SYS_RESET_REQ    2
#define RESET_TYPE_VEC_RESET        3
#define RESET_TYPE_FAST_RESCUE      4
#define RESET_TYPE_NONE_NULINK      5
#define RESET_TYPE_NONE2_8051T1ONLY 6

#define RESET_CONN_TYPE_NORMAL     0
#define RESET_CONN_TYPE_PRERESET   1
#define RESET_CONN_TYPE_UNDERRESET 2
#define RESET_CONN_TYPE_NONE       3
#define RESET_CONN_TYPE_DISCONNECT 4
#define RESET_CONN_TYPE_ICPMODE    5

#define RESET_MODE_EXTMODE 0
#define RESET_MODE_MODE1   1

#define DEVICE_ID_N76E003 0xda3650
#define DEVICE_ID_N76E616 0xda2f50
#define DEVICE_ID_MS51FB  0xda4b21

#define MEMORY_SPACE_PROGRAM 0x0
#define MEMORY_SPACE_CONFIG  0x3

int probe_init(struct probe *ctx);
void probe_release(struct probe *ctx);

int probe_get_version(struct probe *ctx,
                      struct nulink1_version_info *rinfo);
int probe_set_config(struct probe *ctx,
                     const struct nulink1_config *nulink1_config);
int probe_reset(struct probe *ctx, int reset_type, int reset_conn_type,
                int reset_mode);
int probe_get_device_id(struct probe *ctx, uint32_t * rid);
int probe_erase_flash_chip(struct probe *ctx);
int probe_write_memory(struct probe *ctx, int address, int memory_space,
                       const void *data, size_t n);
int probe_unknown_a5(struct probe *ctx);

#endif
