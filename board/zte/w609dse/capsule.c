// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025 BigfootACA <bigfoot@classfun.cn>
 */

#include <efi.h>
#include <efi_loader.h>
#include <dm.h>

DECLARE_GLOBAL_DATA_PTR;

struct efi_fw_image fw_images[] = {
	{
		.image_type_id = EFI_GUID(0xC758C749, 0x62D2, 0x602C, 0xF6, 0x78, 0x3D, 0x62, 0xCA, 0x97, 0xC8, 0x67),
		.fw_name = u"ZTE-W609DSE-LOADER1",
		.image_index = 1,
	},{
		.image_type_id = EFI_GUID(0xDAA3B0B6, 0x70AA, 0xDAEF, 0x45, 0xC2, 0xEF, 0x6B, 0x03, 0xAD, 0xCA, 0x8B),
		.fw_name = u"ZTE-W609DSE-UBOOT",
		.image_index = 2,
	},{
		.image_type_id = EFI_GUID(0x7DE55355, 0xCCAB, 0x65F1, 0x14, 0x3D, 0xBA, 0x16, 0x8A, 0x86, 0x8D, 0x3A),
		.fw_name = u"ZTE-W609DSE-TRUST",
		.image_index = 3,
	},
};

struct efi_capsule_update_info update_info = {
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};

void rockchip_capsule_update_board_setup(void){}
