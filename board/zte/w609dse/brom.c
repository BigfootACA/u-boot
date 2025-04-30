// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 BigfootACA <bigfoot@classfun.cn>
 */

#include <linux/delay.h>
#include <command.h>
#include <dm.h>
#include <sysreset.h>
#include <asm/io.h>
#include <asm/arch-rockchip/boot_mode.h>

#if (CONFIG_ROCKCHIP_BOOT_MODE_REG != 0)

extern void set_back_to_bootrom_dnl_flag(void);
static void goto_brom(void)
{
	writel(BOOT_LOADER, CONFIG_ROCKCHIP_BOOT_MODE_REG);
	flush_dcache_all();
	invalidate_icache_all();
	invalidate_dcache_all();
	sysreset_walk(SYSRESET_COLD);
}

static int do_rbrom(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]) {
	printf("Rebooting into Rockchip MaskROM...\n");
	goto_brom();
	return 0;
}

U_BOOT_CMD(
	rbrom, 1, 0,    do_rbrom,
	"Reboot into Rockchip MaskROM", ""
);

#if CONFIG_IS_ENABLED(SHOW_BOOT_PROGRESS)
#include <bootstage.h>

void show_boot_progress(int val)
{
	#if !defined(CONFIG_XPL_BUILD)
	if (val == BOOTSTAGE_ID_NEED_RESET)
		goto_brom();
	#endif
}
#endif
#endif
