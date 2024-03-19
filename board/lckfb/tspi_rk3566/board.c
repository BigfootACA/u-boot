// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 BigfootACA <bigfoot@classfun.cn>
 */

#include <common.h>
#include <dm.h>
#include <adc.h>

DECLARE_GLOBAL_DATA_PTR;

#if IS_ENABLED(CONFIG_SERIAL) && IS_ENABLED(CONFIG_DM_SERIAL)
void serial_reinit_all(void){}
#endif
