// SPDX-License-Identifier: LGPL-2.1+
/*
 * Copyright 2024 BigfootACA <bigfoot@classfun.cn>
 */

#include <vsprintf.h>
#include <env.h>
#include <stdbool.h>
#include <stdio.h>
#include <slots.h>

const char *slot_availables[] = {"a", "b", "recovery", NULL};
int slot_max_count = CONFIG_SLOTS_DEFAULT_MAX_COUNT;
static bool slot_env_changed = false;

#ifndef CONFIG_BOOTARGS
#define CONFIG_BOOTARGS ""
#endif

void slot_flush(void) {
	if (!slot_env_changed) return;
	env_save();
	slot_env_changed = false;
}

void slot_initialize(void) {
	if (env_get_yesno("slot_initialized") == 1) return;
	env_load();
	if (env_get_yesno("slot_initialized") == 1) return;
	env_set("slot_current", slot_availables[0]);
	env_set("slot_initialized", "yes");
	env_set("slot_method", "part");
	env_set("slot_bootargs", CONFIG_BOOTARGS);
	env_set("slot_a_count", "0");
	env_set("slot_a_bootargs", "root=PARTLABEL=system_a");
	env_set("slot_b_count", "0");
	env_set("slot_b_bootargs", "root=PARTLABEL=system_b");
	env_set("slot_recovery_count", "0");
	env_set("slot_boot_intf", CONFIG_SLOTS_DEFAULT_BOOT_INTF);
	env_set_ulong("slot_boot_dev", CONFIG_SLOTS_DEFAULT_BOOT_DEV);
	env_set_ulong("slot_boot_part", CONFIG_SLOTS_DEFAULT_BOOT_PART);
	env_save();
}

bool slot_is_valid(const char *slot) {
	int i;
	if (!slot || !slot[0]) return false;
	for (i = 0; slot_availables[i]; i++)
		if (strcmp(slot, slot_availables[i]) == 0)
			return true;
	return false;
}

const char *slot_get_current(void) {
	int i;
	const char *cur = env_get("slot_current");
	for (i = 0; slot_availables[i]; i++)
		if (strcmp(cur, slot_availables[i]) == 0)
			return slot_availables[i];
	return slot_find_bootable();
}

void slot_set_current(const char *slot) {
	if (!slot || !slot[0]) return;
	if (!slot_is_valid(slot)) {
		printf("Target slot %s is invalid\n", slot);
		return;
	}
	printf("Switch to slot %s\n", slot);
	env_set("slot_current", slot);
	slot_env_changed = true;
}

const char *slot_get_info(const char *slot, const char *var, const char *def) {
	char buff[64];
	const char *val;
	if (!var || !var[0]) return def;
	if (!slot_is_valid(slot)) slot = slot_get_current();
	snprintf(buff, sizeof(buff), "slot_%s_%s", slot, var);
	val = env_get(buff);
	if (!val || !val[0]) return def;
	return val;
}

void slot_set_info(const char *slot, const char *var, const char *val) {
	char buff[64];
	if (!slot_is_valid(slot)) slot = slot_get_current();
	snprintf(buff, sizeof(buff), "slot_%s_%s", slot, var);
	env_set(buff, val);
	slot_env_changed = true;
}

unsigned long slot_get_info_unsigned(
	const char *slot,
	const char *var,
	unsigned long def
) {
	unsigned long ret;
	char buff[64], *endp;
	const char *val;
	snprintf(buff, sizeof(buff), "%lu", def);
	val = slot_get_info(slot, var, buff);
	if (!val || !val[0]) return def;
	ret = simple_strtoul(val, &endp, 0);
	if (!endp || endp[0]) return def;
	return ret;
}

void slot_set_info_unsigned(const char *slot, const char *var, unsigned long val) {
	char buff[64];
	snprintf(buff, sizeof(buff), "%lu", val);
	slot_set_info(slot, var, buff);
}

int slot_get_count(const char *slot) {
	return (int)slot_get_info_unsigned(slot, "count", slot_max_count);
}

void slot_set_count(const char *slot, int count) {
	printf("Set slot %s count to %d\n", slot, count);
	slot_set_info_unsigned(slot, "count", (unsigned long)count);
}

void slot_inc_count(const char *slot) {
	int cnt = slot_get_count(slot) + 1;
	if (cnt < 1) cnt = 1;
	if (cnt > slot_max_count) cnt = slot_max_count;
	slot_set_count(slot, cnt);
}

void slot_set_unbootable(const char *slot) {
	slot_set_count(slot, slot_max_count);
}

bool slot_is_bootable(const char *slot) {
	return slot_get_count(slot) < slot_max_count;
}

bool slot_is_successful(const char *slot) {
	return slot_get_count(slot) == 0;
}

const char *slot_find_bootable(void) {
	int i;
	for (i = 0; slot_availables[i]; i++){
		if (!slot_is_bootable(slot_availables[i])) continue;
		if (strcmp(slot_availables[i], "recovery") == 0) continue;
		return slot_availables[i];
	}
	if (slot_is_bootable("recovery")) return "recovery";
	return NULL;
}

const char *slot_find_current_bootable(void) {
	const char *cur = slot_get_current();
	const char *next = slot_find_bootable();
	if (strcmp(cur, "recovery") == 0) return next;
	if (slot_is_bootable(cur)) return cur;
	return next;
}

const char *slot_find_next_bootable(const char *slot) {
	int i;
	if (!slot_is_valid(slot)) slot = slot_get_current();
	for (i = 0; slot_availables[i]; i++){
		if (!slot_is_bootable(slot_availables[i])) continue;
		if (strcmp(slot, slot_availables[i]) == 0) continue;
		if (strcmp(slot_availables[i], "recovery") == 0) continue;
		return slot_availables[i];
	}
	if (slot_is_bootable("recovery")) return "recovery";
	return NULL;
}
