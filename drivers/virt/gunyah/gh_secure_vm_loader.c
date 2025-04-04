// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/version.h>
#include <linux/anon_inodes.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/of_address.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/errno.h>
#include <linux/types.h>

#include "gh_private.h"
#include "gh_secure_vm_virtio_backend.h"
#include "gvm_dump_debugfs.h"

#define PAGE_ROUND_UP(x) ((((u64)(x) + (PAGE_SIZE - 1)) / PAGE_SIZE)  * PAGE_SIZE)

struct gh_sec_vm_fw_mem {
	phys_addr_t fw_phys;
	void *fw_virt;
	ssize_t fw_size;
	bool is_static;
};

struct gh_sec_vm_dev {
	struct list_head list;
	const char *vm_name;
	struct device *dev;
	bool system_vm;
	struct gh_sec_vm_fw_mem *fw_mem_regions;
	unsigned int fw_mem_count;
	int pas_id;
	int vmid;
	unsigned int fw_index;
	struct gh_shmem *sh_mem_regions;
	unsigned int sh_mem_count;
};

const static struct {
	enum gh_vm_names val;
	const char *str;
} fw_name_to_vm_name[] = {
	{GH_PRIMARY_VM, "pvm"},
	{GH_TRUSTED_VM, "trustedvm"},
	{GH_CPUSYS_VM, "cpusys_vm"},
	{GH_OEM_VM, "oemvm"},
	{GH_AUTO_VM, "autoghgvm"},
	{GH_AUTO_VM_LV, "autoghgvmlv"},
};

static DEFINE_SPINLOCK(gh_sec_vm_lock);
static LIST_HEAD(gh_sec_vm_list);

static inline enum gh_vm_names get_gh_vm_name(const char *str)
{
	int vmid;

	for (vmid = 0; vmid < ARRAY_SIZE(fw_name_to_vm_name); ++vmid) {
		if (!strcmp(str, fw_name_to_vm_name[vmid].str))
			return fw_name_to_vm_name[vmid].val;
	}
	return GH_VM_MAX;
}

static struct gh_sec_vm_dev *get_sec_vm_dev_by_name(const char *vm_name)
{
	struct gh_sec_vm_dev *sec_vm_dev;

	spin_lock(&gh_sec_vm_lock);

	list_for_each_entry(sec_vm_dev, &gh_sec_vm_list, list) {
		if (!strcmp(sec_vm_dev->vm_name, vm_name)) {
			spin_unlock(&gh_sec_vm_lock);
			return sec_vm_dev;
		}
	}

	spin_unlock(&gh_sec_vm_lock);

	return NULL;
}

static u64 gh_sec_load_metadata(struct gh_sec_vm_dev *vm_dev,
					void *mdata, size_t mdata_size_act)
{
	struct device *dev = vm_dev->dev;
	const struct elf32_phdr *phdrs;
	const struct elf32_phdr *phdr;
	const struct elf32_hdr *ehdr;
	bool relocatable = false;
	void *metadata_start;
	u64 image_start_addr = 0;
	size_t mdata_size = 0;
	u64 image_end_addr;
	u64 image_size = 0;
	u32 max_paddr = 0;
	u64 moffset = 0;
	u32 fw_index = vm_dev->fw_index;
	int i;

	ehdr = (struct elf32_hdr *)mdata;
	phdrs = (struct elf32_phdr *)(ehdr + 1);

	mdata_size = PAGE_ROUND_UP(mdata_size_act);
	if (mdata_size < mdata_size_act) {
		dev_err(dev, "Overflow detected while calculating metadata size\"%s\"\n",
			vm_dev->vm_name);
		return 0;
	}

	/* Calculate total image size */
	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = &phdrs[i];
		if (phdr->p_flags & QCOM_MDT_RELOCATABLE)
			relocatable = true;

		if (phdr->p_paddr > max_paddr) {
			if (phdr->p_memsz > (U64_MAX - phdr->p_paddr)) {
				dev_err(dev, "Overflow detected while calculating metadata offset\"%s\"\n",
					vm_dev->vm_name);
				return 0;
			}
			image_end_addr = phdr->p_paddr + phdr->p_memsz;
			max_paddr = phdr->p_paddr;
		}
		image_size += phdr->p_memsz;
	}

	if ((image_size > (U64_MAX - mdata_size)) ||
			(vm_dev->fw_mem_regions[fw_index].fw_size < (image_size + mdata_size))) {
		dev_err(dev, "Metadata cannot fit in mem_region  \"%s\"\n",
							vm_dev->vm_name);
		return 0;
	}

	if (!relocatable)
		image_start_addr = vm_dev->fw_mem_regions[fw_index].fw_phys;

	/* Calculate suitable metadata offset */
	moffset = vm_dev->fw_mem_regions[fw_index].fw_size - mdata_size;

	if (moffset > vm_dev->fw_mem_regions[fw_index].fw_size ||
		(image_start_addr > (U64_MAX - moffset)) ||
		((u64) vm_dev->fw_mem_regions[fw_index].fw_virt > (U64_MAX - moffset))) {
		dev_err(dev, "Overflow detected while calculating metadata offset\"%s\"\n",
						vm_dev->vm_name);
		return 0;
	}

	if (image_end_addr <= (image_start_addr + moffset)) {
		metadata_start = vm_dev->fw_mem_regions[fw_index].fw_virt + moffset;
		memcpy(metadata_start, mdata, mdata_size_act);
		return moffset;
	}

	dev_err(dev, "Metadata cannot fit in mem_region %s\n",
						vm_dev->vm_name);
	return 0;
}

static int gh_vm_loader_sec_load(struct gh_sec_vm_dev *vm_dev,
					struct gh_vm *vm)
{
	struct device *dev = vm_dev->dev;
	struct gh_mem_parcel *mem_parcels;
	const struct firmware *fw;
	char fw_name[GH_VM_FW_NAME_MAX];
	size_t metadata_size = 1;
	u64 metadata_offset;
	void *metadata;
	int i;
	u32 fw_index = vm_dev->fw_index;
	int ret = 0;

	scnprintf(fw_name, ARRAY_SIZE(fw_name), "%s.mdt", vm_dev->vm_name);

	ret = request_firmware(&fw, fw_name, dev);
	if (ret) {
		dev_err(dev, "Error requesting fw \"%s\": %d\n", fw_name, ret);
		return ret;
	}

	metadata = qcom_mdt_read_metadata(fw, &metadata_size, fw_name, dev);
	if (IS_ERR(metadata)) {
		release_firmware(fw);
		return PTR_ERR(metadata);
	}

	metadata_offset = gh_sec_load_metadata(vm_dev, metadata, metadata_size);
	if (!metadata_offset) {
		dev_err(dev, "Failed to load metadata \"%s\": %d\n", fw_name, ret);
		goto release_fw;
	}

	ret = qcom_mdt_load_no_init(dev, fw, fw_name, vm_dev->pas_id, vm_dev->fw_mem_regions[fw_index].fw_virt,
				vm_dev->fw_mem_regions[fw_index].fw_phys, vm_dev->fw_mem_regions[fw_index].fw_size, NULL);
	if (ret) {
		dev_err(dev, "Failed to load fw \"%s\": %d\n", fw_name, ret);
		goto release_fw;
	}

	mem_parcels = devm_kcalloc(dev, vm_dev->fw_mem_count, sizeof(*mem_parcels), GFP_KERNEL);

	for (i = 0; i < vm_dev->fw_mem_count; i++) {
		mem_parcels[i].mem_phys = vm_dev->fw_mem_regions[i].fw_phys;
		mem_parcels[i].mem_size = vm_dev->fw_mem_regions[i].fw_size;
	}

	ret = gh_provide_mem(vm, mem_parcels, vm_dev->fw_mem_count,
				vm_dev->system_vm);

	if (ret) {
		dev_err(dev, "Failed to provide memory for %s, %d\n",
						vm_dev->vm_name, ret);
		goto release_fw;
	}

	vm->is_secure_vm = true;

	ret = gh_vm_configure(GH_VM_AUTH_PIL_ELF, metadata_offset,
				metadata_size, 0, 0, vm_dev->pas_id,
				vm_dev->vm_name, vm);
	if (ret)
		dev_err(dev, "Configuring secure VM %s to memory failed %ld\n",
					vm_dev->vm_name, ret);

	for (i = 0; i < vm_dev->sh_mem_count; i++) {
		ret = gh_provide_shmem(vm, &vm_dev->sh_mem_regions[i]);
		if (ret) {
			dev_err(dev, "Failed to provide shared memory for %s, %d\n",
					vm_dev->vm_name, ret);
			goto release_fw;
		}
	}

release_fw:
	kfree(metadata);
	release_firmware(fw);
	return ret;
}

static int gh_sec_vm_loader_load_fw(struct gh_sec_vm_dev *vm_dev,
							struct gh_vm *vm)
{
	enum gh_vm_names vm_name;
	dma_addr_t dma_handle;
	struct device *dev;
	int ret = 0;
	void *virt;
	int i;

	dev = vm_dev->dev;

	vm_name = get_gh_vm_name(vm_dev->vm_name);

	for (i = 0; i < vm_dev->fw_mem_count; i++) {
		if (!vm_dev->fw_mem_regions[i].is_static) {
			virt = dma_alloc_coherent(dev, vm_dev->fw_mem_regions[i].fw_size, &dma_handle,
					GFP_KERNEL);
			if (!virt) {
				ret = -ENOMEM;
				dev_err(dev, "Couldn't allocate cma memory for %s %d\n",
							vm_dev->vm_name, ret);
				return ret;
			}

			vm_dev->fw_mem_regions[i].fw_virt = virt;
			vm_dev->fw_mem_regions[i].fw_phys = dma_to_phys(dev, dma_handle);
		}
	}

	ret = gh_rm_vm_alloc_vmid(vm_name, &vm_dev->vmid);
	if (ret < 0) {
		dev_err(dev, "Couldn't allocate VMID for %s %d\n",
						vm_dev->vm_name, ret);
		for (i = 0; i < vm_dev->fw_mem_count; i++) {
			if (!vm_dev->fw_mem_regions[i].is_static) {
				virt = vm_dev->fw_mem_regions[i].fw_virt;
				dma_handle = phys_to_dma(dev, vm_dev->fw_mem_regions[i].fw_phys);
				dma_free_coherent(dev, vm_dev->fw_mem_regions[i].fw_size, virt, dma_handle);
			}
		}
		return ret;
	}

	vm->status.vm_status = GH_RM_VM_STATUS_LOAD;
	vm->vmid = vm_dev->vmid;

	ret = gh_vm_loader_sec_load(vm_dev, vm);
	if (ret) {
		dev_err(dev, "Loading Secure VM %s failed %d\n",
						vm_dev->vm_name, ret);
		return ret;
	}

	return ret;
}

long gh_vm_ioctl_set_fw_name(struct gh_vm *vm, unsigned long arg)
{
	struct gh_sec_vm_dev *sec_vm_dev;
	struct gh_fw_name vm_fw_name;
	struct device *dev;
	long ret = -EINVAL;

	if (copy_from_user(&vm_fw_name, arg, sizeof(vm_fw_name)))
		return -EFAULT;

	vm_fw_name.name[GH_VM_FW_NAME_MAX - 1] = '\0';
	mutex_lock(&vm->vm_lock);
	if (strlen(vm->fw_name)) {
		pr_err("Secure VM %s already loaded %ld\n",
					vm->fw_name, ret);
		ret = -EEXIST;
		goto err_fw_name;
	}

	sec_vm_dev = get_sec_vm_dev_by_name(vm_fw_name.name);
	if (!sec_vm_dev) {
		pr_err("Requested Secure VM %s not supported\n",
							vm_fw_name.name);
		ret = -EINVAL;
		goto err_fw_name;
	}

	dev = sec_vm_dev->dev;

	cleanup_gvm_ramdump_ctx(sec_vm_dev->vmid);

	ret = gh_sec_vm_loader_load_fw(sec_vm_dev, vm);
	if (ret) {
		dev_err(dev, "Loading secure VM %s to memory failed %ld\n",
					sec_vm_dev->vm_name, ret);
		goto err_fw_name;
	}

	scnprintf(vm->fw_name, ARRAY_SIZE(vm->fw_name),
						"%s", vm_fw_name.name);

	mutex_unlock(&vm->vm_lock);
	gh_uevent_notify_change(GH_EVENT_CREATE_VM, vm);
	return ret;

err_fw_name:
	mutex_unlock(&vm->vm_lock);
	return ret;
}

long gh_vm_ioctl_get_fw_name(struct gh_vm *vm, unsigned long arg)
{
	struct gh_fw_name vm_fw_name;

	mutex_lock(&vm->vm_lock);
	scnprintf(vm_fw_name.name, ARRAY_SIZE(vm_fw_name.name),
						"%s", vm->fw_name);
	mutex_unlock(&vm->vm_lock);

	if (copy_to_user((void __user *)arg, &vm_fw_name, sizeof(vm_fw_name)))
		return -EFAULT;

	return 0;
}

long gh_vm_ioctl_get_mem_count(struct gh_vm *vm)
{
	struct gh_sec_vm_dev *sec_vm_dev;
	u32 mem_count;

	mutex_lock(&vm->vm_lock);

	sec_vm_dev = get_sec_vm_dev_by_name(vm->fw_name);
	if (!sec_vm_dev) {
		pr_err("secure vm %s not supported\n", vm->fw_name);
		mutex_unlock(&vm->vm_lock);
		return -EINVAL;
	}

	mem_count = sec_vm_dev->fw_mem_count;

	mutex_unlock(&vm->vm_lock);

	return mem_count;
}

static int gh_vm_mem_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gh_sec_vm_fw_mem *mem_region = file->private_data;
	size_t mmap_size;

	if (!mem_region)
		return -EINVAL;

	mmap_size = vma->vm_end - vma->vm_start;
	if (mmap_size != mem_region->fw_size)
		return -EINVAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
	vm_flags_set(vma, vma->vm_flags | VM_DONTEXPAND | VM_DONTDUMP);
#else
	vma->vm_flags = vma->vm_flags | VM_DONTEXPAND | VM_DONTDUMP;
#endif

	if (io_remap_pfn_range(vma, vma->vm_start,
			__phys_to_pfn(mem_region->fw_phys),
			mmap_size, vma->vm_page_prot)) {
		pr_err("%s: ioremap_pfn_range failed\n", __func__);
		return -EAGAIN;
	}

	return 0;
}


/**
 * gh_vm_mem_llseek - llseek implementation for non-protected memmory
 * @file:	file structure to seek on
 * @offset:	file offset to seek to
 * @whence:	type of seek
 */

static loff_t gh_vm_mem_llseek(struct file *file, loff_t offset, int whence)
{
	loff_t new_pos;
	struct gh_sec_vm_fw_mem *mem_region = file->private_data;
	ssize_t mem_region_size;

	if (!mem_region)
		return -EINVAL;

	mem_region_size = mem_region->fw_size;
	if (mem_region_size == 0)
		return -EINVAL;

	switch (whence) {
		case SEEK_CUR:
			new_pos = file->f_pos + offset;
			break;
		case SEEK_END:
			new_pos = mem_region_size + offset;
			break;
		case SEEK_SET:
			new_pos = offset;
			break;
		default:
			pr_err("whence %d is not support!\n", whence);
			return -EINVAL;
	}

	if (new_pos < 0 || new_pos > mem_region_size) {
		pr_err("out of boundary\n");
		return -EINVAL;
	}

	file->f_pos = new_pos;

	return new_pos;
}

static const struct file_operations gh_vm_mem_fops = {
	.owner = THIS_MODULE,
	.mmap = gh_vm_mem_mmap,
	.llseek = gh_vm_mem_llseek,
};

long gh_vm_ioctl_get_mem_region(struct gh_vm *vm, unsigned long arg)
{
	struct gh_sec_vm_dev *sec_vm_dev;
	struct vm_mem_region mem_region;
	u8 mem_idx;
	int fd;
	char name[SZ_16];
	struct file *file;

	if (copy_from_user(&mem_region, arg,
				sizeof(mem_region)))
		return -EFAULT;

	mutex_lock(&vm->vm_lock);

	sec_vm_dev = get_sec_vm_dev_by_name(vm->fw_name);
	if (!sec_vm_dev) {
		pr_err("secure vm %s not supported\n", vm->fw_name);
		mutex_unlock(&vm->vm_lock);
		return -EINVAL;
	}

	mem_idx = mem_region.idx;
	if (mem_idx >= sec_vm_dev->fw_mem_count) {
		mutex_unlock(&vm->vm_lock);
		return -EINVAL;
	}

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		mutex_unlock(&vm->vm_lock);
		return -EFAULT;
	}

	snprintf(name, sizeof(name), "gh-mem:%d", mem_idx);
	file = anon_inode_getfile(name, &gh_vm_mem_fops,
			&(sec_vm_dev->fw_mem_regions[mem_idx]), O_RDWR);
	if (IS_ERR(file)) {
		mutex_unlock(&vm->vm_lock);
		return -EFAULT;
	}

	file->f_mode |= FMODE_LSEEK;
	fd_install(fd, file);

	mem_region.fw_phys = sec_vm_dev->fw_mem_regions[mem_idx].fw_phys;
	mem_region.fw_size = sec_vm_dev->fw_mem_regions[mem_idx].fw_size;
	mem_region.fd = fd;

	mutex_unlock(&vm->vm_lock);

	if (copy_to_user(arg, &mem_region,
				sizeof(mem_region)))
		return -EFAULT;

	return 0;
}

int gh_secure_vm_loader_reclaim_fw(struct gh_vm *vm)
{
	struct gh_sec_vm_dev *sec_vm_dev;
	struct gh_mem_parcel *mem_parcels;
	struct device *dev;
	char *fw_name;
	int ret = 0;
	int i;
	bool is_static = true;

	fw_name = vm->fw_name;
	sec_vm_dev = get_sec_vm_dev_by_name(fw_name);
	if (!sec_vm_dev) {
		pr_err("Requested Secure VM %s not supported\n", fw_name);
		return -EINVAL;
	}

	dev = sec_vm_dev->dev;

	for (i = 0; i < sec_vm_dev->sh_mem_count; i ++) {
		ret = gh_reclaim_shmem(vm, &sec_vm_dev->sh_mem_regions[i]);
	}

	mem_parcels = devm_kcalloc(dev, sec_vm_dev->fw_mem_count, sizeof(*mem_parcels), GFP_KERNEL);

	for (i = 0; i < sec_vm_dev->fw_mem_count; i++) {
		mem_parcels[i].mem_phys = sec_vm_dev->fw_mem_regions[i].fw_phys;
		mem_parcels[i].mem_size = sec_vm_dev->fw_mem_regions[i].fw_size;
	}

	ret = gh_reclaim_mem(vm, mem_parcels, sec_vm_dev->fw_mem_count,
					sec_vm_dev->system_vm);

	if (!ret) {
		for (i = 0; i < sec_vm_dev->fw_mem_count; i++) {
			if (!sec_vm_dev->fw_mem_regions[i].is_static) {
				is_static = false;
				dma_free_coherent(dev, sec_vm_dev->fw_mem_regions[i].fw_size, sec_vm_dev->fw_mem_regions[i].fw_virt,
					phys_to_dma(dev, sec_vm_dev->fw_mem_regions[i].fw_phys));
			}
		}

		/* GVM mem reclaim succeeded, collect the GVM ramdump if ramdump collection is enabled */
		if (is_static)
			collect_gvm_ramdump(sec_vm_dev->dev);
	}

	return ret;
}

static int gh_vm_loader_mem_probe(struct gh_sec_vm_dev *sec_vm_dev)
{
	struct device *dev = sec_vm_dev->dev;
	struct reserved_mem *rmem;
	struct device_node *node;
	struct resource res;
	phys_addr_t phys;
	ssize_t size;
	void *virt;
	int i;
	int ret;

	sec_vm_dev->fw_mem_count = of_count_phandle_with_args(dev->of_node, "memory-region", NULL);

	if (!sec_vm_dev->fw_mem_count) {
		dev_err(dev, "No memory regions are specified\n");
		return -EINVAL;
	}

	sec_vm_dev->fw_mem_regions = devm_kcalloc(dev, sec_vm_dev->fw_mem_count,
					sizeof(struct gh_sec_vm_fw_mem), GFP_KERNEL);
	if (!sec_vm_dev->fw_mem_regions)
		return -ENOMEM;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64ULL));
	if (ret) {
		pr_err("%s: dma_set_mask_and_coherent failed\n", __func__);
		return ret;
	}

	for (i = 0; i < sec_vm_dev->fw_mem_count; i++) {
		node = of_parse_phandle(dev->of_node, "memory-region", i);
		if (!node) {
			dev_err(dev, "DT error getting \"memory-region\"\n");
			return -EINVAL;
		}

		if (!of_property_read_bool(node, "no-map")) {
			sec_vm_dev->fw_mem_regions[i].is_static = false;

			ret = of_reserved_mem_device_init_by_idx(dev, dev->of_node, i);
			if (ret) {
				pr_err("%s: Failed to initialize CMA mem, ret %d\n", __func__, ret);
				goto err_of_node_put;
			}

			rmem = of_reserved_mem_lookup(node);
			if (!rmem) {
				ret = -EINVAL;
				pr_err("%s: failed to acquire memory region for %s\n",
					__func__, node->name);
				goto err_of_node_put;
			}

			sec_vm_dev->fw_mem_regions[i].fw_size = rmem->size;
		} else {
			sec_vm_dev->fw_mem_regions[i].is_static = true;
			ret = of_address_to_resource(node, 0, &res);
			if (ret) {
				dev_err(dev, "error %d getting \"memory-region\" resource\n",
					ret);
				goto err_of_node_put;
			}

			phys = res.start;
			size = (size_t)resource_size(&res);
			virt = memremap(phys, size, MEMREMAP_WC);
			if (!virt) {
				dev_err(dev, "Unable to remap firmware memory\n");
				ret = -ENOMEM;
				goto err_of_node_put;
			}

			sec_vm_dev->fw_mem_regions[i].fw_phys = phys;
			sec_vm_dev->fw_mem_regions[i].fw_virt = virt;
			sec_vm_dev->fw_mem_regions[i].fw_size = size;
		}
	}

err_of_node_put:
	of_node_put(node);
	return ret;
}

static int gh_vm_shared_mem_probe(struct gh_sec_vm_dev *sec_vm_dev)
{
	struct device *dev = sec_vm_dev->dev;
	struct device_node *node;
	struct resource res;
	phys_addr_t phys;
	ssize_t size;
	int i;
	int ret;
	int count;

	if (!of_find_property(dev->of_node, "shared-regions", NULL))
		return 0;

	sec_vm_dev->sh_mem_count = of_count_phandle_with_args(dev->of_node, "shared-regions", NULL);

	if (!sec_vm_dev->sh_mem_count) {
		dev_err(dev, "No shared memory regions are specified\n");
		return -EINVAL;
	}

	sec_vm_dev->sh_mem_regions = devm_kcalloc(dev, sec_vm_dev->sh_mem_count,
					sizeof(struct gh_shmem), GFP_KERNEL);
	if (!sec_vm_dev->sh_mem_regions)
		return -ENOMEM;

	for (i = 0; i < sec_vm_dev->sh_mem_count; i++) {
		node = of_parse_phandle(dev->of_node, "shared-regions", i);
		if (!node) {
			dev_err(dev, "DT error getting \"shared-regions\"\n");
			return -EINVAL;
		}

		if (of_find_property(node, "qcom,dst-vmids", NULL)) {
			count = of_property_count_elems_of_size(node, "qcom,dst-vmids", sizeof(u32));
			if (!count) {
				dev_err(dev, "No qcom,dst-vmids are specified\n");
				return -EINVAL;
			} else if (count > GH_MAX_VMIDS) {
				dev_err(dev, "The number of \"qcom,dst-vmids\" exceed limitation\n");
				return -EINVAL;
			}

			sec_vm_dev->sh_mem_regions[i].dst_vmids_count = count;

			ret = of_property_read_u32_array(node, "qcom,dst-vmids", sec_vm_dev->sh_mem_regions[i].dst_vmids, count);
			if (ret) {
				dev_err(dev, "error %d getting \"qcom,dst-vmids\" resource\n", ret);
				goto err_of_node_put;
			}

			ret = of_property_read_u32_array(node, "qcom,dst-perms", sec_vm_dev->sh_mem_regions[i].dst_perms, count);
			if (ret) {
				dev_err(dev, "error %d getting \"qcom,dst-perms\" resource\n", ret);
				goto err_of_node_put;
			}
		} else
			sec_vm_dev->sh_mem_regions[i].dst_vmids_count = 0;

		if (of_find_property(node, "qcom,src-vmids", NULL)) {
			count = of_property_count_elems_of_size(node, "qcom,src-vmids", sizeof(u32));
			if (!count) {
				dev_err(dev, "No qcom,src-vmids are specified\n");
				return -EINVAL;
			} else if (count > GH_MAX_VMIDS) {
				dev_err(dev, "The number of \"qcom,src-vmids\" exceed limitation\n");
				return -EINVAL;
			}

			sec_vm_dev->sh_mem_regions[i].src_vmids_count = count;

			ret = of_property_read_u32_array(node, "qcom,src-vmids", sec_vm_dev->sh_mem_regions[i].src_vmids, count);
			if (ret) {
				dev_err(dev, "error %d getting \"qcom,src-vmids\" resource\n", ret);
				goto err_of_node_put;
			}

			ret = of_property_read_u32_array(node, "qcom,src-perms", sec_vm_dev->sh_mem_regions[i].src_perms, count);
			if (ret) {
				dev_err(dev, "error %d getting \"qcom,src-perms\" resource\n", ret);
				goto err_of_node_put;
			}
		} else
			sec_vm_dev->sh_mem_regions[i].src_vmids_count = 0;

		ret = of_property_read_u32(node, "gunyah-label", &sec_vm_dev->sh_mem_regions[i].gunyah_label);
		if (ret) {
			dev_err(dev, "DT error getting \"gunyah-label\": %d\n", ret);
			return ret;
		}

		if (of_property_read_bool(node, "qcom,is-phantom"))
			sec_vm_dev->sh_mem_regions[i].is_phantom = true;
		else
			sec_vm_dev->sh_mem_regions[i].is_phantom = false;

		if (of_property_read_bool(node, "qcom,is-shared"))
			sec_vm_dev->sh_mem_regions[i].is_shared = true;
		else
			sec_vm_dev->sh_mem_regions[i].is_shared = false;

		ret = of_address_to_resource(node, 0, &res);
		if (ret) {
			dev_err(dev, "error %d getting \"memory-region\" resource\n",
				ret);
			goto err_of_node_put;
		}

		phys = res.start;
		size = (size_t)resource_size(&res);
		sec_vm_dev->sh_mem_regions[i].size = size;
		sec_vm_dev->sh_mem_regions[i].base = phys;
	}

err_of_node_put:
	of_node_put(node);
	return ret;
}

static int gh_secure_vm_loader_probe(struct platform_device *pdev)
{
	struct gh_sec_vm_dev *sec_vm_dev;
	struct device *dev = &pdev->dev;
	enum gh_vm_names vm_name;
	int ret;
	int i;

	sec_vm_dev = devm_kzalloc(dev, sizeof(*sec_vm_dev), GFP_KERNEL);
	if (!sec_vm_dev)
		return -ENOMEM;

	sec_vm_dev->dev = dev;
	platform_set_drvdata(pdev, sec_vm_dev);

	ret = of_property_read_u32(dev->of_node,
				"qcom,pas-id", &sec_vm_dev->pas_id);
	if (ret) {
		dev_err(dev, "DT error getting \"qcom,pas-id\": %d\n", ret);
		return ret;
	}

	sec_vm_dev->system_vm = of_property_read_bool(dev->of_node, "qcom,no-shutdown");
	if (sec_vm_dev->system_vm)
		dev_info(dev, "Vm with no shutdown attribute added\n");


	ret = of_property_read_u32(dev->of_node,
				"qcom,vmid", &sec_vm_dev->vmid);
	if (ret) {
		dev_err(dev, "DT error getting \"qcom,vmid\": %d\n", ret);
		return ret;
	}

	ret = gh_vm_loader_mem_probe(sec_vm_dev);
	if (ret)
		return ret;

	ret = gh_vm_shared_mem_probe(sec_vm_dev);
	if (ret)
		return ret;

	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &sec_vm_dev->vm_name);
	if (ret)
		goto err_unmap_fw;

	ret = of_property_read_u32(dev->of_node,
				"qcom,firmware-index", &sec_vm_dev->fw_index);
	if (ret) {
		dev_err(dev, "DT error getting \"qcom,firmware-index\": %d\n", ret);
		goto err_unmap_fw;
	}

	if (sec_vm_dev->fw_index >= sec_vm_dev->fw_mem_count) {
		dev_err(dev, "firmware index has to be within the firmware regions specified\n");
		ret = -EINVAL;
		goto err_unmap_fw;
	};

	vm_name = get_gh_vm_name(sec_vm_dev->vm_name);
	if (vm_name == GH_VM_MAX) {
		dev_err(dev, "Requested Secure VM %d not supported\n", vm_name);
		ret = -EINVAL;
		goto err_unmap_fw;
	}

	if (get_sec_vm_dev_by_name(sec_vm_dev->vm_name)) {
		dev_err(dev, "Requested Secure VM %s already present\n", sec_vm_dev->vm_name);
		ret = -EINVAL;
		goto err_unmap_fw;
	}

	ret = gh_parse_virtio_properties(dev, sec_vm_dev->vm_name);
	if (ret)
		goto err_unmap_fw;

	spin_lock(&gh_sec_vm_lock);
	list_add(&sec_vm_dev->list, &gh_sec_vm_list);
	spin_unlock(&gh_sec_vm_lock);

	return 0;

err_unmap_fw:
	for (i = 0; i < sec_vm_dev->fw_mem_count; i++)
		if (sec_vm_dev->fw_mem_regions[i].is_static)
			memunmap(sec_vm_dev->fw_mem_regions[i].fw_virt);

	of_reserved_mem_device_release(&pdev->dev);

	return ret;
}

static int gh_secure_vm_loader_remove(struct platform_device *pdev)
{
	struct gh_sec_vm_dev *sec_vm_dev;
	int i;

	sec_vm_dev = platform_get_drvdata(pdev);

	spin_lock(&gh_sec_vm_lock);
	list_del(&sec_vm_dev->list);
	spin_unlock(&gh_sec_vm_lock);

	for (i = 0; i < sec_vm_dev->fw_mem_count; i++)
		if (sec_vm_dev->fw_mem_regions[i].is_static)
			memunmap(sec_vm_dev->fw_mem_regions[i].fw_virt);

	of_reserved_mem_device_release(&pdev->dev);

	return gh_virtio_backend_remove(sec_vm_dev->vm_name);
}

static const struct of_device_id gh_secure_vm_loader_match_table[] = {
	{ .compatible = "qcom,gh-secure-vm-loader" },
	{},
};

static struct platform_driver gh_secure_vm_loader_drv = {
	.probe = gh_secure_vm_loader_probe,
	.remove = gh_secure_vm_loader_remove,
	.driver = {
		.name = "gh_secure_vm_loader",
		.of_match_table = gh_secure_vm_loader_match_table,
	},
};

int gh_secure_vm_loader_init(void)
{
	return platform_driver_register(&gh_secure_vm_loader_drv);
}

void gh_secure_vm_loader_exit(void)
{
	platform_driver_unregister(&gh_secure_vm_loader_drv);
}
