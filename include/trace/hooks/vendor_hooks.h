/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

/*
 * Note: we intentionally omit include file ifdef protection
 *  This is due to the way trace events work. If a file includes two
 *  trace event headers under one "CREATE_TRACE_POINTS" the first include
 *  will override the DECLARE_RESTRICTED_HOOK and break the second include.
 */

#ifndef __GENKSYMS__
#include <linux/tracepoint.h>
#endif

#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_ANDROID_VENDOR_HOOKS)
#include <trace/hooks/vendor_hooks_declare.h>
#else /* !CONFIG_TRACEPOINTS || !CONFIG_ANDROID_VENDOR_HOOKS */
/* suppress trace hooks */
#define DECLARE_HOOK DECLARE_EVENT_NOP
#define DECLARE_RESTRICTED_HOOK(name, proto, args, cond)		\
	DECLARE_EVENT_NOP(name, PARAMS(proto), PARAMS(args))
#endif
