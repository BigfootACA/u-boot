// SPDX-License-Identifier: LGPL-2.1+
/*
 * Copyright 2024 BigfootACA <bigfoot@classfun.cn>
 */

#include <common.h>
#include <command.h>
#include <mapmem.h>
#include <slots.h>
#include <part.h>
#include <blk.h>
#include <fs.h>

static bool load_slot_file(
	struct cmd_tbl *cmdtp,
	const char *slot,
	const char *name,
	ulong laddr
) {
	int ret;
	ulong dev, part;
	const char *intf, *prefix = "slot-";
	char lsaddr[32] = "-", dp[16] = "0:0", path[512], **args;
	if (!(intf = env_get("slot_boot_intf")))
		intf = CONFIG_SLOTS_DEFAULT_BOOT_INTF;
	dev = env_get_ulong("slot_boot_dev", 10, CONFIG_SLOTS_DEFAULT_BOOT_DEV);
	part = env_get_ulong("slot_boot_part", 10, CONFIG_SLOTS_DEFAULT_BOOT_PART);
	if (laddr) snprintf(lsaddr, sizeof(lsaddr), "0x%lx", laddr);
	if (strcmp(slot, "recovery") == 0) prefix = "";
	snprintf(path, sizeof(path), "%s%s/%s", prefix, slot, name);
	snprintf(dp, sizeof(dp), "%lu:%lu", dev, part);
	printf(
		"Loading file %s %s %s from slot %s to %s\n",
		intf, dp, path, slot, lsaddr
	);
	args = (char*[]){"load", (char*)intf, (char*)dp, lsaddr, path, NULL};
	if ((ret = do_load(cmdtp, 0, 5, args, 0)) != CMD_RET_SUCCESS) {
		printf(
			"Load file %s %s %s from slot %s to %s failed: %d\n",
			intf, dp, path, slot, lsaddr, ret
		);
		return false;
	}
	return true;
}

static ulong load_slot_file_env(
	struct cmd_tbl *cmdtp, const char *slot,
	const char *name, const char *env
) {
	ulong addr;
	if (!name || !name[0] || !env || !env[0]) return 0;
	if (!(addr = env_get_hex(env, 0))){
		printf("get environment variable %s failed\n", env);
		return 0;
	}
	if (!load_slot_file(cmdtp, slot, name, addr)) return 0;
	return addr;
}

static void setup_bootargs(const char *slot) {
	const char *args_global, *args_slot;
	char bootargs[1024];
	if (!(args_global = env_get("slot_bootargs")))
		args_global = "";
	if (!(args_slot = slot_get_info(slot, "bootargs", "")))
		args_slot = "";
	snprintf(
		bootargs, sizeof(bootargs), "%s%s%s",
		args_global,
		(args_global[0] && args_slot[0]) ? " " : "",
		args_slot
	);
	if(bootargs[0]) printf("Using bootargs \"%s\"\n", bootargs);
	env_set("bootargs", bootargs);
}

static void try_boot_fat(struct cmd_tbl *cmdtp, const char *slot) {
	int ret;
	ulong kaddr, raddr, faddr;
	const char *fdtfile;
	char ksaddr[32] = "-", rsaddr[32] = "-", fsaddr[32] = "-";
	char *args[] = {"bootz", ksaddr, rsaddr, fsaddr, NULL};
	if (!(fdtfile = env_get("fdtfile"))) {
		printf("FDT file not set\n");
		return;
	}
	kaddr = load_slot_file_env(cmdtp, slot, "zImage", "kernel_addr_r");
	raddr = load_slot_file_env(cmdtp, slot, "initrd.img", "ramdisk_addr_r");
	faddr = load_slot_file_env(cmdtp, slot, fdtfile, "fdt_addr");
	if (!kaddr || !faddr) {
		printf("Load kernel or fdt from slot %s failed\n", slot);
		return;
	}
	if (!raddr) printf("Skip load initrd from slot %s\n", slot);
	if (kaddr) snprintf(ksaddr, sizeof(ksaddr), "0x%lx", kaddr);
	if (raddr) snprintf(rsaddr, sizeof(rsaddr), "0x%lx", raddr);
	if (faddr) snprintf(fsaddr, sizeof(fsaddr), "0x%lx", faddr);
	setup_bootargs(slot);
	slot_flush();
	ret = do_bootz(cmdtp, 0, 4, args);
	if (ret != CMD_RET_SUCCESS)
		printf("Boot Linux from slot %s failed: %d\n", slot, ret);
}

static const char *get_boot_part_name(const char *slot, char *buff) {
	if (!slot_is_valid(slot)) slot = slot_get_current();
	if (strlen(slot) > 3) strncpy(buff, slot, 32);
	else snprintf(buff, 32, "boot_%s", slot);
	return buff;
}

static ulong load_part(const char *intf, int dev, const char *name, ulong laddr) {
	int part;
	void *addr;
	ulong res;
	struct blk_desc *desc;
	struct disk_partition info;
	if (!(desc = blk_get_dev(intf, dev))) {
		printf("Cannot get block device %s %d\n", intf, dev);
		return 0;
	}
	if ((part = part_get_info_by_name(desc, name, &info)) < 0) {
		printf("Cannot found partition with name %s in %s %d\n", name, intf, dev);
		return 0;
	}
	printf(
		"Load partition %s (%s %d:%d) size %lu bytes to 0x%lx\n",
		name, intf, dev, part, info.size * info.blksz, laddr
	);
	addr = map_sysmem(laddr, 0);
	res = blk_dread(desc, info.start, info.size, addr);
	if (res != info.size) {
		printf("Load partition %s mismatch %lu != %lu\n", name, res, info.size);
		return 0;
	}
	return res;
}

static void try_boot_part(struct cmd_tbl *cmdtp, const char *slot) {
	int ret;
	char partname[32];
	char lsaddr[32] = "-";
	const char *intf;
	ulong dev, laddr;
	if (!get_boot_part_name(slot, partname)) {
		printf("Unknown boot part name for slot %s\n", slot);
		return;
	}
	intf = env_get("slot_boot_intf");
	dev = env_get_ulong("slot_boot_dev", 10, 0);
	if (!intf || !intf[0]) intf = CONFIG_SLOTS_DEFAULT_BOOT_INTF;
	if (!(laddr = env_get_hex("kernel_addr_r", 0))) {
		printf("kernel_addr_r is not set\n");
		return;
	}
	if (!load_part(intf, (int)dev, partname, laddr)) {
		printf("Load boot partition %s failed\n", partname);
		return;
	}
	if (laddr) snprintf(lsaddr, sizeof(lsaddr), "0x%lx", laddr);
	setup_bootargs(slot);
	slot_flush();
	ret = do_bootm(cmdtp, 0, 2, (char*[]){"loadm", lsaddr, NULL});
	if (ret != CMD_RET_SUCCESS)
		printf("Boot image from slot %s failed: %d\n", slot, ret);
}

static void try_boot_method(struct cmd_tbl *cmdtp, const char *slot, const char *method) {
	if (!method) method = env_get("slot_method");
	if (!method) method = "fat";
	if (method && strcmp(method, "fat") == 0)
		try_boot_fat(cmdtp, slot);
	else if (method && strcmp(method, "part") == 0)
		try_boot_part(cmdtp, slot);
	else printf("Unknown boot method %s\n", method ? method : "(unknown)");
}

static void try_boot_slot(struct cmd_tbl *cmdtp, const char *slot) {
	printf("Try to boot from slot %s\n", slot);
	slot_set_current(slot);
	slot_inc_count(slot);
	try_boot_method(cmdtp, slot, NULL);
	printf("Failed to boot from slot %s\n", slot);
	printf("Treat slot %s as unbootable\n", slot);
	slot_set_unbootable(slot);
}

static int do_boot_slots(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]) {
	int i;
	const char *slot;
	slot_initialize();
	for (i = 0; slot_availables[i]; i++) printf(
		"Slot %s: count: %d, bootable: %s\n",
		slot_availables[i],
		slot_get_count(slot_availables[i]),
		slot_is_bootable(slot_availables[i]) ? "true" : "false"
	);
	printf("\n");
	if ((slot = slot_find_current_bootable())) do {
		try_boot_slot(cmdtp, slot);
		printf("\n");
	} while ((slot = slot_find_next_bootable(NULL)));
	slot_flush();
	printf("No any available slot found\n");
	return CMD_RET_SUCCESS;
}

static int do_reset_slots(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]) {
	int i;
	slot_initialize();
	for (i = 0; slot_availables[i]; i++){
		printf(
			"Slot %s: count: %d, bootable: %s\n",
			slot_availables[i],
			slot_get_count(slot_availables[i]),
			slot_is_bootable(slot_availables[i]) ? "true" : "false"
		);
		slot_set_count(slot_availables[i], 0);
	}
	slot_flush();
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	boot_slots, 1, 1, do_boot_slots,
	"Boot Linux with auto select slots", NULL
);

U_BOOT_CMD(
	reset_slots, 1, 1, do_reset_slots,
	"Reset all slots to bootable", NULL
);
