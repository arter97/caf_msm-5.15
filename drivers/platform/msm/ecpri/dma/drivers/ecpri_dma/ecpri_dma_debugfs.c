/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/stringify.h>
#include "ecpri_dma_i.h"
#include "dmahal.h"
#include "ecpri_dma_reg_dump.h"

#define DMA_MAX_ENTRY_STRING_LEN 500
#define DMA_MAX_MSG_LEN 4096

#define DMA_DUMP_STATUS_FIELD(f) \
	pr_err(#f "=0x%x\n", status->f)

#define DMA_READ_ONLY_MODE  0444
#define DMA_READ_WRITE_MODE 0664
#define DMA_WRITE_ONLY_MODE 0220

struct ecpri_dma_debugfs_file {
	const char *name;
	umode_t mode;
	void *data;
	const struct file_operations fops;
};

static struct dentry *dent;
static char dbg_buff[DMA_MAX_MSG_LEN + 1];

static s8 ep_reg_idx;


static ssize_t ecpri_dma_read_gen_reg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;

	nbytes = scnprintf(dbg_buff, DMA_MAX_MSG_LEN, "TBD\n");

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ecpri_dma_write_ep_reg(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	s8 option;
	int ret;

	ret = kstrtos8_from_user(buf, count, 0, &option);
	if (ret)
		return ret;

	if (option >= ECPRI_DMA_ENDP_NUM_MAX) {
		DMAERR("bad endp specified %u\n", option);
		return count;
	}

	ep_reg_idx = option;

	return count;
}

/**
 * ecpri_dma_read_ep_reg_n() - Reads and prints endpoint configuration registers
 *
 * Returns the number of characters printed
 */
int ecpri_dma_read_ep_reg_n(char *buf, int max_len, int endp)
{
	return scnprintf(
		dbg_buff, DMA_MAX_MSG_LEN,
		"ECPRI_DMA_ECPRI_ENDP_CFG_DEST_%u=0x%x\n"
		"ECPRI_DMA_ECPRI_ENDP_CFG_XBAR_%u=0x%x\n"
		"ECPRI_DMA_ECPRI_ENDP_GSI_CFG_%u=0x%x\n",
		endp, ecpri_dma_hal_read_reg_n(ECPRI_ENDP_CFG_DEST_n, endp),
		endp, ecpri_dma_hal_read_reg_n(ECPRI_ENDP_CFG_XBAR_n, endp),
		endp, ecpri_dma_hal_read_reg_n(ECPRI_ENDP_GSI_CFG_n, endp));
}

static ssize_t ecpri_dma_read_ep_reg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	int i;
	int start_idx;
	int end_idx;
	int size = 0;
	int ret;
	loff_t pos;

	/* negative ep_reg_idx means all registers */
	if (ep_reg_idx < 0) {
		start_idx = 0;
		end_idx = ecpri_dma_ctx->ecpri_dma_num_endps;
	} else {
		start_idx = ep_reg_idx;
		end_idx = start_idx + 1;
	}
	pos = *ppos;
	for (i = start_idx; i < end_idx; i++) {

		nbytes = ecpri_dma_read_ep_reg_n(dbg_buff, DMA_MAX_MSG_LEN, i);

		*ppos = pos;
		ret = simple_read_from_buffer(ubuf, count, ppos, dbg_buff,
					      nbytes);
		if (ret < 0) {
			return ret;
		}

		size += ret;
		ubuf += nbytes;
		count -= nbytes;
	}

	*ppos = pos + size;
	return size;
}

static ssize_t ecpri_dma_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	int cnt = 0;

	nbytes = scnprintf(dbg_buff, DMA_MAX_MSG_LEN,
		"TBD\n");
	cnt += nbytes;

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ecpri_dma_read_ecpri_dma_hal_regs(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	ecpri_dma_hal_print_all_regs(true);

	return 0;
}

static ssize_t ecpri_dma_trigger_dump_collect(struct file* file,
	const char __user* buf,
	size_t count, loff_t* ppos)
{
	ecpri_dma_save_registers();

	return count;
}

static const struct ecpri_dma_debugfs_file debugfs_files[] = {
	{
		"gen_reg", DMA_READ_ONLY_MODE, NULL, {
			.read = ecpri_dma_read_gen_reg
		}
	}, {
		"ep_reg", DMA_READ_WRITE_MODE, NULL, {
			.read = ecpri_dma_read_ep_reg,
			.write = ecpri_dma_write_ep_reg,
		}
	}, {
		"stats", DMA_READ_ONLY_MODE, NULL, {
			.read = ecpri_dma_read_stats,
		}
	}, {
		"ecpri_dma_print_regs", DMA_READ_ONLY_MODE, NULL, {
			.read = ecpri_dma_read_ecpri_dma_hal_regs,
		}
	},{
		"ecpri_dma_collect_regs", DMA_WRITE_ONLY_MODE, NULL, {
			.write = ecpri_dma_trigger_dump_collect,
		}
	},
};

void ecpri_dma_debugfs_init(void)
{
	const size_t debugfs_files_num =
		sizeof(debugfs_files) / sizeof(struct ecpri_dma_debugfs_file);
	size_t i;
	struct dentry *file;

	dent = debugfs_create_dir("ecpri_dma", NULL);
	if (IS_ERR(dent)) {
		DMAERR("fail to create folder in debug_fs.\n");
		return;
	}

	debugfs_create_u32("ecpri_hw_ver", DMA_READ_ONLY_MODE,
		dent, &ecpri_dma_ctx->ecpri_hw_ver);

	debugfs_create_u32("hw_flavor", DMA_READ_ONLY_MODE,
		dent, &ecpri_dma_ctx->hw_flavor);

	for (i = 0; i < debugfs_files_num; ++i) {
		const struct ecpri_dma_debugfs_file *curr = &debugfs_files[i];

		file = debugfs_create_file(curr->name, curr->mode, dent,
			curr->data, &curr->fops);
		if (!file || IS_ERR(file)) {
			DMAERR("fail to create file for debug_fs %s\n",
				curr->name);
			goto fail;
		}
	}

	return;

fail:
	debugfs_remove_recursive(dent);
}

void ecpri_dma_debugfs_remove(void)
{
	if (IS_ERR(dent)) {
		DMAERR("Debugfs:folder was not created.\n");
		return;
	}
	debugfs_remove_recursive(dent);
}

struct dentry *ecpri_dma_debugfs_get_root(void)
{
	return dent;
}
EXPORT_SYMBOL(ecpri_dma_debugfs_get_root);

#else /* !CONFIG_DEBUG_FS */
#define INVALID_NO_OF_CHAR (-1)
void ecpri_dma_debugfs_init(void) {}
void ecpri_dma_debugfs_remove(void) {}
#endif
