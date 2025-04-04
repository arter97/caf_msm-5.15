// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/of_address.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>

#define GVM_RAMDUMP_PRINT_MARKER	"gh_gvm_ramdump_debugfs"
#define DUMP_INFO_BUFFER_SIZE		512
#define LOAD_CMM_BUFFER_SIZE		512

/*
 * Create the list to add ramdump context of all the GVM during ramdump
 * collecton and delete the context when GVM restarts after ramdump collection.
 */
static LIST_HEAD(gvm_ramdump_list);

/*
 * Define mutex for gvm_ramdump_list to avoid race condition while adding and
 * deleting gvm_ramdump_ctx.
 */
static DEFINE_MUTEX(gvm_ramdump_list_mutex);

static struct dentry *gvm_ramdump_enable_dentry;

/*
 * Define per GVM ramdump context to be used for ramdump collection.
 * vmid - needed during cleanup of GVM ramdump when corresponding
 * GVM restart after ramdump collection.
 * ramdump_start_addr, ramdump_size and mapped_addr for ramdump collection
 * in memremapped address.
 * gvm_ramdump_dir - points to curesponding GVM debugFS ramdump dir and
 * required during cleanup as well.
 * lock is mutex lock to be acquired as ramdump context is shared in
 * ramdump collection and cleanup.
 * list - list_head to add per GVM ramdump ctx in gvm_ramdump_list for cleanup.
 */
struct gvm_ramdump_ctx {
	int vmid;
	int num_mem_regions;
	phys_addr_t *ramdump_start_addr;
	size_t *ramdump_size;
	void **mapped_addr;
	struct dentry *gvm_ramdump_dir;
	struct mutex lock;
	struct list_head list;
};

/* debugFS creation for GVM ramdump is enabled by default */
static bool is_gvm_ramdump_enabled = true;

/* Sanitize the GVM memory region before providing from PVM during VM creation */
#ifdef CONFIG_QCOM_SANITIZE_GVM_MEMORY
static void sanitize_gvm_region(struct gvm_ramdump_ctx *gvm_ctx)
{
	int idx = 0;
	mutex_lock(&gvm_ctx->lock);
	for (idx = 0; idx < gvm_ctx->num_mem_regions; idx++) {
		gvm_ctx->mapped_addr[idx] = memremap(gvm_ctx->ramdump_start_addr[idx],
						     gvm_ctx->ramdump_size[idx], MEMREMAP_WB);
		if (gvm_ctx->mapped_addr[idx]) {
			memset(gvm_ctx->mapped_addr[idx], 0, gvm_ctx->ramdump_size[idx]);
			memunmap(gvm_ctx->mapped_addr[idx]);
		} else
			pr_err("%s: memremap failed\n", GVM_RAMDUMP_PRINT_MARKER);
	}

	mutex_unlock(&gvm_ctx->lock);
}
#else
static inline void sanitize_gvm_region(struct gvm_ramdump_ctx *gvm_ctx) {}
#endif

/* Per GVM ramdump context cleanup in GVM restart after ramdump collection in GVM exit */
static void cleanup_single_gvm(struct gvm_ramdump_ctx *gvm_ctx)
{
	mutex_lock(&gvm_ctx->lock);

	debugfs_remove_recursive(gvm_ctx->gvm_ramdump_dir);
	gvm_ctx->gvm_ramdump_dir = NULL;

	mutex_unlock(&gvm_ctx->lock);
	list_del(&gvm_ctx->list);
	mutex_destroy(&gvm_ctx->lock);
}

void cleanup_gvm_ramdump_ctx(int gvm_id)
{
	struct gvm_ramdump_ctx *gvm_ctx, *tmp_ctx;

	mutex_lock(&gvm_ramdump_list_mutex);
	list_for_each_entry_safe(gvm_ctx, tmp_ctx, &gvm_ramdump_list, list) {
		if (gvm_id == gvm_ctx->vmid) {
			sanitize_gvm_region(gvm_ctx);
			cleanup_single_gvm(gvm_ctx);
		}
	}
	mutex_unlock(&gvm_ramdump_list_mutex);
}

void cleanup_gvm_ramdump_list(void)
{
	struct gvm_ramdump_ctx *gvm_ctx, *tmp_ctx;

	mutex_lock(&gvm_ramdump_list_mutex);
	list_for_each_entry_safe(gvm_ctx, tmp_ctx, &gvm_ramdump_list, list) {
		cleanup_single_gvm(gvm_ctx);
	}
	mutex_unlock(&gvm_ramdump_list_mutex);
	mutex_destroy(&gvm_ramdump_list_mutex);
	debugfs_remove_recursive(gvm_ramdump_enable_dentry);
}

static int get_ramdump_id(struct file *file, int num_mem_regions)
{
	char* name;
	int ramdump_id;

	name = strchr(file->f_path.dentry->d_iname, '.');
	if (name) {
		name--;
		if (!isdigit(*name)) {
			pr_err("%s: Not a valid file \n", GVM_RAMDUMP_PRINT_MARKER);
			return -EINVAL;
		}
		ramdump_id = simple_strtol(name, NULL, 10);
		if (ramdump_id >= num_mem_regions) {
			pr_err("%s: Not a valid file \n", GVM_RAMDUMP_PRINT_MARKER);
			return -EINVAL;
		}

		return ramdump_id;
	}

	pr_err("%s: Not a valid file \n", GVM_RAMDUMP_PRINT_MARKER);

	return -EINVAL;
}

static int gvm_debugfs_release(struct inode *inode, struct file *file)
{
	struct inode *in = file->f_inode;
	struct gvm_ramdump_ctx *gvm_prv_ctx = (struct gvm_ramdump_ctx *)in->i_private;
	int ramdump_id;

	ramdump_id = get_ramdump_id(file, gvm_prv_ctx->num_mem_regions);
	if (ramdump_id < 0)
		return -1;

	if (gvm_prv_ctx->mapped_addr[ramdump_id])
		memunmap(gvm_prv_ctx->mapped_addr[ramdump_id]);

	mutex_unlock(&gvm_prv_ctx->lock);

	return 0;
}

static ssize_t gvm_debugfs_read(struct file *file, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	struct inode *in = file->f_inode;
	struct gvm_ramdump_ctx *gvm_prv_ctx = (struct gvm_ramdump_ctx *)in->i_private;
	int ramdump_id;

	ramdump_id = get_ramdump_id(file, gvm_prv_ctx->num_mem_regions);
	if (ramdump_id < 0)
		return -1;

	ret = simple_read_from_buffer(ubuf, count, ppos, gvm_prv_ctx->mapped_addr[ramdump_id],
			gvm_prv_ctx->ramdump_size[ramdump_id]);
	if (ret < 0) {
		pr_err("%s: Read from buffer failed %d\n", GVM_RAMDUMP_PRINT_MARKER, ret);
		return -1;
	}

	return ret;
}

static int gvm_debugfs_open(struct inode *inode, struct file *file)
{
	struct inode *in = file->f_inode;
	struct gvm_ramdump_ctx *gvm_prv_ctx = (struct gvm_ramdump_ctx *)in->i_private;
	int ramdump_id;

	ramdump_id = get_ramdump_id(file, gvm_prv_ctx->num_mem_regions);
	if (ramdump_id < 0)
		goto free_vm_debugfs;

	mutex_lock(&gvm_prv_ctx->lock);

	gvm_prv_ctx->mapped_addr[ramdump_id] = memremap(gvm_prv_ctx->ramdump_start_addr[ramdump_id],
							gvm_prv_ctx->ramdump_size[ramdump_id], MEMREMAP_WB);
	if (!gvm_prv_ctx->mapped_addr[ramdump_id]) {
		pr_err("%s: memremap failed\n", GVM_RAMDUMP_PRINT_MARKER);
		mutex_unlock(&gvm_prv_ctx->lock);
		goto free_vm_debugfs;
	}

	return 0;

free_vm_debugfs:
	mutex_lock(&gvm_ramdump_list_mutex);
	cleanup_single_gvm(gvm_prv_ctx);
	mutex_unlock(&gvm_ramdump_list_mutex);

	return -EFAULT;
}

static ssize_t dump_info_read(struct file *file, char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	struct inode *in = file->f_inode;
	struct gvm_ramdump_ctx *gvm_prv_ctx = (struct gvm_ramdump_ctx *)in->i_private;
	char d_info_buf[DUMP_INFO_BUFFER_SIZE] = {'\0'};
	const char* header = "pref\t\tbase\t\tlength\t\tregion\t\tfile name\t\t\n";
	const char* dot_line = "--------------------------------------------------------------------------\n";
	const char* d_info_pref = "1\t\t";
	const char* reg_name = "DDR\t\t";
	size_t header_len = strlen(header);
	size_t dot_line_len = strlen(dot_line);
	size_t pref_len = strlen(d_info_pref);
	size_t reg_len = strlen(reg_name);
	int curr_buf_len = 0;
	int idx;
	int append_len;

	strlcpy(d_info_buf, header, sizeof(d_info_buf));
	strlcat(d_info_buf, dot_line, sizeof(d_info_buf));
	curr_buf_len = header_len + dot_line_len;
	for(idx = 0; idx < gvm_prv_ctx->num_mem_regions; idx++) {
		if (curr_buf_len >= DUMP_INFO_BUFFER_SIZE || curr_buf_len + pref_len >= DUMP_INFO_BUFFER_SIZE) {
			pr_err("%s: Insufficient dump info buffer space\n", GVM_RAMDUMP_PRINT_MARKER);
			return -1;
		}

		strlcat(d_info_buf, d_info_pref, sizeof(d_info_buf));
		curr_buf_len += pref_len;
		append_len = snprintf(d_info_buf + curr_buf_len, sizeof(d_info_buf) - curr_buf_len,
					  "0x%lx\t", gvm_prv_ctx->ramdump_start_addr[idx]);
		curr_buf_len += append_len;
		if (curr_buf_len >= DUMP_INFO_BUFFER_SIZE) {
			pr_err("%s: Insufficient dump info buffer space\n", GVM_RAMDUMP_PRINT_MARKER);
			return -1;
		}

		append_len = snprintf(d_info_buf + curr_buf_len, sizeof(d_info_buf) - curr_buf_len,
				      "%lu\t", gvm_prv_ctx->ramdump_size[idx]);
		curr_buf_len += append_len;
		if (curr_buf_len >= DUMP_INFO_BUFFER_SIZE || curr_buf_len + reg_len >= DUMP_INFO_BUFFER_SIZE) {
			pr_err("%s: Insufficient dump info buffer space\n", GVM_RAMDUMP_PRINT_MARKER);
			return -1;
		}

		strlcat(d_info_buf, reg_name, sizeof(d_info_buf));
		curr_buf_len += reg_len;
		append_len = snprintf(d_info_buf + curr_buf_len, sizeof(d_info_buf) - curr_buf_len,
				      "VMDDRCS%d.bin\n", idx);
		curr_buf_len += append_len;
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, d_info_buf, sizeof(d_info_buf));
	if (ret < 0) {
		pr_err("%s: Read from buffer failed %d\n", GVM_RAMDUMP_PRINT_MARKER);
		return -1;
	}

	return ret;
}

static ssize_t load_cmm_read(struct file *file, char __user *ubuf,
                              size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	struct inode *in = file->f_inode;
	struct gvm_ramdump_ctx *gvm_prv_ctx = (struct gvm_ramdump_ctx *)in->i_private;
	char load_cmm_buf[LOAD_CMM_BUFFER_SIZE] = {'\0'};
	const char* cmm_cmd = "d.load.binary ";
	size_t cmm_cmd_len = strlen(cmm_cmd);
	const char* cmm_cmd_2 = "/noclear\n";
	size_t cmm_cmd2_len = strlen(cmm_cmd_2);
	int curr_buf_len = 0;
	int idx;
	int append_len;

	for(idx = 0; idx < gvm_prv_ctx->num_mem_regions; idx++) {
		if (curr_buf_len >= LOAD_CMM_BUFFER_SIZE || curr_buf_len + cmm_cmd_len >= LOAD_CMM_BUFFER_SIZE) {
			pr_err("%s: Insufficient dump info buffer space\n", GVM_RAMDUMP_PRINT_MARKER);
			return -1;
		}

		strlcat(load_cmm_buf, cmm_cmd, sizeof(load_cmm_buf));
		curr_buf_len += cmm_cmd_len;
		append_len = snprintf(load_cmm_buf + curr_buf_len, sizeof(load_cmm_buf) - curr_buf_len,
					  "VMDDRCS%d.bin ", idx);
		curr_buf_len += append_len;
		if (curr_buf_len >= LOAD_CMM_BUFFER_SIZE) {
			pr_err("%s: Insufficient dump info buffer space\n", GVM_RAMDUMP_PRINT_MARKER);
			return -1;
		}

		append_len = snprintf(load_cmm_buf + curr_buf_len, sizeof(load_cmm_buf) - curr_buf_len,
				      "0x%lx ", gvm_prv_ctx->ramdump_start_addr[idx]);
		curr_buf_len += append_len;
		if (curr_buf_len >= LOAD_CMM_BUFFER_SIZE) {
			pr_err("%s: Insufficient dump info buffer space\n", GVM_RAMDUMP_PRINT_MARKER);
			return -1;
		}

		strlcat(load_cmm_buf, cmm_cmd_2, sizeof(load_cmm_buf));
		curr_buf_len += cmm_cmd2_len;
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, load_cmm_buf, sizeof(load_cmm_buf));
	if (ret < 0) {
		pr_err("%s: Read from buffer failed %d\n", GVM_RAMDUMP_PRINT_MARKER);
		return -1;
	}

	return ret;
}

static const struct file_operations gvm_ramdump_ops = {
	.open = gvm_debugfs_open,
	.read = gvm_debugfs_read,
	.release = gvm_debugfs_release
};

static const struct file_operations dump_info_ops = {
	.read = dump_info_read
};

static const struct file_operations load_cmm_ops = {
	.read = load_cmm_read
};

static int get_gvm_resource(struct gvm_ramdump_ctx *gvm_ctx, struct device *dev)
{

	struct  device_node *gvm_mem_np = NULL;
	struct resource gvm_res = {0,};
	int idx = 0;

	if (of_find_property(dev->of_node, "enable-gvm-dump", NULL)) {
		if (of_property_read_s32(dev->of_node, "qcom,vmid", &gvm_ctx->vmid)) {
			dev_err(dev, "%s: error getting \"qcom,vmid\" \n", GVM_RAMDUMP_PRINT_MARKER);
			goto res_fail;
		}

		gvm_ctx->num_mem_regions = of_count_phandle_with_args(dev->of_node, "memory-region", NULL);
		if (!gvm_ctx->num_mem_regions) {
			dev_err(dev, "%s: error getting \"memory-region\" \n", GVM_RAMDUMP_PRINT_MARKER);
			goto res_fail;
		}

		gvm_ctx->ramdump_start_addr = devm_kzalloc(dev, sizeof(phys_addr_t), GFP_KERNEL);
		if (!gvm_ctx->ramdump_start_addr)
			return -ENOMEM;

		gvm_ctx->ramdump_size = devm_kzalloc(dev, sizeof(size_t), GFP_KERNEL);
		if (!gvm_ctx->ramdump_size)
			return -ENOMEM;

		gvm_ctx->mapped_addr = devm_kzalloc(dev, sizeof(void*), GFP_KERNEL);
		if (!gvm_ctx->mapped_addr)
			return -ENOMEM;

		for (idx = 0; idx < gvm_ctx->num_mem_regions; idx++) {
			gvm_mem_np = of_parse_phandle(dev->of_node, "memory-region", idx);
			if (!gvm_mem_np) {
				dev_err(dev, "%s: error getting \"memory-region\" \n", GVM_RAMDUMP_PRINT_MARKER);
				goto res_fail;
			}

			if (of_address_to_resource(gvm_mem_np, 0, &gvm_res)) {
				dev_err(dev, "%s: of_address_to_resource failed \n", GVM_RAMDUMP_PRINT_MARKER);
				goto res_fail;
			}

			gvm_ctx->ramdump_start_addr[idx] = gvm_res.start;
			gvm_ctx->ramdump_size[idx] = resource_size(&gvm_res);
		}

		return 0;
	}

res_fail:
	if (gvm_mem_np)
		of_node_put(gvm_mem_np);

	return -EINVAL;
}

static int create_gvm_ramdump_debugfs(struct device *dev)
{
	struct gvm_ramdump_ctx *gvm_ctx;
	const char *vm_name;
	int ret;
	int idx = 0;
	char name[SZ_16];

	gvm_ctx = devm_kzalloc(dev, sizeof(struct gvm_ramdump_ctx), GFP_KERNEL);
	if (!gvm_ctx)
		return -ENOMEM;

	gvm_ctx->ramdump_size = NULL;
	gvm_ctx->ramdump_start_addr = NULL;
	gvm_ctx->mapped_addr = NULL;

	ret = get_gvm_resource(gvm_ctx, dev);
	if (ret)
		return ret;

	ret = of_property_read_string(dev->of_node, "qcom,firmware-name", &vm_name);
	if (ret) {
		dev_err(dev, "%s: Error getting \"qcom,firmware-name\" \n", GVM_RAMDUMP_PRINT_MARKER);
		return ret;
	}

	mutex_init(&gvm_ctx->lock);

	gvm_ctx->gvm_ramdump_dir = debugfs_create_dir(vm_name, NULL);
	if (IS_ERR_OR_NULL(gvm_ctx->gvm_ramdump_dir)) {
		dev_err(dev, "debugfs_create_dir fail, error %ld\n", PTR_ERR(gvm_ctx->gvm_ramdump_dir));
		gvm_ctx->gvm_ramdump_dir = NULL;
		return -EINVAL;
	}

	for (idx = 0; idx < gvm_ctx->num_mem_regions; idx++) {
		scnprintf(name, sizeof(name), "VMDDRCS%d.bin", idx);
		debugfs_create_file_size(name, 0444, gvm_ctx->gvm_ramdump_dir,
					 gvm_ctx, &gvm_ramdump_ops, gvm_ctx->ramdump_size[idx]);
	}

	debugfs_create_file_size("dump_info.txt", 0444, gvm_ctx->gvm_ramdump_dir,
				 gvm_ctx, &dump_info_ops, DUMP_INFO_BUFFER_SIZE);
	debugfs_create_file_size("load.cmm", 0444, gvm_ctx->gvm_ramdump_dir,
				 gvm_ctx, &load_cmm_ops, LOAD_CMM_BUFFER_SIZE);
	mutex_lock(&gvm_ramdump_list_mutex);
	list_add(&gvm_ctx->list, &gvm_ramdump_list);
	mutex_unlock(&gvm_ramdump_list_mutex);

	return 0;
}

int collect_gvm_ramdump(struct device *dev)
{
	int ret;

	if (is_gvm_ramdump_enabled) {
		ret = create_gvm_ramdump_debugfs(dev);
		if (ret) {
			dev_err(dev, "%s: failed to collect gvm ramdump %d\n", GVM_RAMDUMP_PRINT_MARKER, ret);
			return ret;
		}
	}

	return 0;
}

static ssize_t gvm_debugfs_enable_ramdump_write(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	bool temp;

	if (kstrtobool_from_user(ubuf, count, &temp))
		return -EINVAL;

	is_gvm_ramdump_enabled = temp;

	return count;
}

static ssize_t gvm_debugfs_enable_ramdump_read(struct file *file, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	char enable_buf[40] = {'\0'};

	snprintf(enable_buf, sizeof(enable_buf), "%d\n", is_gvm_ramdump_enabled);

	return simple_read_from_buffer(ubuf, count, ppos, enable_buf, strlen(enable_buf));
}

static const struct file_operations gvm_enable_ramdump_ops = {
	.read = gvm_debugfs_enable_ramdump_read,
	.write = gvm_debugfs_enable_ramdump_write
};

int enable_gvm_ramdump_debugfs(void)
{
	gvm_ramdump_enable_dentry = debugfs_create_file("gvm_ramdump_enable", 0600,
					       NULL, NULL, &gvm_enable_ramdump_ops);
	if (IS_ERR_OR_NULL(gvm_ramdump_enable_dentry)) {
		pr_err("debugfs_create_file failed, error %ld\n", PTR_ERR(gvm_ramdump_enable_dentry));
		gvm_ramdump_enable_dentry = NULL;
		return -EINVAL;
	}

	mutex_init(&gvm_ramdump_list_mutex);

	return 0;
}
