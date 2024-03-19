/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __QUARTZ64_RK3566_H
#define __QUARTZ64_RK3566_H

#define ROCKCHIP_DEVICE_SETTINGS \
	"dfu_alt_info=loader1 part 0 1 mmcpart 0; loader2 part 0 2 mmcpart 0; trust part 0 3 mmcpart 0;\0" \
	"bootargs=root=PARTLABEL=root rootfstype=ext4 rootwait=10 ro\0" \
	"boot_targets=mmc0 mmc1 nvme scsi usb pxe dhcp spi\0" \
	"bootmeths=efi extlinux script pxe\0"

#include <configs/rk3568_common.h>

#endif
