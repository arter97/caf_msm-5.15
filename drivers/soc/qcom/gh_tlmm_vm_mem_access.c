// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/types.h>
#include "linux/gunyah/gh_mem_notifier.h"
#include "linux/gunyah/gh_rm_drv.h"
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/qcom-pinctrl.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/sort.h>

#define GH_TLMM_MEM_LABEL 0x8
#define SHARED_GPIO 0
#define LEND_GPIO 1

struct gh_tlmm_mem_info {
	gh_memparcel_handle_t vm_mem_handle;
	u32 *iomem_bases;
	u32 *iomem_sizes;
	u32 iomem_list_size;
	int num_regs[2];
};

struct gh_tlmm_vm_info {
	struct notifier_block guest_memshare_nb;
	enum gh_vm_names vm_name;
	gh_memparcel_handle_t vm_mem_handle;
	struct gh_tlmm_mem_info mem_info[2];
	void *mem_cookie;
};

static struct gh_tlmm_vm_info gh_tlmm_vm_info_data;
static struct device *gh_tlmm_dev;
static struct pinctrl *qcom_vm_gpio_access_pinctrl;
static struct pinctrl_state *qcom_vm_gpio_access_sleep_state;
static struct pinctrl_state *qcom_vm_gpio_access_active_state;

static struct gh_acl_desc *gh_tlmm_vm_get_acl(enum gh_vm_names vm_name,
						bool lend_gpio)
{
	struct gh_acl_desc *acl_desc;
	gh_vmid_t vmid;
	gh_vmid_t primary_vmid;

	gh_rm_get_vmid(vm_name, &vmid);
	gh_rm_get_vmid(GH_PRIMARY_VM, &primary_vmid);

	if (lend_gpio) {
		acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[1]),
			GFP_KERNEL);
		if (!acl_desc)
			return ERR_PTR(ENOMEM);

		acl_desc->n_acl_entries = 1;
		acl_desc->acl_entries[0].vmid = vmid;
		acl_desc->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;
	} else {
		acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[2]),
			GFP_KERNEL);
		if (!acl_desc)
			return ERR_PTR(ENOMEM);

		acl_desc->n_acl_entries = 2;
		acl_desc->acl_entries[0].vmid = vmid;
		acl_desc->acl_entries[0].perms = GH_RM_ACL_R;
		acl_desc->acl_entries[1].vmid = primary_vmid;
		acl_desc->acl_entries[1].perms = GH_RM_ACL_R | GH_RM_ACL_W;
	}

	return acl_desc;
}

static struct gh_sgl_desc *gh_tlmm_vm_get_sgl(struct gh_tlmm_mem_info
						shared_mem_info)
{
	struct gh_sgl_desc *sgl_desc;
	int i;

	sgl_desc = kzalloc(offsetof(struct gh_sgl_desc,
			sgl_entries[shared_mem_info.iomem_list_size]), GFP_KERNEL);
	if (!sgl_desc)
		return ERR_PTR(ENOMEM);

	sgl_desc->n_sgl_entries = shared_mem_info.iomem_list_size;

	for (i = 0; i < shared_mem_info.iomem_list_size; i++) {
		sgl_desc->sgl_entries[i].ipa_base = shared_mem_info.iomem_bases[i];
		sgl_desc->sgl_entries[i].size = shared_mem_info.iomem_sizes[i];
	}

	return sgl_desc;
}

/*This API is used both for sharing and lending GPIO's*/
static int gh_tlmm_vm_mem_share(struct gh_tlmm_vm_info *gh_tlmm_vm_info_data)
{
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	struct gh_acl_desc *lend_acl_desc;
	struct gh_sgl_desc *lend_sgl_desc;
	gh_memparcel_handle_t mem_handle;
	struct gh_tlmm_mem_info mem_info;
	int num_regs = 0;
	int rc = 0;

	num_regs = gh_tlmm_vm_info_data->mem_info[SHARED_GPIO].num_regs[SHARED_GPIO];
	if (num_regs > 0) {
		acl_desc = gh_tlmm_vm_get_acl(gh_tlmm_vm_info_data->vm_name, false);
		if (IS_ERR(acl_desc)) {
			dev_err(gh_tlmm_dev, "Failed to get acl of IO memories for TLMM\n");
			return PTR_ERR(acl_desc);
		}

		mem_info = gh_tlmm_vm_info_data->mem_info[SHARED_GPIO];
		sgl_desc = gh_tlmm_vm_get_sgl(mem_info);
		if (IS_ERR(sgl_desc)) {
			dev_err(gh_tlmm_dev, "Failed to get sgl of IO memories for TLMM\n");
			rc = PTR_ERR(sgl_desc);
			goto free_acl_desc;
		}

		rc = gh_rm_mem_share(GH_RM_MEM_TYPE_IO, 0, GH_TLMM_MEM_LABEL,
				acl_desc, sgl_desc, NULL, &mem_handle);
		if (rc) {
			dev_err(gh_tlmm_dev, "Failed to share IO memories for TLMM rc:%d\n", rc);
			goto free_sgl_desc;
		}

		gh_tlmm_vm_info_data->mem_info[SHARED_GPIO].vm_mem_handle = mem_handle;
	}

	num_regs = gh_tlmm_vm_info_data->mem_info[LEND_GPIO].num_regs[LEND_GPIO];
	if (num_regs > 0) {
		lend_acl_desc = gh_tlmm_vm_get_acl(gh_tlmm_vm_info_data->vm_name, true);
		if (IS_ERR(lend_acl_desc)) {
			dev_err(gh_tlmm_dev, "Failed to get acl of IO memories for TLMM\n");
			rc = PTR_ERR(lend_acl_desc);
			goto free_sgl_desc;
		}

		mem_info = gh_tlmm_vm_info_data->mem_info[LEND_GPIO];
		lend_sgl_desc = gh_tlmm_vm_get_sgl(mem_info);
		if (IS_ERR(lend_sgl_desc)) {
			dev_err(gh_tlmm_dev, "Failed to get sgl of IO memories for lend TLMM\n");
			rc = PTR_ERR(lend_sgl_desc);
			goto free_lend_acl_desc;
		}

		memset((gh_memparcel_handle_t *)&mem_handle, 0, sizeof(gh_memparcel_handle_t));

		rc = gh_rm_mem_lend(GH_RM_MEM_TYPE_IO, 0, GH_TLMM_MEM_LABEL,
			lend_acl_desc, lend_sgl_desc, NULL, &mem_handle);
		if (rc) {
			dev_err(gh_tlmm_dev, "Failed to lend IO memories for TLMM rc:%d\n", rc);
			goto free_lend_sgl_desc;
		}

		gh_tlmm_vm_info_data->mem_info[LEND_GPIO].vm_mem_handle = mem_handle;
	}

free_lend_sgl_desc:
	kfree(lend_sgl_desc);
free_lend_acl_desc:
	kfree(lend_acl_desc);
free_sgl_desc:
	kfree(sgl_desc);
free_acl_desc:
	kfree(acl_desc);

	return rc;
}

static int __maybe_unused gh_guest_memshare_nb_handler(struct notifier_block *this,
					unsigned long cmd, void *data)
{
	struct gh_tlmm_vm_info *vm_info;
	struct gh_rm_notif_vm_status_payload *vm_status_payload = data;
	u8 vm_status = vm_status_payload->vm_status;
	gh_vmid_t peer_vmid;

	vm_info = container_of(this, struct gh_tlmm_vm_info, guest_memshare_nb);

	if (cmd != GH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	gh_rm_get_vmid(vm_info->vm_name, &peer_vmid);

	if (peer_vmid != vm_status_payload->vmid)
		return NOTIFY_DONE;

	/*
	 * Listen to STATUS_READY notification from RM.
	 * These notifications come from RM after PIL loading the VM images.
	 */
	if (vm_status == GH_RM_VM_STATUS_READY)
		gh_tlmm_vm_mem_share(&gh_tlmm_vm_info_data);

	return NOTIFY_DONE;
}

static int gh_tlmm_vm_mem_release(struct gh_tlmm_vm_info *gh_tlmm_vm_info_data, int gpio_type)
{
	int rc = 0;
	gh_memparcel_handle_t vm_mem_handle;

	vm_mem_handle = gh_tlmm_vm_info_data->mem_info[gpio_type].vm_mem_handle;
	if (!vm_mem_handle) {
		dev_err(gh_tlmm_dev, "Invalid memory handle\n");
		return -EINVAL;
	}

	rc = gh_rm_mem_release(vm_mem_handle, 0);
	if (rc)
		dev_err(gh_tlmm_dev, "VM mem release failed rc:%d\n", rc);

	rc = gh_rm_mem_notify(vm_mem_handle,
		GH_RM_MEM_NOTIFY_OWNER_RELEASED,
		GH_MEM_NOTIFIER_TAG_TLMM, 0);
	if (rc)
		dev_err(gh_tlmm_dev, "Failed to notify mem release to PVM rc:%d\n",
							rc);

	gh_tlmm_vm_info_data->mem_info[gpio_type].vm_mem_handle = 0;
	return rc;
}

static int gh_tlmm_vm_mem_reclaim(struct gh_tlmm_vm_info *gh_tlmm_vm_info_data, int gpio_type)
{
	int rc = 0;
	gh_memparcel_handle_t vm_mem_handle;

	vm_mem_handle = gh_tlmm_vm_info_data->mem_info[gpio_type].vm_mem_handle;
	if (!vm_mem_handle) {
		dev_err(gh_tlmm_dev, "Invalid memory handle\n");
		return -EINVAL;
	}

	rc = gh_rm_mem_reclaim(vm_mem_handle, 0);
	if (rc)
		dev_err(gh_tlmm_dev, "VM mem reclaim failed rc:%d\n", rc);

	gh_tlmm_vm_info_data->mem_info[gpio_type].vm_mem_handle = 0;

	return rc;
}

static int gh_tlmm_prepare_iomem(struct device_node *np, struct gh_tlmm_mem_info
					*mem_info, int num_regs, bool lend)
{
	int rc = 0, i, gpio, ret;
	u32 *gpios;
	struct resource *res;

	gpios = kmalloc_array(num_regs, sizeof(*gpios), GFP_KERNEL);
	if (!gpios)
		return -ENOMEM;

	for (i = 0; i < num_regs; i++) {
		if (lend)
			/*GPIO's to lend*/
			gpio = of_get_named_gpio(np, "tlmm-vm-gpio-lend-list", i);
		else
			/*GPIO's to be shared*/
			gpio = of_get_named_gpio(np, "tlmm-vm-gpio-list", i);

		if (gpio < 0) {
			rc = gpio;
			dev_err(gh_tlmm_dev, "Failed to read gpio list %d\n", rc);
			goto gpios_error;
		}
		gpios[i] = gpio;
	}

	mem_info->iomem_list_size = num_regs;
	mem_info->iomem_bases = kcalloc(num_regs, sizeof(*mem_info->iomem_bases),
							GFP_KERNEL);
	if (!mem_info->iomem_bases) {
		rc = -ENOMEM;
		goto gpios_error;
	}

	mem_info->iomem_sizes = kzalloc(sizeof(*mem_info->iomem_sizes) * num_regs,
					GFP_KERNEL);
	if (!mem_info->iomem_sizes) {
		rc = -ENOMEM;
		goto io_bases_error;
	}

	res = kzalloc(sizeof(*res), GFP_KERNEL);
	for (i = 0; i < num_regs; i++)  {
		ret = msm_gpio_get_pin_address(gpios[i], res);
		if (!ret) {
			dev_err(gh_tlmm_dev, "Invalid gpio = %d\n", gpios[i]);
			rc = -EINVAL;
			goto io_sizes_error;
		}

		mem_info->iomem_bases[i] = res->start;
		mem_info->iomem_sizes[i] = resource_size(res);
	}

	kfree(gpios);
	kfree(res);
	return rc;
io_sizes_error:
	kfree(res);
	kfree(mem_info->iomem_sizes);
io_bases_error:
	kfree(mem_info->iomem_bases);
gpios_error:
	kfree(gpios);
	return rc;
}

static int gh_tlmm_vm_populate_vm_info(struct platform_device *dev, struct gh_tlmm_vm_info *vm_info)
{
	int rc = 0;
	struct device_node *np = dev->dev.of_node;
	gh_memparcel_handle_t __maybe_unused vm_mem_handle;
	bool master;
	u32 peer_vmid;
	int num_regs = 0;

	master = of_property_read_bool(np, "qcom,master");
	if (!master) {
		rc = of_property_read_u32_index(np, "qcom,rm-mem-handle",
				1, &vm_mem_handle);
		if (rc) {
			dev_err(gh_tlmm_dev, "Failed to receive mem handle rc:%d\n", rc);
			goto vm_error;
		}

		vm_info->mem_info[SHARED_GPIO].vm_mem_handle = vm_mem_handle;
		vm_info->mem_info[LEND_GPIO].vm_mem_handle = vm_mem_handle;
	}

	rc = of_property_read_u32(np, "peer-name", &peer_vmid);
	if (rc) {
		dev_err(gh_tlmm_dev, "peer-name not found rc=%x using default\n", rc);
		peer_vmid = GH_TRUSTED_VM;
	}

	vm_info->vm_name = peer_vmid;

	vm_info->mem_info[SHARED_GPIO].num_regs[SHARED_GPIO] = of_gpio_named_count(np,
			"tlmm-vm-gpio-list");
	vm_info->mem_info[LEND_GPIO].num_regs[LEND_GPIO] = of_gpio_named_count(np,
			"tlmm-vm-gpio-lend-list");
	if (vm_info->mem_info[SHARED_GPIO].num_regs[SHARED_GPIO] < 0 &&
		vm_info->mem_info[LEND_GPIO].num_regs[LEND_GPIO] < 0) {
		dev_err(gh_tlmm_dev, "Invalid number of gpios specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	num_regs = vm_info->mem_info[SHARED_GPIO].num_regs[SHARED_GPIO];
	if (num_regs > 0)
		rc = gh_tlmm_prepare_iomem(np, &vm_info->mem_info[SHARED_GPIO],
					num_regs, false);

	num_regs = vm_info->mem_info[LEND_GPIO].num_regs[LEND_GPIO];
	if (num_regs > 0)
		rc = gh_tlmm_prepare_iomem(np, &vm_info->mem_info[LEND_GPIO],
					num_regs, true);

	if (rc < 0)
		dev_err(gh_tlmm_dev, "Failed to prepare iomem %d\n", rc);

	return rc;

vm_error:
	return rc;
}

static void __maybe_unused gh_tlmm_vm_mem_on_release_handler(enum gh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_released_payload *release_payload;
	struct gh_tlmm_vm_info *vm_info;

	if (notif_type != GH_RM_NOTIF_MEM_RELEASED) {
		dev_err(gh_tlmm_dev, "Invalid notification type\n");
		return;
	}

	if (tag != GH_MEM_NOTIFIER_TAG_TLMM) {
		dev_err(gh_tlmm_dev, "Invalid tag\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		dev_err(gh_tlmm_dev, "Invalid data or notification message\n");
		return;
	}

	vm_info = (struct gh_tlmm_vm_info *)entry_data;
	if (!vm_info) {
		dev_err(gh_tlmm_dev, "Invalid vm_info\n");
		return;
	}

	release_payload = (struct gh_rm_notif_mem_released_payload  *)notif_msg;
	if (release_payload->mem_handle != vm_info->mem_info[SHARED_GPIO].vm_mem_handle &&
			release_payload->mem_handle != vm_info->mem_info[LEND_GPIO].vm_mem_handle) {
		dev_err(gh_tlmm_dev, "Invalid mem handle detected mem_handle 0x%x, vm_info->vm_mem_handle 0x%x\n",
				release_payload->mem_handle, vm_info->vm_mem_handle);
		return;
	}

	if (release_payload->mem_handle == vm_info->mem_info[SHARED_GPIO].vm_mem_handle)
		gh_tlmm_vm_mem_reclaim(vm_info, SHARED_GPIO);

	if (release_payload->mem_handle == vm_info->mem_info[LEND_GPIO].vm_mem_handle)
		gh_tlmm_vm_mem_reclaim(vm_info, LEND_GPIO);
}

static int gh_tlmm_vm_mem_access_probe(struct platform_device *pdev)
{
	void __maybe_unused *mem_cookie;
	int owner_vmid, ret;
	struct device_node *node;
	gh_vmid_t vmid;

	gh_tlmm_dev = &pdev->dev;

	if (gh_tlmm_vm_populate_vm_info(pdev, &gh_tlmm_vm_info_data)) {
		dev_err(gh_tlmm_dev, "Failed to populate TLMM VM info\n");
		return -EINVAL;
	}

	node = of_find_compatible_node(NULL, NULL, "qcom,gunyah-vm-id-1.0");
	if (IS_ERR_OR_NULL(node)) {
		node = of_find_compatible_node(NULL, NULL, "qcom,haven-vm-id-1.0");
		if (IS_ERR_OR_NULL(node)) {
			dev_err(gh_tlmm_dev, "Could not find vm-id node\n");
			return -ENODEV;
		}
	}

	ret = of_property_read_u32(node, "qcom,owner-vmid", &owner_vmid);
	if (ret) {
		/* GH_PRIMARY_VM */
		mem_cookie = gh_mem_notifier_register(GH_MEM_NOTIFIER_TAG_TLMM,
					gh_tlmm_vm_mem_on_release_handler, &gh_tlmm_vm_info_data);
		if (IS_ERR(mem_cookie)) {
			dev_err(gh_tlmm_dev, "Failed to register on release notifier%d\n",
						PTR_ERR(mem_cookie));
			return -EINVAL;
		}

		gh_tlmm_vm_info_data.mem_cookie = mem_cookie;
		gh_tlmm_vm_info_data.guest_memshare_nb.notifier_call = gh_guest_memshare_nb_handler;

		gh_tlmm_vm_info_data.guest_memshare_nb.priority = INT_MAX;
		ret = gh_rm_register_notifier(&gh_tlmm_vm_info_data.guest_memshare_nb);
		if (ret)
			return ret;
	} else {
		ret = gh_rm_get_vmid(gh_tlmm_vm_info_data.vm_name, &vmid);
		if (ret)
			return ret;

		if (gh_tlmm_vm_info_data.mem_info[SHARED_GPIO].num_regs[SHARED_GPIO] > 0)
			gh_tlmm_vm_mem_release(&gh_tlmm_vm_info_data, SHARED_GPIO);

		qcom_vm_gpio_access_pinctrl = devm_pinctrl_get(gh_tlmm_dev);
		if (IS_ERR_OR_NULL(qcom_vm_gpio_access_pinctrl)) {
			dev_err(gh_tlmm_dev, "Failed to get PINCTRL handle for TLMM VM test\n");
			qcom_vm_gpio_access_pinctrl = NULL;
		} else {
			qcom_vm_gpio_access_sleep_state =
					pinctrl_lookup_state(qcom_vm_gpio_access_pinctrl, "sleep");
			qcom_vm_gpio_access_active_state =
					pinctrl_lookup_state(qcom_vm_gpio_access_pinctrl, "active");

			ret = pinctrl_select_state(qcom_vm_gpio_access_pinctrl,
								qcom_vm_gpio_access_active_state);
			if (ret) {
				dev_err(gh_tlmm_dev, "Failed to set PINCTRL state\n");
				return ret;
			}
		}
	}

	return 0;

}

static int gh_tlmm_vm_mem_access_remove(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	bool master;

	if (gh_tlmm_vm_info_data.mem_info[LEND_GPIO].num_regs[LEND_GPIO] > 0)
		gh_tlmm_vm_mem_release(&gh_tlmm_vm_info_data, LEND_GPIO);

	master = of_property_read_bool(np, "qcom,master");
	if (master)
		gh_mem_notifier_unregister(gh_tlmm_vm_info_data.mem_cookie);
	gh_rm_unregister_notifier(&gh_tlmm_vm_info_data.guest_memshare_nb);

	return 0;
}

static const struct of_device_id gh_tlmm_vm_mem_access_of_match[] = {
	{ .compatible = "qcom,tlmm-vm-mem-access"},
	{}
};
MODULE_DEVICE_TABLE(of, gh_tlmm_vm_mem_access_of_match);

static struct platform_driver gh_tlmm_vm_mem_access_driver = {
	.probe = gh_tlmm_vm_mem_access_probe,
	.remove = gh_tlmm_vm_mem_access_remove,
	.driver = {
		.name = "gh_tlmm_vm_mem_access",
		.of_match_table = gh_tlmm_vm_mem_access_of_match,
	},
};

static int __init gh_tlmm_vm_mem_access_init(void)
{
	return platform_driver_register(&gh_tlmm_vm_mem_access_driver);
}
late_initcall_sync(gh_tlmm_vm_mem_access_init);

static __exit void gh_tlmm_vm_mem_access_exit(void)
{
	platform_driver_unregister(&gh_tlmm_vm_mem_access_driver);
}
module_exit(gh_tlmm_vm_mem_access_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. TLMM VM Memory Access Driver");
MODULE_LICENSE("GPL v2");
