// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/of_address.h>
#include <linux/qcom_scm.h>

#include "coresight-priv.h"
#include "coresight-common.h"
#include "coresight-tmc.h"

#define APSS 1
#define MPSS 2
#define ETR1 1

/*
 * struct secure_etr_buf description of secure etr buffer
 * @base：virtual address of the buffer.
 * @paddr: physical address of the buffer.
 * @size: size of the buffer.
 */

struct secure_etr_buf {
	void __iomem	*base;
	phys_addr_t	paddr;
	size_t	size;
};

/*
 * struct secure_etr_drvdata description of secure etr driver data
 * @dev:	the device entity associated to this component.
 * @csdev: standard CoreSight device information.
 * @real_name: real etr device name associated with it.
 * @real_sink: real etr drvdata.
 * @sram_node: name of handle "/dev/xxx.tmc" entry
 * @etm_inst_id: supported secure etm inst_id.
 * @secure_etr_buf: secure etr buffer.
 * @mode: how this TMC is being used.
 * @coresight_csr: related csr.
 * @csr_name: name of related csr.
 * @atid_offset: atid register offset of csr.
 * @clk: clock of etr
 */
struct secure_etr_drvdata {
	struct device	*dev;
	struct coresight_device	*csdev;
	const char	*sram_node;
	struct cdev	sram_dev;
	struct class	*sram_class;
	const char	*real_name;
	struct tmc_drvdata	*real_sink;
	uint32_t	etm_inst_id;

	struct mutex	mem_lock;
	spinlock_t	spinlock;
	bool	reading;
	u32	mode;

	struct secure_etr_buf	*etr_buf;

	struct coresight_csr	*csr;
	const char	*csr_name;
	u32	atid_offset;
	struct clk	*clk;
};

DEFINE_CORESIGHT_DEVLIST(secure_etr_devs, "secure_etr");

static int coresight_sink_by_id(struct device *dev, const void *data)
{
	struct coresight_device *csdev = to_coresight_device(dev);
	unsigned long hash;

	if (csdev->type == CORESIGHT_DEV_TYPE_SINK ||
	     csdev->type == CORESIGHT_DEV_TYPE_LINKSINK) {

		if (!csdev->ea)
			return 0;
		/*
		 * See function etm_perf_add_symlink_sink() to know where
		 * this comes from.
		 */
		hash = (unsigned long)csdev->ea->var;

		if ((u32)hash == *(u32 *)data)
			return 1;
	}

	return 0;
}

static struct tmc_drvdata *coresight_get_real_dev(
			struct secure_etr_drvdata *drvdata, u32 id)
{
	struct device *dev = NULL;

	dev = bus_find_device(drvdata->csdev->dev.bus, NULL, &id,
			      coresight_sink_by_id);

	return dev ? dev_get_drvdata(dev->parent) : NULL;
}

static ssize_t buffer_size_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct secure_etr_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%#x\n", drvdata->etr_buf->size);
}

static DEVICE_ATTR_RO(buffer_size);

static struct attribute *coresight_secure_etr_attrs[] = {
	&dev_attr_buffer_size.attr,
	NULL,
};

static const struct attribute_group coresight_secure_etr_group = {
	.attrs = coresight_secure_etr_attrs,
};

static const struct attribute_group *coresight_secure_etr_groups[] = {
	&coresight_secure_etr_group,
	NULL,
};

/*
 *secure_etr_map_mem_permission - assign the reserved memory region to mpss
 */
static int secure_etr_map_mem_permission(struct secure_etr_buf *etr_buf)
{
	struct qcom_scm_vmperm dst_perms;
	unsigned int src_perms;
	int ret;

	src_perms = BIT(QCOM_SCM_VMID_HLOS);

	dst_perms.vmid = QCOM_SCM_VMID_MSS_MSA;
	dst_perms.perm = QCOM_SCM_PERM_RW;

	ret = qcom_scm_assign_mem(etr_buf->paddr, etr_buf->size,
					&src_perms, &dst_perms, 1);
	return ret;
}

/*
 *secure_etr_map_mem_permission - assign the reserved memory region to apss
 */
static int secure_etr_unmap_mem_permission(struct secure_etr_buf *etr_buf)
{
	struct qcom_scm_vmperm dst_perms;
	unsigned int src_perms;
	int ret;

	src_perms = BIT(QCOM_SCM_VMID_MSS_MSA);

	dst_perms.vmid = QCOM_SCM_VMID_HLOS;
	dst_perms.perm = QCOM_SCM_PERM_RW;

	ret = qcom_scm_assign_mem(etr_buf->paddr, etr_buf->size,
				&src_perms, &dst_perms, 1);
	return ret;
}

static int secure_etr_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct secure_etr_drvdata *drvdata = container_of(inode->i_cdev,
				struct secure_etr_drvdata, sram_dev);
	mutex_lock(&drvdata->mem_lock);
	if (drvdata->reading) {
		ret = -EBUSY;
		goto out;
	}

	if (drvdata->mode != CS_MODE_SYSFS) {
		ret = -EINVAL;
		goto out;
	}
	/*
	 * reclaim ownership of ETR.
	 * assign the reserved memory region to apss.
	 */
	ret = remote_etm_etr_assign(drvdata->etm_inst_id, APSS, ETR1,
			drvdata->etr_buf->paddr, drvdata->etr_buf->size);
	if (ret)
		goto out;

	secure_etr_unmap_mem_permission(drvdata->etr_buf);
	drvdata->reading = true;
	file->private_data = drvdata;
out:
	mutex_unlock(&drvdata->mem_lock);
	if (!ret)
		nonseekable_open(inode, file);
	return ret;
}

static ssize_t secure_etr_read(struct file *file, char __user *data,
			size_t len, loff_t *ppos)
{
	struct secure_etr_drvdata *drvdata = file->private_data;
	struct secure_etr_buf *etr_buf = drvdata->etr_buf;

	mutex_lock(&drvdata->mem_lock);

	if ((*ppos + len) > etr_buf->size || (*ppos + len) < len)
		len = etr_buf->size - *ppos;
	if (len <= 0) {
		mutex_unlock(&drvdata->mem_lock);
		return len;
	}

	if (copy_to_user(data, (etr_buf->base + *ppos), len)) {
		dev_dbg(drvdata->dev,
			"%s: copy_to_user failed\n", __func__);
		mutex_unlock(&drvdata->mem_lock);
		return -EFAULT;
	}

	*ppos += len;

	dev_dbg(drvdata->dev, "%zu bytes copied\n", len);

	mutex_unlock(&drvdata->mem_lock);
	return len;
}
static int secure_etr_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct secure_etr_drvdata *drvdata = file->private_data;

	mutex_lock(&drvdata->mem_lock);

	/*
	 * assign the reserved memory region to mpss.
	 * assign ownership of ETR to mpss.
	 * re-enable secure remote etm.
	 */
	secure_etr_map_mem_permission(drvdata->etr_buf);
	ret = remote_etm_etr_assign(drvdata->etm_inst_id, MPSS, ETR1,
			drvdata->etr_buf->paddr, drvdata->etr_buf->size);
	if (ret)
		goto out;
	ret = remote_etm_reenable(drvdata->etm_inst_id);
	if (ret)
		goto out;
	drvdata->reading = false;

out:
	mutex_unlock(&drvdata->mem_lock);
	return ret;
}

static const struct file_operations secure_etr_fops = {
	.owner		= THIS_MODULE,
	.open		= secure_etr_open,
	.read		= secure_etr_read,
	.release	= secure_etr_release,
	.llseek		= no_llseek,
};

static int sec_etr_sram_dev_register(struct secure_etr_drvdata *drvdata)
{
	int ret;
	struct device *device;
	dev_t dev;

	ret = alloc_chrdev_region(&dev, 0, 1, drvdata->sram_node);
	if (ret)
		goto err_alloc;

	cdev_init(&drvdata->sram_dev, &secure_etr_fops);

	drvdata->sram_dev.owner = THIS_MODULE;
	ret = cdev_add(&drvdata->sram_dev, dev, 1);
	if (ret)
		goto err_cdev_add;

	drvdata->sram_class = class_create(THIS_MODULE,
					   drvdata->sram_node);
	if (IS_ERR(drvdata->sram_class)) {
		ret = PTR_ERR(drvdata->sram_class);
		goto err_class_create;
	}

	device = device_create(drvdata->sram_class, NULL,
			       drvdata->sram_dev.dev, drvdata,
			       drvdata->sram_node);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		goto err_dev_create;
	}

	return 0;
err_dev_create:
	class_destroy(drvdata->sram_class);
err_class_create:
	cdev_del(&drvdata->sram_dev);
err_cdev_add:
	unregister_chrdev_region(drvdata->sram_dev.dev, 1);
err_alloc:
	return ret;
}

static void sec_etr_sram_dev_deregister(struct secure_etr_drvdata *drvdata)
{
	device_destroy(drvdata->sram_class, drvdata->sram_dev.dev);
	class_destroy(drvdata->sram_class);
	cdev_del(&drvdata->sram_dev);
	unregister_chrdev_region(drvdata->sram_dev.dev, 1);
}

static int enable_secure_etr_sink(struct coresight_device *csdev,
			       u32 mode, void *data)
{
	struct secure_etr_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret = 0;
	u32 hash;
	struct tmc_drvdata *real_sink;

	mutex_lock(&drvdata->mem_lock);
	if (drvdata->reading) {
		ret = -EBUSY;
		goto unlock_out;
	}

	if (drvdata->mode == CS_MODE_SYSFS) {
		atomic_inc(csdev->refcnt);
		goto unlock_out;
	}
	/*
	 * check the status of real ETR. If real ETR is enabled,
	 * secure ETR cannot be enabled.
	 */
	hash = hashlen_hash(hashlen_string(NULL, drvdata->real_name));
	real_sink = coresight_get_real_dev(drvdata, hash);
	if (!real_sink) {
		dev_info(drvdata->dev, "real_sink config error\n");
		ret = -EINVAL;
		goto unlock_out;
	}
	if (real_sink->mode == CS_MODE_SYSFS ||
				real_sink->mode == CS_MODE_PERF) {
		dev_info(drvdata->dev, "%s is enabled, please disable it\n",
			drvdata->real_name);
		ret = -EBUSY;
		goto unlock_out;
	}

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		goto unlock_out;
	/*
	 * assign the reserved memory region to mpss.
	 * assign ownership of ETR to mpss.
	 */
	secure_etr_map_mem_permission(drvdata->etr_buf);
	ret = remote_etm_etr_assign(drvdata->etm_inst_id, MPSS, ETR1,
			drvdata->etr_buf->paddr, drvdata->etr_buf->size);
	if (ret)
		goto err;

	dev_info(drvdata->dev, "modem etr enable\n");
	drvdata->mode = CS_MODE_SYSFS;
	/*
	 * change real etr mode to CS_MODE_SEC. This ensures that
	 * real ETR cannot be enabled when secure etr is used.
	 */
	real_sink->mode = CS_MODE_SEC;
	drvdata->real_sink = real_sink;
	atomic_inc(csdev->refcnt);
	goto unlock_out;

err:
	secure_etr_unmap_mem_permission(drvdata->etr_buf);
	clk_disable_unprepare(drvdata->clk);
unlock_out:
	mutex_unlock(&drvdata->mem_lock);
	return ret;
}

static int disable_secure_etr_sink(struct coresight_device *csdev)
{
	struct secure_etr_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret = 0;

	mutex_lock(&drvdata->mem_lock);
	if (drvdata->reading) {
		ret = -EBUSY;
		goto unlock_out;
	}

	if (atomic_dec_return(csdev->refcnt)) {
		ret = -EBUSY;
		goto unlock_out;
	}
	/*
	 * reclaim ownership of ETR.
	 * assign the reserved memory region to apss.
	 */
	ret = remote_etm_etr_assign(drvdata->etm_inst_id, APSS, ETR1,
				drvdata->etr_buf->paddr, drvdata->etr_buf->size);
	if (ret)
		dev_err(drvdata->dev, "assign etr to apss fail\n");

	secure_etr_unmap_mem_permission(drvdata->etr_buf);

	dev_info(drvdata->dev, "disable modem etr\n");
	drvdata->mode = CS_MODE_DISABLED;
	if (drvdata->real_sink) {
		drvdata->real_sink->mode = CS_MODE_DISABLED;
		drvdata->real_sink = NULL;
	}

unlock_out:
	clk_disable_unprepare(drvdata->clk);
	mutex_unlock(&drvdata->mem_lock);
	return ret;
}

static const struct coresight_ops_sink secure_etr_sink_ops = {
	.enable		= enable_secure_etr_sink,
	.disable	= disable_secure_etr_sink,
};

const struct coresight_ops secure_etr_cs_ops = {
	.sink_ops	= &secure_etr_sink_ops,
};

/*
 * secure_etr_map_memory - initialize reserved memory region.
 */

static int secure_etr_map_memory(struct secure_etr_drvdata *drvdata, const char *region)
{
	struct device_node *np;
	struct resource r;
	resource_size_t size;
	int ret;
	struct secure_etr_buf *etr_buf;

	np = of_parse_phandle(drvdata->dev->of_node, region, 0);
	if (!np) {
		dev_err(drvdata->dev, "No %s specified\n", region);
		return -EINVAL;
	}
	ret = of_address_to_resource(np, 0, &r);
	of_node_put(np);
	if (ret)
		return ret;

	etr_buf = devm_kzalloc(drvdata->dev, sizeof(*etr_buf), GFP_KERNEL);
	if (!etr_buf)
		return -ENOMEM;

	size = resource_size(&r);
	etr_buf->base = devm_ioremap_wc(drvdata->dev, r.start, size);
	if (!etr_buf->base)
		return -ENOMEM;
	etr_buf->paddr = (phys_addr_t)r.start;
	etr_buf->size = size;
	drvdata->etr_buf = etr_buf;

	return 0;
}

static int secure_etr_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct secure_etr_drvdata *drvdata;
	struct coresight_desc desc = { 0 };

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = dev;
	platform_set_drvdata(pdev, drvdata);
	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	dev->platform_data = pdata;

	spin_lock_init(&drvdata->spinlock);
	mutex_init(&drvdata->mem_lock);

	drvdata->clk = devm_clk_get(dev, "apb_pclk");

	if (IS_ERR(drvdata->clk))
		dev_err(dev, "not config clk\n");

	ret = of_property_read_string(dev->of_node, "real-name",
							&drvdata->real_name);
	if (ret)
		return ret;

	ret = of_property_read_u32(dev->of_node, "qdss,support-remote-etm",
							&drvdata->etm_inst_id);
	if (ret)
		return ret;

	ret = of_get_coresight_csr_name(dev->of_node, &drvdata->csr_name);
	if (ret)
		dev_err(dev, "No csr data\n");
	else {
		drvdata->csr = coresight_csr_get(drvdata->csr_name);
		if (IS_ERR(drvdata->csr)) {
			dev_err(dev, "failed to get csr, defer probe\n");
			return -EPROBE_DEFER;
		}
	}
	of_property_read_u32(dev->of_node, "csr-atid-offset",
			&drvdata->atid_offset);

	ret = secure_etr_map_memory(drvdata, "memory-region");
	if (ret)
		return ret;

	desc.name = coresight_alloc_device_name(&secure_etr_devs, dev);
	if (!desc.name)
		return -ENOMEM;

	desc.dev = dev;
	desc.pdata = pdata;
	desc.type = CORESIGHT_DEV_TYPE_SINK;
	desc.subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_SYSMEM;
	desc.ops = &secure_etr_cs_ops;
	desc.groups = coresight_secure_etr_groups;

	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		return ret;
	}

	drvdata->sram_node = desc.name;
	ret = sec_etr_sram_dev_register(drvdata);
	if (ret) {
		coresight_unregister(drvdata->csdev);
		return ret;
	}

	pm_runtime_enable(dev);
	return 0;
}

static int secure_etr_remove(struct platform_device *pdev)
{
	struct secure_etr_drvdata *drvdata = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	sec_etr_sram_dev_deregister(drvdata);
	coresight_unregister(drvdata->csdev);
	return 0;
}

static const struct of_device_id secure_etr_match[] = {
	{.compatible = "qcom,coresight-secure-etr"},
	{}
};

static struct platform_driver secure_etr_driver = {
	.probe          = secure_etr_probe,
	.remove         = secure_etr_remove,
	.driver         = {
		.name   = "coresight-secure-etr",
		.of_match_table = secure_etr_match,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(secure_etr_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Secure ETR driver");
