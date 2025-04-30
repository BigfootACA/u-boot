// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 Fuzhou Rockchip Electronics Co., Ltd
 */

#include <dm.h>
#include <i2c.h>
#include <video_bridge.h>
#include <asm/io.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <linux/media-bus-format.h>

#include "rockchip_bridge.h"
#include "rockchip_display.h"
#include "rockchip_panel.h"

#include "rk1000.h"

#define TVE_POWCR	0x03
#define TVE_OFF		0X07
#define TVE_ON		0x03

struct rk1000_tve {
	struct udevice *dev;
	struct rk1000_ctl rk1000_ctl;
};

enum {
	CVBS_NTSC = 0,
	CVBS_PAL,
};

int rk1000_tve_i2c_write(struct rk1000_tve *rk1000_tve, u8 reg, u8 val)
{
	struct dm_i2c_chip *chip = dev_get_parent_plat(rk1000_tve->dev);
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg;
	buf[1] = val;
	msg.addr = chip->chip_addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	ret = dm_i2c_xfer(rk1000_tve->dev, &msg, 1);
	if (ret) {
		dev_err(rk1000_tve->dev,
			"rk1000 tve i2c write failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int rk1000_tve_i2c_read(struct rk1000_tve *rk1000_tve, u8 reg, u8 *val)
{
	struct dm_i2c_chip *chip = dev_get_parent_plat(rk1000_tve->dev);
	u8 data;
	struct i2c_msg msg[] = {
		{
			.addr = chip->chip_addr,
			.flags = 0,
			.buf = (u8 *)&reg,
			.len = 1,
		}, {
			.addr = chip->chip_addr,
			.flags = I2C_M_RD,
			.buf = (u8 *)&data,
			.len = 1,
		}
	};
	int ret;

	ret = dm_i2c_xfer(rk1000_tve->dev, msg, 2);
	if (ret) {
		dev_err(rk1000_tve->dev,
			"rk1000 tve i2c read failed: %d\n", ret);
		return ret;
	}

	*val = data;

	return 0;
}

static int rk1000_tv_write_block(struct rk1000_tve *rk1000_tve,
				 u8 reg, const u8 *buf, u8 len)
{
	int i, ret;

	for (i = 0; i < len; i++) {
		ret = rk1000_tve_i2c_write(rk1000_tve, reg + i, buf[i]);
		if (ret)
			break;
	}

	return ret;
}

static int rk1000_tve_probe(struct udevice *dev)
{
	struct rk1000_tve *rk1000_tve = dev_get_priv(dev);
	struct rockchip_bridge *bridge =
		(struct rockchip_bridge *)dev_get_driver_data(dev);
	int ret;

	rk1000_tve->dev = dev;

	ret = uclass_get_device_by_name(UCLASS_I2C_GENERIC,
					"rk1000-ctl@40",
					&rk1000_tve->rk1000_ctl.dev);
	if (ret)
		return ret;

	bridge->dev = dev;

	return 0;
}

static void rk1000_tve_bridge_enable(struct rockchip_bridge *bridge)
{
	u8 tv_encoder_regs_pal[] = {0x06, 0x00, 0x00, 0x03, 0x00, 0x00};
	u8 tv_encoder_control_regs_pal[] = {0x41, 0x01};
	u8 tv_encoder_regs_ntsc[] = {0x00, 0x00, 0x00, 0x03, 0x00, 0x00};
	u8 tv_encoder_control_regs_ntsc[] = {0x43, 0x01};
	char data[4] = {0x88, 0x00, 0x22, 0x00};
	struct rk1000_tve *rk1000_tve = dev_get_priv(bridge->dev);
	struct connector_state *conn_state = &bridge->state->conn_state;
	struct drm_display_mode *mode = &conn_state->mode;
	struct rk1000_ctl *rk1000_ctl = &rk1000_tve->rk1000_ctl;

	rk1000_ctl_write_block(rk1000_ctl, 0, (u8 *)data, 4);

	/* rk1000 power down output dac */
	data[0] = 0x07;
	rk1000_tv_write_block(rk1000_tve, 0x03, (u8 *)data, 1);

	if (mode->vdisplay == 576) {
		rk1000_tv_write_block(rk1000_tve, 0, tv_encoder_regs_pal,
				      sizeof(tv_encoder_regs_pal));
		rk1000_ctl_write_block(rk1000_ctl, 3,
				       tv_encoder_control_regs_pal,
				       sizeof(tv_encoder_control_regs_pal));
	} else {
		rk1000_tv_write_block(rk1000_tve, 0, tv_encoder_regs_ntsc,
				      sizeof(tv_encoder_regs_ntsc));
		rk1000_ctl_write_block(rk1000_ctl, 3,
				       tv_encoder_control_regs_ntsc,
				       sizeof(tv_encoder_control_regs_ntsc));
	}
}

static void rk1000_tve_bridge_disable(struct rockchip_bridge *bridge)
{
	struct rk1000_tve *rk1000_tve = dev_get_priv(bridge->dev);
	char data[1] = {0x07};

	/* rk1000 power down output dac */
	rk1000_tv_write_block(rk1000_tve, 0x03, (u8 *)data, 1);
}

static const struct rockchip_bridge_funcs rk1000_tve_bridge_funcs = {
	.enable = rk1000_tve_bridge_enable,
	.disable = rk1000_tve_bridge_disable,
};

static struct rockchip_bridge rk1000_tve_driver_data = {
	.funcs = &rk1000_tve_bridge_funcs,
};

struct video_bridge_ops rk1000_tve_ops = {
};

static const struct udevice_id rk1000_tve_ids[] = {
	{
		.compatible = "rockchip,rk1000-tve",
		.data = (ulong)&rk1000_tve_driver_data,
	},
	{ }
};

U_BOOT_DRIVER(rk1000_tve) = {
	.name = "rk1000_tve",
	.id = UCLASS_VIDEO_BRIDGE,
	.of_match = rk1000_tve_ids,
	.ops = &rk1000_tve_ops,
	.probe = rk1000_tve_probe,
	.priv_auto = sizeof(struct rk1000_tve),
};
