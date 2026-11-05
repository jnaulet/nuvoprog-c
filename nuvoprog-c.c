#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include "probe.h"
#include "verbose.h"

#include "ihex.h"

int verbose_level = LOG_INFO;
static struct probe probe;

typedef enum {
    COMMAND_NONE,
    COMMAND_RESET,
    COMMAND_ERASE,
    COMMAND_PROGRAM,
    COMMAND_COUNT
} command_t;

typedef enum {
    DEVICE_N76Exxx,             /* 18k */
    DEVICE_MS51xB,              /* 16k */
    DEVICE_MS51xC,              /* 32k */
    DEVICE_COUNT
} device_t;

static int connect_to_target(device_t *dev)
{
    uint32_t device_id = 0;
    struct nulink1_version_info info;

    /* automated requests */
    (void) probe_get_version(&probe, &info);
    /* Setting config {1000 1T8051 3300 0 0} */
    (void) probe_set_config(&probe, NULL);
    /* Performing reset {Auto ICP Mode Ext Mode} */
    (void) probe_reset(&probe, RESET_TYPE_AUTO, RESET_CONN_TYPE_ICPMODE,
                       RESET_MODE_EXTMODE);
    /* Performing reset {None (NuLink) ICP Mode Ext Mode} */
    (void) probe_reset(&probe, RESET_TYPE_NONE_NULINK,
                       RESET_CONN_TYPE_ICPMODE, RESET_MODE_EXTMODE);
    /* Checking device ID */
    if (probe_get_device_id(&probe, &device_id) < 0) {
	VERBOSE(LOG_ERR, "connect_to_target: can't get device ID\n");
	return -(errno = ECOMM);
    }

    if (device_id == (uint32_t) DEVICE_ID_N76E003 ||
        device_id == (uint32_t) DEVICE_ID_N76E616) {
	*dev = DEVICE_N76Exxx;
	return 0;
    }

    if (device_id == (uint32_t) DEVICE_ID_MS51FB) {
	*dev = DEVICE_MS51xB;
	return 0;
    }

    if (device_id == (uint32_t) DEVICE_ID_MS51PC) {
	*dev = DEVICE_MS51xC;
	return 0;
    }

    VERBOSE(LOG_ERR, "connect_to_target: unknown device %x\n", device_id);
    return -(errno = ENOENT);
}

static int reset(void)
{
    /* Performing reset {Auto ICP Mode Ext Mode} */
    (void) probe_reset(&probe, RESET_TYPE_AUTO, RESET_CONN_TYPE_ICPMODE,
                       RESET_MODE_EXTMODE);
    /* Performing reset {Auto Disconnect 0x00000001} */
    (void) probe_reset(&probe, RESET_TYPE_AUTO, RESET_CONN_TYPE_DISCONNECT,
                       RESET_MODE_MODE1);
    /* Performing reset {None (NuLink) Disconnect Ext Mode} */
    (void) probe_reset(&probe, RESET_TYPE_NONE_NULINK,
                       RESET_CONN_TYPE_DISCONNECT, RESET_MODE_EXTMODE);

    return 0;
}

static int erase(void)
{
    /* Erasing flash */
    return probe_erase_flash_chip(&probe);
}

static int program(device_t dev, const char *path)
{
#define FLASH_BLOCK_SIZE 32
#define FLASH_ALIGN_SIZE FLASH_BLOCK_SIZE
#define FLASH_MEM_SIZE   32768
    static uint8_t flash_mem[FLASH_MEM_SIZE];

    /* ihex init */
    memory_t memory;
    memory_t *head = &memory;

    /* Erasing flash */
    (void) probe_erase_flash_chip(&probe);
    /* Writing 32 bytes to config 0x0000 */
    (void) probe_write_memory(&probe, 0x0000, MEMORY_SPACE_CONFIG,
                              "\xff\xff\xff\xff\xff\xff\xff\xff"
                              "\xff\xff\xff\xff\xff\xff\xff\xff"
                              "\xff\xff\xff\xff\xff\xff\xff\xff"
                              "\xff\xff\xff\xff\xff\xff\xff\xff",
                              (size_t) 32);

    /* ihex parsing */
    memory_init(&memory);
    int write_size = 0;
    int n = parse_file((char *) path, &memory);

    switch (dev) {
    case DEVICE_N76Exxx:
	write_size = 18 * 1024;
	break;
    case DEVICE_MS51xB:
	write_size = 16 * 1024;
	break;
    case DEVICE_MS51xC:
	write_size = 32 * 1024;
	break;
    default:
	VERBOSE(LOG_ERR, "program: impossible to find flash size !");
	return -1;
    }

    VERBOSE(LOG_DBG,
            "program: parsed %d bytes in %d segments with %d records, write size %d\n",
            memory_size(&memory), memory_count(&memory), n, write_size);

    /* flatten memory */
    memset(flash_mem, 0xff, sizeof(flash_mem));
    for (int i = 0; i < memory_count(&memory); i++) {

	uint8_t *buffer = head->segment->buffer;
	int address = head->segment->address;
	int size = head->segment->size;

	VERBOSE(LOG_DBG, "program: segment #%d (0x%x - 0x%x)\n",
	        i + 1, address, address + size);

	/* to linear memory */
	memcpy(&flash_mem[address], buffer, size);
	/* next */
	head = head->next;
    }

    /* actual flashing */
    for (int addr = 0; addr < write_size; addr += FLASH_BLOCK_SIZE)
        /* write, no control (!) */
	(void) probe_write_memory(&probe, addr, MEMORY_SPACE_PROGRAM,
                                  &flash_mem[addr],
                                  (size_t) FLASH_BLOCK_SIZE);

    /* no need for this anymore */
    memory_free(&memory);

    /* Performing reset {Auto ICP Mode Ext Mode} */
    (void) probe_reset(&probe, RESET_TYPE_AUTO, RESET_CONN_TYPE_ICPMODE,
                       RESET_MODE_EXTMODE);
    /* Performing reset {Auto Disconnect 0x00000001} */
    (void) probe_reset(&probe, RESET_TYPE_AUTO, RESET_CONN_TYPE_DISCONNECT,
                       RESET_MODE_MODE1);
    /* Performing reset {None (NuLink) Disconnect Ext Mode} */
    (void) probe_reset(&probe, RESET_TYPE_NONE_NULINK,
                       RESET_CONN_TYPE_DISCONNECT, RESET_MODE_EXTMODE);

    return 0;
#undef FLASH_MEM_SIZE
#undef FLASH_ALIGN_SIZE
#undef FLASH_BLOCK_SIZE
}

static void print_usage(const char *argv0)
{
    VERBOSE(LOG_INFO, "Usage: %s [options]\n", argv0);
    VERBOSE(LOG_INFO, "Options are:\n");
    VERBOSE(LOG_INFO, " --reset|-r                   Reset probe/chip\n");
    VERBOSE(LOG_INFO, " --erase|-e                   Erase chip flash\n");
    VERBOSE(LOG_INFO,
            " --program|-p ihex            Program ihex file to chip memory\n");
    VERBOSE(LOG_INFO, " --force|-f n76|ms51xb|ms51xc Force device type\n");
    VERBOSE(LOG_INFO,
            " --verbose|-v                 Display more information\n");
    VERBOSE(LOG_INFO,
            " --help|-h                    Displays this message\n");
}

static int parse_force(const char *arg, device_t *dev)
{
    if (strcmp(arg, "n76") == 0) {
	*dev = DEVICE_N76Exxx;
	return 0;
    }

    if (strcmp(arg, "ms51xb") == 0) {
	*dev = DEVICE_MS51xB;
	return 0;
    }

    if (strcmp(arg, "ms51xc") == 0) {
	*dev = DEVICE_MS51xC;
	return 0;
    }

    *dev = DEVICE_COUNT;
    return -1;
}

int main(int argc, char **argv)
{
    int option_index = 0, c;
    static struct option long_options[] = {
	{ "reset", no_argument, 0, 'r' },
	{ "erase", no_argument, 0, 'e' },
	{ "program", required_argument, 0, 'p' },
	{ "force", required_argument, 0, 'f' },
	{ "verbose", no_argument, 0, 'v' },
	{ "help", no_argument, 0, 'h' },
	{ 0, 0, 0, 0 }
    };

    device_t device = DEVICE_COUNT;
    device_t forced_device = DEVICE_COUNT;
    command_t command = COMMAND_NONE;
    const char *arg = NULL;

    while ((c =
            getopt_long(argc, argv, "rep:f:vh", long_options,
                        &option_index)) != -1) {
	switch (c) {
	case 'r':
	    command = COMMAND_RESET;
	    break;
	case 'e':
	    command = COMMAND_ERASE;
	    break;
	case 'p':
	    command = COMMAND_PROGRAM;
	    arg = optarg;
	    break;
	case 'f':
	    if (parse_force(optarg, &forced_device) < 0) {
		VERBOSE(LOG_ERR, "--force: unknown device %s\n", optarg);
		print_usage(argv[0]);
		return 0;
	    }
	    break;
	case 'v':
	    verbose_level = LOG_DBG;
	    break;
	case 'h':              /*@fallthrough@ */
	default:
	    print_usage(argv[0]);
	    return 0;
	}
    }

    if (command == COMMAND_NONE) {
	print_usage(argv[0]);
	return 1;
    }

    /* options are ok */
    if (probe_init(&probe) != 0)
	VERBOSE(LOG_ERR, "Can't init probe\n");

    /* normal procedure */
    if (connect_to_target(&device) != 0) {
	VERBOSE(LOG_ERR, "Can't connect to target\n");
	return 1;
    }

    /* forced */
    if (forced_device != DEVICE_COUNT) {
	VERBOSE(LOG_INFO, "Device forced\n");
	device = forced_device;
    }

    switch (command) {
    case COMMAND_RESET:
	(void) reset();
	break;
    case COMMAND_ERASE:
	(void) erase();
	break;
    case COMMAND_PROGRAM:
	(void) program(device, arg);
	break;
    default:
	VERBOSE(LOG_ERR, "Unknown command %d\n", (int) command);
	print_usage(argv[0]);
	return 1;
    }

    /* the end */
    probe_release(&probe);
    return 0;
}
