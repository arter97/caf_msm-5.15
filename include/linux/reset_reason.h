/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved. */

#include <linux/types.h>

struct nvmem_cell;
extern void store_reset_reason(char *cmd, struct nvmem_cell *nvmem_cell);
