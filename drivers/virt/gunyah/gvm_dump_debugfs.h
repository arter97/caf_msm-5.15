/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#if IS_ENABLED(CONFIG_QCOM_GVM_DUMP)
int enable_gvm_dump_debugfs(void);
int collect_gvm_dump(struct device_node *node);
void cleanup_gvm_dump_ctx(u16 gvm_id);
void cleanup_gvm_dump_list(void);
#else
static inline int enable_gvm_dump_debugfs(void)
{
	return 0;
}

static inline int collect_gvm_dump(struct device_node *node)
{
	return 0;
}

static inline void cleanup_gvm_dump_ctx(u16 gvm_id)
{
}

static inline void cleanup_gvm_dump_list(void)
{
}
#endif
