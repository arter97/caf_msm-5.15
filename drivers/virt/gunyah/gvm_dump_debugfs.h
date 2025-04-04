/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#if IS_ENABLED(CONFIG_QCOM_GVM_DUMP)
int enable_gvm_ramdump_debugfs(void);
int collect_gvm_ramdump(struct device *dev);
void cleanup_gvm_ramdump_ctx(u16 gvm_id);
void cleanup_gvm_ramdump_list(void);
#else
static inline int enable_gvm_ramdump_debugfs(void)
{
	return 0;
}

static inline int collect_gvm_ramdump(struct device *dev)
{
	return 0;
}

static inline void cleanup_gvm_ramdump_ctx(u16 gvm_id)
{
}

static inline void cleanup_gvm_ramdump_list(void)
{
}
#endif
