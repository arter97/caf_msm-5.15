// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/of_address.h>
#include <linux/string.h>

/*
 * Create the list to add dump context of all the GVM during dump
 * collecton and delete the context when GVM restarts after dump collection.
 */
static LIST_HEAD(gvm_dump_list);

/*
 * Define mutex for gvm_dump_list to avoid race condition while adding and
 * deleting gvm_dump_ctx.
 */

static DEFINE_MUTEX(gvm_dump_list_mutex);

static struct dentry *gvm_dump_enable_dentry;

/*
 * Define per GVM dump context to be used for dump collection.
 * vmid - needed during cleanup of GVM dump when curesponding
 * GVM restart after dump collection.
 * dump_start_addr, dump_size and mapped_addr for dump collection
 * in ioremapped address.
 * gvm_dump_dir - points to curesponding GVM debugFS dump dir and
 * required during cleanup as well.
 * lock is mutex lock to be acquired as dump context is shared in
 * dump collection and cleanup.
 * list - list_head to add per GVM dum ctx in gvm_dump_list for cleanup.
 */
struct gvm_dump_ctx {
	int vmid;
	phys_addr_t dump_start_addr;
	size_t dump_size;
	void *__iomem mapped_addr;
	struct dentry *gvm_dump_dir;
	struct mutex lock;
	struct list_head list;
};

/* debugFS creation for GVM dump is enabled by default */
static bool is_gvm_dump_enabled = true;

/* Sanitize the GVM memory region before providing from PVM during VM creation */
#ifdef CONFIG_QCOM_SANITIZE_GVM_MEMORY
static void sanitize_gvm_region(struct gvm_dump_ctx *gvm_ctx)
{
	mutex_lock(&gvm_ctx->lock);
	gvm_ctx->mapped_addr = ioremap(gvm_ctx->dump_start_addr, gvm_ctx->dump_size);

	if (gvm_ctx->mapped_addr) {
		memset_io(gvm_ctx->mapped_addr, 0, gvm_ctx->dump_size);
		iounmap(gvm_ctx->mapped_addr);
	} else
		pr_err("gvm_dump: %s: ioremap failed\n", __func__);

	mutex_unlock(&gvm_ctx->lock);
}
#else
static inline void sanitize_gvm_region(struct gvm_dump_ctx *gvm_ctx) {}
#endif

/* Per GVM dump context cleanup in GVM restart after dump collection in GVM exit */
static void cleanup_single_gvm(struct gvm_dump_ctx *gvm_ctx)
{
	mutex_lock(&gvm_ctx->lock);

	debugfs_remove_recursive(gvm_ctx->gvm_dump_dir);
	gvm_ctx->gvm_dump_dir = NULL;

	mutex_unlock(&gvm_ctx->lock);

	list_del(&gvm_ctx->list);

	mutex_destroy(&gvm_ctx->lock);
	kfree(gvm_ctx);
}

void cleanup_gvm_dump_ctx(u16 gvm_id)
{
	struct gvm_dump_ctx *gvm_ctx, *tmp_ctx;

	mutex_lock(&gvm_dump_list_mutex);
	list_for_each_entry_safe(gvm_ctx, tmp_ctx, &gvm_dump_list, list) {
		if (gvm_id == gvm_ctx->vmid) {
			sanitize_gvm_region(gvm_ctx);
			cleanup_single_gvm(gvm_ctx);
		}
	}
	mutex_unlock(&gvm_dump_list_mutex);
}

static int gvm_debugfs_release(struct inode *inode, struct file *file)
{
	struct inode *in = file->f_inode;
	struct gvm_dump_ctx *gvm_prv_ctx = (struct gvm_dump_ctx *)in->i_private;

	if (gvm_prv_ctx->mapped_addr)
		iounmap(gvm_prv_ctx->mapped_addr);

	mutex_unlock(&gvm_prv_ctx->lock);

	return 0;
}

static ssize_t gvm_debugfs_read(struct file *file, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	struct inode *in = file->f_inode;
	struct gvm_dump_ctx *gvm_prv_ctx = (struct gvm_dump_ctx *)in->i_private;

	return simple_read_from_buffer(ubuf, count, ppos, gvm_prv_ctx->mapped_addr,
				       gvm_prv_ctx->dump_size);
}

static int gvm_debugfs_open(struct inode *inode, struct file *file)
{
	struct inode *in = file->f_inode;
	struct gvm_dump_ctx *gvm_prv_ctx = (struct gvm_dump_ctx *)in->i_private;

	mutex_lock(&gvm_prv_ctx->lock);

	gvm_prv_ctx->mapped_addr = ioremap(gvm_prv_ctx->dump_start_addr, gvm_prv_ctx->dump_size);

	if (!gvm_prv_ctx->mapped_addr) {
		pr_err("gvm_dump: %s: ioremap failed\n", __func__);
		mutex_unlock(&gvm_prv_ctx->lock);
		goto free_vm_debugfs;
	}

	return 0;

free_vm_debugfs:
	mutex_lock(&gvm_dump_list_mutex);
	cleanup_single_gvm(gvm_prv_ctx);
	mutex_unlock(&gvm_dump_list_mutex);
	return -EFAULT;
}

static const struct file_operations gvm_dump_ops = {
	.open = gvm_debugfs_open,
	.read = gvm_debugfs_read,
	.release = gvm_debugfs_release
};

static int get_gvm_resource(struct gvm_dump_ctx *gvm_ctx, struct device_node *node)
{

	struct  device_node *gvm_mem_np = NULL;
	struct resource gvm_res = {0,};

	if (of_find_property(node, "enable-gvm-dump", NULL)) {
		if (of_property_read_u32(node, "qcom,vmid", &gvm_ctx->vmid)) {
			pr_err("gvm_dump: %s: error getting \"qcom,vmid\" \n", __func__);
			goto res_fail;
		}

		gvm_mem_np = of_parse_phandle(node, "memory-region", 0);
		if (!gvm_mem_np) {
			pr_err("gvm_dump: %s: error getting \"memory-region\"\n", __func__);
			goto res_fail;
		}

		if (of_address_to_resource(gvm_mem_np, 0, &gvm_res)) {
			pr_err("gvm_dump: %s: of_address_to_resource failed\n", __func__);
			goto res_fail;
		}

		gvm_ctx->dump_start_addr = gvm_res.start;
		gvm_ctx->dump_size = resource_size(&gvm_res);
		gvm_ctx->mapped_addr = NULL;

		return 0;
	}

res_fail:
	if (gvm_mem_np)
		of_node_put(gvm_mem_np);

	return -EINVAL;
}

static int create_gvm_dump_debugfs(struct device_node *node)
{
	struct gvm_dump_ctx *gvm_ctx;
	const char *vm_name;
	int ret;

	/*
	 * Allocate memory for gvm_ctx.
	 */
	gvm_ctx = kzalloc(sizeof(struct gvm_dump_ctx), GFP_KERNEL);
	if (!gvm_ctx)
		return -ENOMEM;

	ret = get_gvm_resource(gvm_ctx, node);
	if (ret)
		goto free_gvm_dump_ctx;

	if (of_property_read_string(node, "qcom,firmware-name", &vm_name)) {
		pr_err("gvm_dump: %s: error getting \"qcom,firmware-name\" \n", __func__);
		goto free_gvm_dump_ctx;
	}

	mutex_init(&gvm_ctx->lock);

	gvm_ctx->gvm_dump_dir = debugfs_create_dir(vm_name, NULL);

	if (!gvm_ctx->gvm_dump_dir) {
		ret = -ENOMEM;
		goto free_gvm_dump_ctx;
	}

	if (!debugfs_create_file("dump", 0444, gvm_ctx->gvm_dump_dir, gvm_ctx, &gvm_dump_ops)) {
		pr_err("gvm_dump: %s: failed to create debugfs file\n", __func__);
		debugfs_remove_recursive(gvm_ctx->gvm_dump_dir);
		ret = -ENOMEM;
		goto free_gvm_dump_ctx;
	}

	mutex_lock(&gvm_dump_list_mutex);
	list_add(&gvm_ctx->list, &gvm_dump_list);
	mutex_unlock(&gvm_dump_list_mutex);

	return 0;

free_gvm_dump_ctx:
	kfree(gvm_ctx);
	return ret;

}

int collect_gvm_dump(struct device_node *node)
{
	int ret;

	if (is_gvm_dump_enabled) {
		ret = create_gvm_dump_debugfs(node);
		if (ret) {
			pr_err("gvm_dump: %s: failed to collect gvm dump %d\n", __func__, ret);
			return ret;
		}
	}

	return 0;

}

/* Cleanup dump context of all GVMs before secure_vm_loader driver unregisters */
void cleanup_gvm_dump_list(void)
{
	struct gvm_dump_ctx *gvm_ctx, *tmp_ctx;

	mutex_lock(&gvm_dump_list_mutex);
	list_for_each_entry_safe(gvm_ctx, tmp_ctx, &gvm_dump_list, list) {
		cleanup_single_gvm(gvm_ctx);
	}
	mutex_unlock(&gvm_dump_list_mutex);
	mutex_destroy(&gvm_dump_list_mutex);
	debugfs_remove_recursive(gvm_dump_enable_dentry);
}

static ssize_t gvm_debugfs_enable_dump_write(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	bool temp;

	if (kstrtobool_from_user(ubuf, count, &temp))
		return -EINVAL;

	is_gvm_dump_enabled = temp;

	return count;
}

static ssize_t gvm_debugfs_enable_dump_read(struct file *file, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	char enable_buff[2];

	snprintf(enable_buff, sizeof(enable_buff), "%d\n", is_gvm_dump_enabled);
	return simple_read_from_buffer(ubuf, count, ppos, enable_buff, sizeof(enable_buff));
}

static const struct file_operations gvm_enable_dump_ops = {
	.read = gvm_debugfs_enable_dump_read,
	.write = gvm_debugfs_enable_dump_write
};

int enable_gvm_dump_debugfs(void)
{
	gvm_dump_enable_dentry = debugfs_create_file("gvm_dump_enable", 0600,
					       NULL, NULL, &gvm_enable_dump_ops);

	if (!gvm_dump_enable_dentry) {
		pr_err("gvm_dump: %s: failed to create debugfs file\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&gvm_dump_list_mutex);

	return 0;
}
