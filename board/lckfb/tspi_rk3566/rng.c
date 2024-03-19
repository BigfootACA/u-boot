// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 BigfootACA <bigfoot@classfun.cn>
 */

#include <common.h>
#include <efi.h>
#include <efi_loader.h>
#include <dm.h>
#include <rng.h>
#include <stdlib.h>

DECLARE_GLOBAL_DATA_PTR;

#if IS_ENABLED(CONFIG_BOARD_RNG_SEED) || IS_ENABLED(CONFIG_EFI_RNG_PROTOCOL)
static int get_rng_device(struct udevice **pdev){
	int ret = -ENODEV, i, cnt;
	*pdev = NULL;
	if ((cnt = uclass_id_count(UCLASS_RNG)) <= 0) {
		pr_info("no any rng device found\n");
		return ret;
	}
	for (i = 0; i < cnt; i++) {
		ret = uclass_get_device(UCLASS_RNG, i, pdev);
		if (!ret && *pdev) return 0;
		pr_debug("unable to get rng device %d: %d\n", i, ret);
	}
	pr_info("no available rng device found in %d: %d\n", cnt, ret);
	return ret;
}

#if IS_ENABLED(CONFIG_BOARD_RNG_SEED)
int board_rng_seed(struct abuf *buf) {
	struct udevice *dev;
	size_t len = 0x8;
	u64 *data;
	int ret;
	if ((ret = get_rng_device(&dev))) return ret;
	if (!(data = malloc(len))) return -ENOMEM;
	if ((ret = dm_rng_read(dev, data, len))) {
		pr_warn("unable to read rng seed: %d\n", ret);
		free(data);
		return ret;
	}
	abuf_init_set(buf, data, len);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_EFI_RNG_PROTOCOL)
efi_status_t platform_get_rng_device(struct udevice **dev) {
	return get_rng_device(dev) ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}
#endif
#endif
