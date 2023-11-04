/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2020 Jernej Skrabec <jernej.skrabec@siol.net>
 */

enum axp313a_reg {
	AXP313A_CHIP_VERSION = 0x3,
	AXP313A_OUTPUT_CTRL1 = 0x10,
	AXP313A_DCDC3_VOLTAGE = 0x15,
	AXP313A_SHUTDOWN = 0x1A,
};

#define AXP313A_CHIP_VERSION_MASK	0xcf

#define AXP313A_OUTPUT_CTRL1_DCDC1_EN	(1 << 0)	// GPU
#define AXP313A_OUTPUT_CTRL1_DCDC2_EN	(1 << 1)	// CPU
#define AXP313A_OUTPUT_CTRL1_DCDC3_EN	(1 << 2)	// DRAM
#define AXP313A_OUTPUT_CTRL1_ALDO1_EN	(1 << 3)	// could be 1v8 bus
#define AXP313A_OUTPUT_CTRL1_DLDO1_EN	(1 << 4)	// could be 3v3 bus

#define AXP313A_POWEROFF		(1 << 7)
