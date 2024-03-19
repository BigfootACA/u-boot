// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 BigfootACA <bigfoot@classfun.cn>
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <adc.h>

#if (CONFIG_ROCKCHIP_BOOT_MODE_REG != 0)
/* Use saradc channel 0 */
int rockchip_dnl_key_pressed(void) {
	unsigned int val;
	struct udevice *dev;
	struct uclass *uc;
	int ret;
	if ((ret = uclass_get(UCLASS_ADC, &uc))) return false;
	uclass_foreach_dev(dev, uc) {
		if (strncmp(dev->name, "saradc", 6)) continue;
		if (!(ret = adc_channel_single_shot(dev->name, 0, &val)))
			return val >= 0 && val <= 50;
		pr_warn("unable to read adc value: %d\n", ret);
	}
	pr_warn("no available saradc device found\n");
	return false;
}

extern void set_back_to_bootrom_dnl_flag(void);
static int do_rbrom(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]) {
	set_back_to_bootrom_dnl_flag();
	return do_reset(cmdtp, 0, 0, NULL);
}

U_BOOT_CMD(
	rbrom, 1, 0,	do_rbrom,
	"Reboot into Rockchip MaskROM", ""
);

#endif
