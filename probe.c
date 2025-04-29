#include "probe.h"
#include "verbose.h"

#include <stdio.h>
#include <string.h>

struct nulink1_frame {
    uint8_t seqno;
    uint8_t len;
    uint8_t body[62];
} __attribute__((__packed__));

struct nulink1_reset {
    uint32_t command;
    uint32_t reset_type;
    uint32_t reset_conn_type;
    uint32_t reset_mode;
} __attribute__((__packed__));

struct nulink1_device_id {
    uint32_t command;
    uint32_t device_id;
} __attribute__((__packed__));

struct nulink1_memory {
    uint32_t command;
    uint16_t address;
    uint16_t memory_space;
    uint32_t length;
    uint8_t data[32];
} __attribute__((__packed__));

int probe_init(struct probe *ctx)
{
    int res, n;
    libusb_device **list;

    ctx->type = PROBE_UNKNOWN;
    ctx->dev = NULL;
    ctx->usb = NULL;
    ctx->handle = NULL;

    if ((res = libusb_init(&ctx->usb)) < 0) {
	VERBOSE(LEVEL_ERR, "libusb: can't init context\n");
	return res;
    }

    if ((n = (int) libusb_get_device_list(ctx->usb, &list)) < 0) {
	VERBOSE(LEVEL_ERR, "libusb: can't get device list\n");
	return n;
    }

    while (n-- != 0) {
	libusb_device *device = list[n];
	struct libusb_device_descriptor desc;

	if ((res = libusb_get_device_descriptor(device, &desc)) != 0)
	    continue;

	if (desc.idVendor == 0x0416 && desc.idProduct == 0x511c) {
	    VERBOSE(LEVEL_INFO,
	            "nuvoprog: found NuLink1 compatible device\n");
	    ctx->dev = device;
	    ctx->type = PROBE_NULINK1;
	}
    }

    /* free */
    libusb_free_device_list(list, 1);

    if (ctx->dev == NULL || ctx->type == PROBE_UNKNOWN) {
	VERBOSE(LEVEL_ERR, "libusb: can't find a compatible usb device\n");
	libusb_exit(ctx->usb);
	return -1;
    }

    if ((res = libusb_open(ctx->dev, &ctx->handle)) != 0) {
	VERBOSE(LEVEL_ERR, "libusb: can't open device\n");
	libusb_exit(ctx->usb);
	return -1;
    }

    /* init protocol variables */
    ctx->seqno = 0;

    return 0;
}

void probe_release(struct probe *ctx)
{
    libusb_exit(ctx->usb);
}

static uint8_t probe_seqno(struct probe *ctx)
{
    return (uint8_t) (ctx->seqno = ((ctx->seqno + 1) & 0x7fu));
}

#define EP_IN      0x81
#define EP_OUT     0x02
#define EP_TIMEOUT 1000

static int probe_req(struct probe *ctx, struct nulink1_frame *frame,
                     size_t n)
{
    int xfered = 0;
    unsigned char *frame8 = (void *) frame;

    frame->seqno = probe_seqno(ctx);
    frame->len = (uint8_t) n;

    if (libusb_bulk_transfer
	(ctx->handle, EP_OUT, frame8, sizeof(*frame), &xfered,
	 EP_TIMEOUT) != 0 || xfered != (int) sizeof(*frame)) {
	VERBOSE(LEVEL_ERR,
	        "libusb: bulk req to EP_OUT incomplete or failed (%d)\n",
	        xfered);
	return -1;
    }

    VERBOSE(LEVEL_DBG, "> ");
    for (size_t i = 0; i < sizeof(*frame); i++)
	VERBOSE(LEVEL_DBG, "%02x", (unsigned) frame8[i]);
    VERBOSE(LEVEL_DBG, "\n");

    return xfered;
}

static int probe_resp(struct probe *ctx, struct nulink1_frame *frame)
{
    int xfered = 0;
    unsigned char *frame8 = (void *) frame;

    if (libusb_bulk_transfer
	(ctx->handle, EP_IN, frame8, sizeof(*frame), &xfered,
	 EP_TIMEOUT) != 0 || xfered != (int) sizeof(*frame)) {
	VERBOSE(LEVEL_ERR,
	        "libusb: bulk resp from EP_IN incomplete or failed (%d)\n",
	        xfered);
	return -1;
    }

    VERBOSE(LEVEL_DBG, "< ");
    for (size_t i = 0; i < sizeof(*frame); i++)
	VERBOSE(LEVEL_DBG, "%02x", (unsigned) frame8[i]);
    VERBOSE(LEVEL_DBG, "\n");

    /* check seqno */
    if (frame->seqno != ctx->seqno) {
	VERBOSE(LEVEL_ERR, "probe: errnoeous seqno (%u, expected %u)\n",
	        frame->seqno, ctx->seqno);
	/* error */
	return -1;
    }

    return xfered;
}

int probe_get_version(struct probe *ctx,
                      struct nulink1_version_info *rinfo)
{
    static struct nulink1_frame frame;

    /* request max length */
    memset(frame.body, 0xff, sizeof(frame.body));

    (void) probe_req(ctx, &frame, sizeof(frame.body));
    (void) probe_resp(ctx, &frame);

    /*
     * debug
     */
    struct nulink1_version_info *info =
        (struct nulink1_version_info *) frame.body;
    VERBOSE(LEVEL_INFO, "firmware version: %x product_id %x, flags %x\n",
            info->firmware_version, info->product_id, info->flags);

    memcpy(rinfo, info, sizeof(*rinfo));
    return 0;
}

int probe_set_config(struct probe *ctx,
                     const struct nulink1_config *nulink1_config)
{
    static struct nulink1_frame frame;
    struct nulink1_config *config = (void *) frame.body;

    config->command = (uint32_t) 0xa2;

    if (nulink1_config == NULL) {
	config->clock = (uint32_t) 1000;        /* FIXME */
	config->chip_family = (uint32_t) CHIP_FAMILY_1T8051;
	config->voltage = (uint32_t) 3300;      /* FIXME */
	config->power_target = (uint32_t) 0;    /* FIXME */
	config->usb_func_e = (uint32_t) 0;      /* FIXME */
    } else
	memcpy(config, nulink1_config, sizeof(*config));

    (void) probe_req(ctx, &frame, sizeof(*config));
    (void) probe_resp(ctx, &frame);

    /*
     * debug
     */

    return 0;
}

int probe_reset(struct probe *ctx, int reset_type,
                int reset_conn_type, int reset_mode)
{
    static struct nulink1_frame frame;
    struct nulink1_reset *reset = (void *) frame.body;

    reset->command = (uint32_t) 0xe2;
    reset->reset_type = (uint32_t) reset_type;
    reset->reset_conn_type = (uint32_t) reset_conn_type;
    reset->reset_mode = (uint32_t) reset_mode;

    (void) probe_req(ctx, &frame, sizeof(*reset));
    (void) probe_resp(ctx, &frame);

    /*
     * debug
     */

    return 0;
}

int probe_get_device_id(struct probe *ctx, uint32_t *rid)
{
    static struct nulink1_frame frame;
    struct nulink1_device_id *devid = (void *) frame.body;

    /* request 64bits */
    devid->command = (uint32_t) 0xa3;
    devid->device_id = 0;

    (void) probe_req(ctx, &frame, sizeof(*devid));
    (void) probe_resp(ctx, &frame);

    /*
     * debug
     */
    VERBOSE(LEVEL_DBG, "device id: %x\n", devid->device_id);

    *rid = devid->device_id;
    return 0;
}

int probe_erase_flash_chip(struct probe *ctx)
{
    static struct nulink1_frame frame;

    /* no params */
    *(uint32_t *) frame.body = (uint32_t) 0xa4;

    (void) probe_req(ctx, &frame, sizeof(uint32_t));
    (void) probe_resp(ctx, &frame);

    return 0;
}

int probe_write_memory(struct probe *ctx,
                       int address, int memory_space,
                       const void *data, size_t n)
{
    static struct nulink1_frame frame;
    struct nulink1_memory *memory = (struct nulink1_memory *) frame.body;

    memory->command = (uint32_t) 0xa0;
    memory->address = (uint16_t) address;
    memory->memory_space = (uint16_t) memory_space;
    memory->length = (uint32_t) n;
    memcpy(memory->data, data, n);

    (void) probe_req(ctx, &frame, sizeof(*memory));
    (void) probe_resp(ctx, &frame);

    return 0;
}

int probe_unknown_a5(struct probe *ctx)
{
    static struct nulink1_frame frame;

    /* just push 24 zeroes */
    (void) probe_req(ctx, &frame, (size_t) 24);
    (void) probe_resp(ctx, &frame);

    return 0;
}
