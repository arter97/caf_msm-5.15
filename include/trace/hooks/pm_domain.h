/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM pm_domain

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_PM_DOMAIN_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_PM_DOMAIN_H

#if defined(CONFIG_CPU_IDLE_GOV_QCOM_LPM)
#include <trace/hooks/vendor_hooks_declare.h>
#else
#include <trace/hooks/vendor_hooks.h>
#endif

/* struct generic_pm_domain */
#include <linux/pm_domain.h>
DECLARE_HOOK(android_vh_allow_domain_state,
	TP_PROTO(struct generic_pm_domain *genpd, uint32_t idx, bool *allow),
	TP_ARGS(genpd, idx, allow))

#endif /* _TRACE_HOOK_PM_DOMAIN_H */

#include <trace/define_trace.h>
