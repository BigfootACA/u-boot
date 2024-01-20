// SPDX-License-Identifier: LGPL-2.1+
/*
 * Copyright 2024 BigfootACA <bigfoot@classfun.cn>
 */

#ifndef SLOTS_H
#define SLOTS_H
extern const char *slot_availables[];
extern int slot_max_count;
extern void slot_flush(void);
extern void slot_initialize(void);
extern bool slot_is_valid(const char *slot);
extern const char *slot_get_current(void);
extern void slot_set_current(const char *slot);
extern const char *slot_get_info(const char *slot, const char *var, const char *def);
extern void slot_set_info(const char *slot, const char *var, const char *val);
extern unsigned long slot_get_info_unsigned(const char *slot, const char *var, unsigned long def);
extern void slot_set_info_unsigned(const char *slot, const char *var, unsigned long val);
extern int slot_get_count(const char *slot);
extern void slot_set_count(const char *slot, int count);
extern void slot_inc_count(const char *slot);
extern bool slot_is_bootable(const char *slot);
extern bool slot_is_successful(const char *slot);
extern void slot_set_unbootable(const char *slot);
extern const char *slot_find_bootable(void);
extern const char *slot_find_next_bootable(const char *slot);
extern const char *slot_find_current_bootable(void);
#endif
