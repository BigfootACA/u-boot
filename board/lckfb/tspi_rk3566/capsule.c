// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 BigfootACA <bigfoot@classfun.cn>
 */

#include <common.h>
#include <efi.h>
#include <efi_loader.h>
#include <dm.h>

DECLARE_GLOBAL_DATA_PTR;

struct efi_fw_image fw_images[] = {
	{
		.image_type_id = EFI_GUID(0x32d0cdc7, 0xf2d1, 0x4d61, 0x9f, 0xf4, 0x02, 0x8c, 0x53, 0x9b, 0xda, 0xa9),
		.fw_name = u"LCKFB-TSPI-RK3566-LOADER1",
		.image_index = 1,
	},{
		.image_type_id = EFI_GUID(0x27b8a16d, 0xf562, 0x47f1, 0xba, 0x21, 0xd9, 0x5b, 0xeb, 0xe8, 0x17, 0x23),
		.fw_name = u"LCKFB-TSPI-RK3566-LOADER2",
		.image_index = 2,
	},{
		.image_type_id = EFI_GUID(0x737be826, 0x9014, 0x4ad0, 0x98, 0xb0, 0x40, 0x56, 0x52, 0x7a, 0x97, 0x05),
		.fw_name = u"LCKFB-TSPI-RK3566-TRUST",
		.image_index = 3,
	},
};

struct efi_capsule_update_info update_info = {
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};

void rockchip_capsule_update_board_setup(void){}