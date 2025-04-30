// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025, BigfootACA <bigfoot@classfun.cn>
 */

#include <cpu.h>
#include <dm.h>
#include <linux/bitops.h>
#include <smbios_def.h>

DECLARE_GLOBAL_DATA_PTR;

static int rk3568_cpu_get_desc(const struct udevice *dev, char *buf, int size) {
	strncpy(buf, "Cortex-A55", size - 1);
	return 0;
}

static int rk3568_cpu_get_vendor(const struct udevice *dev, char *buf, int size) {
	strncpy(buf, "ARM", size - 1);
	return 0;
}

static int rk3568_cpu_get_info(const struct udevice *dev, struct cpu_info *info){
	info->address_width = 64; /* static ARMv8 64-bit */
	info->cpu_freq = 2000000000; /* 2.0GHz */
	info->features = 0;
	info->features |= BIT(CPU_FEAT_L1_CACHE);
	info->features |= BIT(CPU_FEAT_MMU);
	info->features |= BIT(CPU_FEAT_DEVICE_ID);
	return 0;
}

static int rk3568_cpu_is_current(struct udevice *dev){
	unsigned long mpidr;
	asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr));
	return dev_read_addr(dev) == (mpidr & 0xffff);
}

static int rk3568_cpu_get_count(const struct udevice *dev){
	return 4; /* static 4 core */
}

static int rk3568_cpu_bind(struct udevice *dev) {
	struct cpu_plat *plat = dev_get_parent_plat(dev);
	plat->cpu_id = dev_read_addr(dev); /* mpidr in device tree */
	plat->device_id = 0x412fd050; /* static midr */
	plat->timebase_freq = 2000000000; /* 2.0GHz */
	plat->family = SMBIOS_PROCESSOR_FAMILY_ARMV8; /* SMBIOS ARMv8 */
	plat->id[0] = 0x412fd050; /* static midr */
	plat->id[1] = 0; /* must be zero */
	plat->ucode_version = 0;
	return 0;
}

static int rk3568_cpu_probe(struct udevice *dev){
	return rk3568_cpu_bind(dev);
}

static const struct cpu_ops rk3568_cpu_ops = {
	.get_desc       = rk3568_cpu_get_desc,
	.get_info       = rk3568_cpu_get_info,
	.get_count      = rk3568_cpu_get_count,
	.get_vendor     = rk3568_cpu_get_vendor,
	.is_current     = rk3568_cpu_is_current,
};

static const struct udevice_id rk3568_cpu_ids[] = {
	{ .compatible = "arm,cortex-a55" },
	{ }
};

U_BOOT_DRIVER(rk3568_cpu) = {
	.name = "rk3568_cpu",
	.id = UCLASS_CPU,
	.of_match = rk3568_cpu_ids,
	.bind = rk3568_cpu_bind,
	.probe = rk3568_cpu_probe,
	.ops = &rk3568_cpu_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
