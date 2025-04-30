// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025 BigfootACA <bigfoot@classfun.cn>
 */

#include <dm.h>
#include <command.h>
#include <linux/delay.h>
#include <asm/gpio.h>
#include <asm/arch-rockchip/gpio.h>
#include <dt-bindings/pinctrl/rockchip.h>
#include <power/regulator.h>
#include <adc.h>
#include <stdio.h>
#include <fdt_simplefb.h>
#include <fdt_support.h>

DECLARE_GLOBAL_DATA_PTR;

#if IS_ENABLED(CONFIG_SERIAL) && IS_ENABLED(CONFIG_DM_SERIAL)
void serial_reinit_all(void){}
#endif

#ifndef CONFIG_XPL_BUILD

int ft_board_setup(void *blob, struct bd_info *bd)
{
	return 0;
}

static void out_gpio(const char *name, int val) {
	unsigned gpio;
	int ret;
	ret = gpio_lookup_name(name, NULL, NULL, &gpio);
	if (ret) {
		printf("gpio %s not found: %d\n", name, ret);
		return;
	}
	gpio_request(gpio, "gpio_output");
	ret = gpio_direction_output(gpio, val);
	if (ret) {
		printf("failed to set %s (%u): %d\n", name, gpio, ret);
	}
}

// static void enable_supply(const char *name) {
// 	int ret;
// 	struct udevice *dev;
// 	ret = regulator_get_by_platname(name, &dev);
// 	if (ret) {
// 		printf("regulator %s not found: %d\n", name, ret);
// 		return;
// 	}
// 	ret = regulator_set_enable(dev, true);
// 	if (ret) {
// 		printf("failed to enable %s (%s): %d\n", name, dev->name, ret);
// 	}
// }

static void enable_vidconsole(void)
{
	char *stdname;
	char buf[64];
	char *debug_uart = "serial@fe660000";

	stdname = env_get("stdout");
	if (!stdname || !strstr(stdname, "vidconsole")) {
		if (!stdname)
			stdname = debug_uart;
		snprintf(buf, sizeof(buf), "%s,vidconsole", stdname);
		env_set("stdout", buf);
	}

	stdname = env_get("stderr");
	if (!stdname || !strstr(stdname, "vidconsole")) {
		if (!stdname)
			stdname = debug_uart;
		snprintf(buf, sizeof(buf), "%s,vidconsole", stdname);
		env_set("stderr", buf);
	}
}

int board_init(void)
{
	if (IS_ENABLED(CONFIG_VIDEO))
		enable_vidconsole();

	/* Left USB VBUS */
	out_gpio("A20", 1);
	/* Right USB VBUS */
	out_gpio("D0", 1);

	/* WebCam Power */
	out_gpio("D2", 1);
	/* EC Power */
	out_gpio("D1", 1);

	/* eDP Backlight PWM Enable */
	out_gpio("D9", 1);
	/* eDP Backlight Enable */
	out_gpio("C27", 1);
	/* eDP Panel Enable */
	out_gpio("C28", 1);

	/* USB2 Hub Reset */
	mdelay(100);
	out_gpio("A8", 1);
	mdelay(100);
	out_gpio("A8", 0);

	/* EC Reset */
	mdelay(100);
	out_gpio("C1", 1);
	mdelay(100);
	out_gpio("C1", 0);

	return 0;
}
#endif
