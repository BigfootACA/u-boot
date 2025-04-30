// SPDX-License-Identifier: GPL-2.0+
/*
 *  EFI application tables support
 *
 *  Copyright (c) 2016 Alexander Graf
 *  Copyright (c) 2025 BigfootACA
 */

#include "asm/types.h"
#define LOG_CATEGORY LOGC_EFI

#include <efi_loader.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <smbios.h>
#include <linux/sizes.h>
#include <asm/global_data.h>
#include <display_options.h>
#include <dm.h>
#include <env.h>
#include <linux/stringify.h>
#include <linux/string.h>
#include <mapmem.h>
#include <smbios.h>
#include <misc.h>
#include <sysinfo.h>
#include <tables_csum.h>
#include <version.h>
#include <malloc.h>
#include <dm/ofnode.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

enum {
	TABLE_SIZE	= SZ_16K,
};

/* Safeguard for checking that U_BOOT_VERSION_NUM macros are compatible with U_BOOT_DMI */
#if U_BOOT_VERSION_NUM < 2000 || U_BOOT_VERSION_NUM > 2099 || \
    U_BOOT_VERSION_NUM_PATCH < 1 || U_BOOT_VERSION_NUM_PATCH > 12
#error U_BOOT_VERSION_NUM macros are not compatible with DMI, fix U_BOOT_DMI macros
#endif

#if IS_ENABLED(CONFIG_GENERATE_SMBIOS_TABLE)
#error CONFIG_GENERATE_SMBIOS_TABLE must disabled
#endif

/*
 * U_BOOT_DMI_DATE contains BIOS Release Date in format mm/dd/yyyy.
 * BIOS Release Date is calculated from U-Boot version and fixed day 01.
 * So for U-Boot version 2021.04 it is calculated as "04/01/2021".
 * BIOS Release Date should contain date when code was released
 * and not when it was built or compiled.
 */
#if U_BOOT_VERSION_NUM_PATCH < 10
#define U_BOOT_DMI_MONTH "0" __stringify(U_BOOT_VERSION_NUM_PATCH)
#else
#define U_BOOT_DMI_MONTH __stringify(U_BOOT_VERSION_NUM_PATCH)
#endif
#define U_BOOT_DMI_DAY "01"
#define U_BOOT_DMI_YEAR __stringify(U_BOOT_VERSION_NUM)
#define U_BOOT_DMI_DATE U_BOOT_DMI_MONTH "/" U_BOOT_DMI_DAY "/" U_BOOT_DMI_YEAR

enum {
	SMBIOS_PORT_CONNECTOR = 8,
};

struct smbios_ctx {
	char *eos;
	char *next_ptr;
	char *last_str;
};

struct __packed smbios_type4_ext {
	struct smbios_header hdr;
	u8 socket_design;
	u8 processor_type;
	u8 processor_family;
	u8 processor_manufacturer;
	u32 processor_id[2];
	u8 processor_version;
	u8 voltage;
	u16 external_clock;
	u16 max_speed;
	u16 current_speed;
	u8 status;
	u8 processor_upgrade;
	u16 l1_cache_handle;
	u16 l2_cache_handle;
	u16 l3_cache_handle;
	u8 serial_number;
	u8 asset_tag;
	u8 part_number;
	u8 core_count;
	u8 core_enabled;
	u8 thread_count;
	u16 processor_characteristics;
	u16 processor_family2;
	u16 core_count2;
	u16 core_enabled2;
	u16 thread_count2;
	u16 thread_enabled;
	u8 socket_type;
	char eos[SMBIOS_STRUCT_EOS_BYTES];
};

struct __packed smbios_type8 {
	struct smbios_header hdr;
	u8 int_ref;
	u8 int_conn_type;
	u8 ext_ref;
	u8 ext_conn_type;
	u8 port_type;
	char eos[SMBIOS_STRUCT_EOS_BYTES];
};

struct __packed smbios_type9 {
	struct smbios_header hdr;
	u8 slot_type;
	u8 slot_data_bus_width;
	u8 current_usage;
	u8 slot_length;
	u16 slot_id;
	u8 slot_characteristics1;
	u8 slot_characteristics2;
	u16 segment_group_num;
	u8 bus_num;
	u8 device_num;
	u8 data_bus_width;
	u8 peer_group_count;
	u8 slot_information;
	u8 slot_physical_width;
	u8 slot_pitch;
	u8 slot_height;
	char eos[SMBIOS_STRUCT_EOS_BYTES];
};

struct __packed smbios_type16 {
	struct smbios_header hdr;
	u8 location;
	u8 use;
	u8 err_corr;
	u32 max_capacity;
	u16 err_info_handle;
	u16 mem_dev_cnt;
	u64 ext_max_capacity;
	char eos[SMBIOS_STRUCT_EOS_BYTES];
};

struct __packed smbios_type17 {
	struct smbios_header hdr;
	u16 array_handle;
	u16 err_info_handle;
	u16 total_width;
	u16 data_width;
	u16 size;
	u8 form_factor;
	u8 device_set;
	u8 device_locator;
	u8 bank_locator;
	u8 memory_type;
	u16 type_detail;
	u16 speed;
	u8 manufacturer;
	u8 serial_number;
	u8 asset_tag;
	u8 part_number;
	u8 attributes;
	u32 ext_size;
	u16 configured_speed;
	u16 minimum_voltage;
	u16 maximum_voltage;
	u16 configured_voltage;
	char eos[SMBIOS_STRUCT_EOS_BYTES];
};

struct __packed smbios_type19 {
	struct smbios_header hdr;
	u32 start_addr;
	u32 end_addr;
	u16 array_handle;
	u8 partition_width;
	u64 ext_start_addr;
	u64 ext_end_addr;
	char eos[SMBIOS_STRUCT_EOS_BYTES];
};

typedef int (*smbios_write_type)(ulong *addr, int *handle,
				 struct smbios_ctx *ctx);

struct smbios_write_method {
	smbios_write_type write;
};

static int smbios_add_string(struct smbios_ctx *ctx, const char *str)
{
	int i = 1;
	char *p = ctx->eos;

	if (!str)
		return 0;

	for (;;) {
		if (!*p) {
			ctx->last_str = p;
			strcpy(p, str);
			p += strlen(str);
			*p++ = '\0';
			ctx->next_ptr = p;
			*p++ = '\0';

			return i;
		}

		if (!strcmp(p, str)) {
			ctx->last_str = p;
			return i;
		}

		p += strlen(p) + 1;
		i++;
	}
}

static void smbios_set_eos(struct smbios_ctx *ctx, char *eos)
{
	ctx->eos = eos;
	ctx->next_ptr = eos;
	ctx->last_str = NULL;
}

/**
 * smbios_string_table_len() - compute the string area size
 *
 * This computes the size of the string area including the string terminator.
 *
 * @ctx:	SMBIOS context
 * Return:	string area size
 */
static int smbios_string_table_len(const struct smbios_ctx *ctx)
{
	/* In case no string is defined we have to return two \0 */
	if (ctx->next_ptr == ctx->eos)
		return 2;

	/* Allow for the final \0 after all strings */
	return (ctx->next_ptr + 1) - ctx->eos;
}

static int smbios_write_type0(ulong *current, int *handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type0 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_BIOS_INFORMATION, len, *handle);
	smbios_set_eos(ctx, t->eos);
	t->vendor = smbios_add_string(ctx, "U-Boot");
	t->bios_ver = smbios_add_string(ctx, PLAIN_VERSION);
	t->bios_release_date = smbios_add_string(ctx, U_BOOT_DMI_DATE);
	t->bios_rom_size = 0xff;
	t->extended_bios_rom_size = SZ_4M >> 20;
	t->bios_characteristics = BIOS_CHARACTERISTICS_PCI_SUPPORTED |
				  BIOS_CHARACTERISTICS_SELECTABLE_BOOT |
				  BIOS_CHARACTERISTICS_UPGRADEABLE;
	t->bios_characteristics_ext1 = BIT(7); /* Smart Battery is supported */
	t->bios_characteristics_ext2 = BIOS_CHARACTERISTICS_EXT2_UEFI |
				       BIOS_CHARACTERISTICS_EXT2_TARGET;
	t->bios_major_release = U_BOOT_VERSION_NUM % 100;
	t->bios_minor_release = U_BOOT_VERSION_NUM_PATCH;
	t->ec_major_release = 0xff;
	t->ec_minor_release = 0xff;

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type1(ulong *current, int *handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type1 *t;
	int len = sizeof(*t);
	char *serial_str = env_get("serial#");

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_SYSTEM_INFORMATION, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->manufacturer = smbios_add_string(ctx, "ZTE");
	t->product_name = smbios_add_string(ctx, "W609D SE");
	t->version = smbios_add_string(ctx, "v0.3");
	if (serial_str) {
		t->serial_number = smbios_add_string(ctx, serial_str);
		strlcpy((char *)t->uuid, serial_str, sizeof(t->uuid));
	}
	t->wakeup_type = SMBIOS_WAKEUP_TYPE_POWER_SWITCH;
	t->sku_number = smbios_add_string(ctx, "2023AP6152");
	t->family = smbios_add_string(ctx, "W600D2");

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type2(ulong *current, int *handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type2 *t;
	int len = sizeof(*t);
	u8 *eos_addr;

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_BOARD_INFORMATION, len, *handle);
	eos_addr = (u8 *)t + len - sizeof(t->eos);
	smbios_set_eos(ctx, eos_addr);

	t->manufacturer = smbios_add_string(ctx, "ZTE");
	t->product_name = smbios_add_string(ctx, "W600D2_MB_RK68");
	t->version = smbios_add_string(ctx, "PVT_V0.3");
	t->serial_number = 0;
	t->asset_tag_number = 0;
	t->feature_flags = SMBIOS_BOARD_FEAT_HOST_BOARD;
	t->chassis_location = smbios_add_string(ctx, "Internal");
	t->board_type =	SMBIOS_BOARD_TYPE_MOTHERBOARD;

	t->chassis_handle = *handle + 1;

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type3(ulong *current, int *handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type3 *t;
	int len = sizeof(*t);
	u8 *eos_addr;

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_SYSTEM_ENCLOSURE, len, *handle);
	eos_addr = (u8 *)t + len - sizeof(t->eos);
	smbios_set_eos(ctx, eos_addr);

	t->manufacturer = smbios_add_string(ctx, "ZTE");
	t->chassis_type = 0x09; /* Laptop */
	t->bootup_state = SMBIOS_STATE_SAFE;
	t->power_supply_state = SMBIOS_STATE_SAFE;
	t->thermal_state = SMBIOS_STATE_SAFE;
	t->security_status = SMBIOS_SECURITY_NONE;
	t->version = smbios_add_string(ctx, "v0.3");
	t->serial_number = 0;
	t->asset_tag_number = 0;
	t->oem_defined = 0;
	t->height = 0;
	t->number_of_power_cords = 0;
	t->sku_number = 0;

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static char *rk3568_get_cpuid(void)
{
	#if IS_ENABLED(CONFIG_ROCKCHIP_OTP)
	int ret;
	u64 cpuid[2];
	struct udevice *dev;
	static char buff[40];

	ret = uclass_get_device_by_driver(UCLASS_MISC, DM_DRIVER_GET(rockchip_otp), &dev);
	if (ret < 0) {
		log_err("no rockchip_otp device found: %d\n", ret);
		return NULL;
	}

	ret = misc_read(dev, CFG_CPUID_OFFSET, cpuid, sizeof(cpuid));
	if (ret != sizeof(cpuid)) {
		log_err("failed to read cpuid: %d\n", ret);
		return NULL;
	}

	memset(buff, 0, sizeof(buff));
	sprintf(buff, "%016llx%016llx", cpuid[0], cpuid[1]);

	return buff;
	#else
	return NULL;
	#endif
}

static int smbios_write_type4(ulong *current, int *handle,
			      struct smbios_ctx *ctx)
{
	struct smbios_type4_ext *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_PROCESSOR_INFORMATION, len, *handle);
	smbios_set_eos(ctx, t->eos);
	t->socket_design = smbios_add_string(ctx, "SoC");
	t->processor_type = SMBIOS_PROCESSOR_TYPE_CENTRAL;
	t->processor_family = SMBIOS_PROCESSOR_FAMILY_EXT;
	t->processor_manufacturer = smbios_add_string(ctx, "ARM");
	t->processor_id[0] = 0x412fd050;
	t->processor_id[1] = 0;
	t->processor_version = smbios_add_string(ctx, "Cortex-A55");
	t->voltage = 140; /* 1.2V */
	t->external_clock = 0;
	t->max_speed = 2000; /* 2.0GHz */
	t->current_speed = 2000; /* 2.0GHz */
	t->status = BIT(6) | SMBIOS_PROCESSOR_STATUS_ENABLED; /* Populated | Enabled */
	t->processor_upgrade = 0xff;
	t->l1_cache_handle = *handle + 1; /* L1D */
	t->l2_cache_handle = SMBIOS_CACHE_HANDLE_NONE;
	t->l3_cache_handle = *handle + 3; /* L3 */
	t->serial_number = smbios_add_string(ctx, rk3568_get_cpuid());
	t->asset_tag = 0;
	t->part_number = smbios_add_string(ctx, "RK3568B2");
	t->core_count =	4;
	t->core_enabled = 4;
	t->thread_count = 4;
	t->processor_characteristics =
		SMBIOS_PROCESSOR_64BIT |
		SMBIOS_PROCESSOR_MULTICORE |
		SMBIOS_PROCESSOR_EXEC_PROT |
		SMBIOS_PROCESSOR_ENH_VIRT |
		SMBIOS_PROCESSOR_POW_CON;
	t->processor_family2 = SMBIOS_PROCESSOR_FAMILY_ARMV8;
	t->core_count2 = 4;
	t->core_enabled2 = 4;
	t->thread_count2 = 4;
	t->thread_enabled = 4;
	t->socket_type = smbios_add_string(ctx, "FCBGA636L");

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type7_l1d(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type7 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_CACHE_INFORMATION, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->socket_design = smbios_add_string(ctx, "L1 Data");
	t->config.fields.level = 1;
	t->config.fields.benabled = true;
	t->config.fields.opmode = 1; /* Write Back */
	t->max_size.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->max_size.fields.size = 32;
	t->inst_size.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->inst_size.fields.size = 32;
	t->supp_sram_type.fields.sync = true;
	t->curr_sram_type.fields.sync = true;
	t->speed = 0;
	t->err_corr_type = SMBIOS_CACHE_ERRCORR_SBITECC;
	t->sys_cache_type = SMBIOS_CACHE_SYSCACHE_TYPE_DATA;
	t->associativity = SMBIOS_CACHE_ASSOC_2WAY;
	t->max_size2.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->max_size2.fields.size = 32;
	t->inst_size2.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->inst_size2.fields.size = 32;

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type7_l1i(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type7 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_CACHE_INFORMATION, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->socket_design = smbios_add_string(ctx, "L1 Instruction");
	t->config.fields.level = 1;
	t->config.fields.benabled = true;
	t->config.fields.opmode = 1; /* Write Back */
	t->max_size.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->max_size.fields.size = 32;
	t->inst_size.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->inst_size.fields.size = 32;
	t->supp_sram_type.fields.sync = true;
	t->curr_sram_type.fields.sync = true;
	t->speed = 0;
	t->err_corr_type = SMBIOS_CACHE_ERRCORR_PARITY;
	t->sys_cache_type = SMBIOS_CACHE_SYSCACHE_TYPE_INST;
	t->associativity = SMBIOS_CACHE_ASSOC_OTHER;
	t->max_size2.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->max_size2.fields.size = 32;
	t->inst_size2.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->inst_size2.fields.size = 32;

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type7_l3(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type7 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_CACHE_INFORMATION, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->socket_design = smbios_add_string(ctx, "L3");
	t->config.fields.level = 3;
	t->config.fields.benabled = true;
	t->config.fields.opmode = 3; /* Varies With Memory Address */
	t->max_size.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->max_size.fields.size = 512;
	t->inst_size.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->inst_size.fields.size = 512;
	t->supp_sram_type.fields.sync = true;
	t->curr_sram_type.fields.sync = true;
	t->speed = 0;
	t->err_corr_type = SMBIOS_CACHE_ERRCORR_SBITECC;
	t->sys_cache_type = SMBIOS_CACHE_SYSCACHE_TYPE_UNIFIED;
	t->associativity = SMBIOS_CACHE_ASSOC_16WAY;
	t->max_size2.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->max_size2.fields.size = 512;
	t->inst_size2.fields.granu = SMBIOS_CACHE_GRANU_1K;
	t->inst_size2.fields.size = 512;

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type8_left_usb_typec(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type8 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_PORT_CONNECTOR, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->int_conn_type = 0;
	t->ext_conn_type = 0x23; /* Type-C */
	t->ext_ref = smbios_add_string(ctx, "USB Type-C");
	t->int_ref = smbios_add_string(ctx, "X1A6");
	t->port_type = 0x10; /* USB */

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type8_left_usb_typea(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type8 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_PORT_CONNECTOR, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->int_conn_type = 0;
	t->ext_conn_type = 0x12; /* USB */
	t->ext_ref = smbios_add_string(ctx, "Left USB Type-A");
	t->int_ref = smbios_add_string(ctx, "X5");
	t->port_type = 0x10; /* USB */

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type8_left_hdmi_typea(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type8 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_PORT_CONNECTOR, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->int_conn_type = 0;
	t->ext_conn_type = 0xff; /* Others */
	t->ext_ref = smbios_add_string(ctx, "Left HDMI Type-A");
	t->int_ref = smbios_add_string(ctx, "X15A5");
	t->port_type = 0x1c; /* Video */

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type8_right_audio_jack(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type8 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_PORT_CONNECTOR, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->int_conn_type = 0;
	t->ext_conn_type = 0x1f; /* Mini Jack */
	t->ext_ref = smbios_add_string(ctx, "Right 3.5mm Headphone Jack");
	t->int_ref = smbios_add_string(ctx, "X1");
	t->port_type = 0x1d; /* Audio Port */

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type8_right_usb_typea(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type8 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_PORT_CONNECTOR, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->int_conn_type = 0;
	t->ext_conn_type = 0x12; /* USB */
	t->ext_ref = smbios_add_string(ctx, "Right USB Type-A");
	t->int_ref = smbios_add_string(ctx, "X2");
	t->port_type = 0x10; /* USB */

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type9(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type9 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_SYSTEM_SLOTS, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->slot_type = 0x16; /* M.2 Key B */
	t->slot_data_bus_width = 0x09; /* x1 */
	t->current_usage = 0x02; /* Unknown */
	t->slot_length = 0x03; /* Short */
	t->slot_id = 0x0000;
	t->slot_characteristics1 = BIT(2); /* 3.3V */
	t->slot_characteristics2 = 0;
	t->segment_group_num = 0;
	t->bus_num = 0;
	t->device_num = 0;
	t->data_bus_width = 0;
	t->peer_group_count = 0;
	t->slot_information = 3; /* PCIe 3.0 */
	t->slot_physical_width = 0x0A; /* x2 */
	t->slot_pitch = 0x05; /* 0.5mm */
	t->slot_height = 0;

	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type16(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type16 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_PHYS_MEMORY_ARRAY, len, *handle);
	smbios_set_eos(ctx, t->eos);

	u64 max_mem_sz = 8ULL * SZ_1G;

	t->location = 0x3; /* System board */
	t->use = 0x3; /* System memory */
	t->err_corr = 0x3; /* No error correction */
	t->max_capacity = max_mem_sz / 1024; /* Max memory size in kilobytes */
	t->err_info_handle = 0xFFFE; /* Not provides */
	t->mem_dev_cnt = 1;
	t->ext_max_capacity = max_mem_sz; /* Max memory size in bytes */

	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type17(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type17 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_MEMORY_DEVICE, len, *handle);
	smbios_set_eos(ctx, t->eos);

	t->array_handle = *handle - 1;
	t->err_info_handle = 0xFFFE; /* Not provides */
	t->total_width = 32; /* 32 bits */
	t->data_width = 32; /* 32 bits */
	t->size = gd->ram_size / SZ_1M; /* Size in megabytes */
	t->form_factor = 0x05; /* Chip */
	t->device_set = 0; /* Not part of a set */
	t->device_locator = smbios_add_string(ctx, "Internal");
	t->bank_locator = smbios_add_string(ctx, "Bank 0");
	t->memory_type = 0x1E; /* LPDDR4 */
	t->type_detail = BIT(7) | BIT(13); /* Synchronous | Registered */
	t->speed = 2400; /* 2400 MT/s */
	t->manufacturer = smbios_add_string(ctx, "Samsung");
	t->serial_number = 0;
	t->asset_tag = 0;
	t->part_number = smbios_add_string(ctx, "K4F6E3S4HM-MGCJ");
	t->attributes = 1; /* 1 Ranks */
	t->ext_size = gd->ram_size / SZ_1M; /* Size in megabytes */
	t->configured_speed = 2400; /* 2400 MT/s */
	t->minimum_voltage = 1100; /* 1.1V */
	t->maximum_voltage = 1800; /* 1.8V */
	t->configured_voltage = 1100; /* 1.1V */

	len = t->hdr.length + smbios_string_table_len(ctx);
	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type19(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type19 *t;
	int len = sizeof(*t), total = 0;
	int array_handle = *handle - 2;
	int bank;
	
	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS; bank++) {
		phys_addr_t start = gd->bd->bi_dram[bank].start;
		phys_size_t size = gd->bd->bi_dram[bank].size;
		phys_addr_t end = start + size - 1;
		if (size == 0) break;

		t = map_sysmem(*current, len);
		memset(t, 0, len);
		fill_smbios_header(t, SMBIOS_MEMORY_ARRAY_MAPPED_ADDRESS, len, *handle);
		smbios_set_eos(ctx, t->eos);

		if (start >= UINT32_MAX) {
			t->start_addr = UINT32_MAX;
			t->end_addr = UINT32_MAX;
		} else {
			t->start_addr = start / 1024; /* Start address */
			t->end_addr = end / 1024; /* End address */
		}
		t->array_handle = array_handle;
		t->partition_width = 1;
		t->ext_start_addr = start;
		t->ext_end_addr = end;

		*current += len;
		total += len;
		unmap_sysmem(t);
		(*handle)++;
	}

	return total;
}


static int smbios_write_type32(ulong *current, int *handle, struct smbios_ctx *ctx)
{
	struct smbios_type32 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_SYSTEM_BOOT_INFORMATION, len, *handle);
	smbios_set_eos(ctx, t->eos);

	*current += len;
	unmap_sysmem(t);

	return len;
}

static int smbios_write_type127(ulong *current, int *handle,
				struct smbios_ctx *ctx)
{
	struct smbios_type127 *t;
	int len = sizeof(*t);

	t = map_sysmem(*current, len);
	memset(t, 0, len);
	fill_smbios_header(t, SMBIOS_END_OF_TABLE, len, *handle);

	*current += len;
	unmap_sysmem(t);

	return len;
}

static struct smbios_write_method smbios_write_funcs[] = {
	{ smbios_write_type0, },
	{ smbios_write_type1, },
	{ smbios_write_type2, },
	{ smbios_write_type3, },
	{ smbios_write_type4, },
	{ smbios_write_type7_l1d, },
	{ smbios_write_type7_l1i, },
	{ smbios_write_type7_l3, },
	{ smbios_write_type8_left_usb_typec, },
	{ smbios_write_type8_left_usb_typea, },
	{ smbios_write_type8_left_hdmi_typea, },
	{ smbios_write_type8_right_audio_jack, },
	{ smbios_write_type8_right_usb_typea, },
	{ smbios_write_type9, },
	{ smbios_write_type16, },
	{ smbios_write_type17, },
	{ smbios_write_type19, },
	{ smbios_write_type32, },
	{ smbios_write_type127 },
};

static ulong plat_write_smbios_table(ulong addr)
{
	ulong table_addr, start_addr;
	struct smbios3_entry *se;
	struct smbios_ctx ctx;
	ulong tables;
	int len = 0;
	int handle = 0;
	int i;

	start_addr = addr;

	addr = ALIGN(addr + sizeof(struct smbios3_entry), 16);
	tables = addr;

	for (i = 0; i < ARRAY_SIZE(smbios_write_funcs); i++) {
		const struct smbios_write_method *method;

		method = &smbios_write_funcs[i];
		len += method->write((ulong *)&addr, &handle, &ctx);
		handle++;
	}

	table_addr = (ulong)map_sysmem(tables, 0);
	se = map_sysmem(start_addr, sizeof(struct smbios3_entry));
	memset(se, '\0', sizeof(struct smbios3_entry));
	memcpy(se->anchor, "_SM3_", 5);
	se->length = sizeof(struct smbios3_entry);
	se->major_ver = 3;
	se->minor_ver = 8;
	se->doc_rev = 0;
	se->entry_point_rev = 1;
	se->table_maximum_size = len;
	se->struct_table_address = table_addr;
	se->checksum = table_compute_checksum(se, sizeof(struct smbios3_entry));
	unmap_sysmem(se);

	return addr;
}

static int plat_install_smbios_table(void)
{
	ulong addr;
	void *buf;

	/* Align the table to a 4KB boundary to keep EFI happy */
	buf = memalign(SZ_4K, TABLE_SIZE);
	if (!buf)
		return log_msg_ret("mem", -ENOMEM);

	addr = map_to_sysmem(buf);
	if (!plat_write_smbios_table(addr)) {
		log_err("Failed to write SMBIOS table\n");
		free(buf);
		return log_msg_ret("smbios", -EINVAL);
	}

	/* Make a note of where we put it */
	log_debug("SMBIOS tables written to %lx\n", addr);
	gd->arch.smbios_start = addr;

	return 0;
}
EVENT_SPY_SIMPLE(EVT_LAST_STAGE_INIT, plat_install_smbios_table);
