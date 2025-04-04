// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/anon_inodes.h>
#include <linux/miscdevice.h>
#include <linux/pagemap.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/qcom_scm.h>
#include <linux/gunyah.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/limits.h>

#include "gh_secure_vm_virtio_backend.h"
#include "gh_secure_vm_loader.h"
#include "gh_proxy_sched.h"
#include "gh_private.h"
#include "gvm_dump_debugfs.h"

#define MAX_VCPU_NAME		20 /* gh-vcpu:u32_max +1 */
#define MAX_VM_SUSP_LABEL	18 /* vm_u16_max_susp_irq + 1 */

SRCU_NOTIFIER_HEAD_STATIC(gh_vm_notifier);
static DEFINE_SPINLOCK(vm_list_lock);
static LIST_HEAD(vm_list);

/*
 * Support for RM calls and the wait for change of status
 */
#define gh_rm_call_and_set_status(name) \
static int gh_##name(struct gh_vm *vm, int vm_status)			 \
{									 \
	int ret = 0;							 \
	ret = gh_rm_##name(vm->vmid);					 \
	if (!ret)							 \
		vm->status.vm_status = vm_status;			 \
	return ret;							 \
}

gh_rm_call_and_set_status(vm_start);

#define gh_wait_for_vm_status(vm, wait_status) 				\
	wait_event(vm->vm_status_wait, (vm->status.vm_status == wait_status))

/*
static struct gh_vm* find_vm_by_id(gh_vmid_t vmid)
{
	struct gh_vm *vm = NULL, *tmp;

	spin_lock(&vm_list_lock);
	list_for_each_entry(tmp, &vm_list, list) {
		if (tmp->vmid == vmid) {
			vm = tmp;
			break;
		}
	}
	spin_unlock(&vm_list_lock);

	return vm;
}

static struct gh_vm* find_vm_by_susp_irq(int irq)
{
	struct gh_vm *vm = NULL, *tmp;

	spin_lock(&vm_list_lock);
	list_for_each_entry(tmp, &vm_list, list) {
		if (tmp->susp_irq == irq) {
			vm = tmp;
			break;
		}
	}
	spin_unlock(&vm_list_lock);

	return vm;
}
*/

int gh_register_vm_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&gh_vm_notifier, nb);
}
EXPORT_SYMBOL(gh_register_vm_notifier);

int gh_unregister_vm_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&gh_vm_notifier, nb);
}
EXPORT_SYMBOL(gh_unregister_vm_notifier);

static void gh_notify_clients(struct gh_vm *vm, unsigned long val)
{
	srcu_notifier_call_chain(&gh_vm_notifier, val, &vm->vmid);
}

static void gh_notif_vm_status(struct gh_vm *vm,
			struct gh_rm_notif_vm_status_payload *status)
{
	if (vm->vmid != status->vmid)
		return;

	/* Wake up the waiters only if there's a change in any of the states */
	if (status->vm_status != vm->status.vm_status &&
	   (status->vm_status == GH_RM_VM_STATUS_RESET ||
	   status->vm_status == GH_RM_VM_STATUS_READY)) {
		pr_info("VM: %d status %d complete\n", vm->vmid,
							status->vm_status);
		vm->status.vm_status = status->vm_status;
		wake_up(&vm->vm_status_wait);
	}
}

static void gh_notif_vm_exited(struct gh_vm *vm,
			struct gh_rm_notif_vm_exited_payload *vm_exited)
{
	if (vm->vmid != vm_exited->vmid)
		return;

	mutex_lock(&vm->vm_lock);
	vm->exit_type = vm_exited->exit_type;
	vm->status.vm_status = GH_RM_VM_STATUS_EXITED;
	gh_wakeup_all_vcpus(vm->vmid);
	wake_up(&vm->vm_status_wait);
	wake_up_interruptible(&vm->vm_status_wait);
	wake_up_interruptible(&vm->vm_exit_ioc_wait);
	mutex_unlock(&vm->vm_lock);
}

static int gh_wait_for_vm_status_intr(struct gh_vm *vm, int wait_status)
{
	int ret = 0;

	ret = wait_event_interruptible(vm->vm_status_wait,
			vm->status.vm_status == wait_status);
	if (ret < 0)
		pr_err("Wait for VM_STATUS %d interrupted\n", wait_status);

	return ret;
}

static int gh_ioc_wait_for_vm_status(struct gh_vm *vm, int wait_status)
{
	int ret = 0;

	ret = wait_event_interruptible(vm->vm_exit_ioc_wait,
			vm->status.vm_status == wait_status);
	if (ret < 0)
		pr_err("Wait for VM_STATUS %d interrupted\n", wait_status);

	return ret;
}

static int gh_vm_rm_notifier_fn(struct notifier_block *nb,
					unsigned long cmd, void *data)
{
	struct gh_vm *vm;

	vm = container_of(nb, struct gh_vm, rm_nb);

	switch (cmd) {
	case GH_RM_NOTIF_VM_STATUS:
		gh_notif_vm_status(vm, data);
		break;
	case GH_RM_NOTIF_VM_EXITED:
		gh_notif_vm_exited(vm, data);
		break;
	}

	return NOTIFY_DONE;
}

static void gh_vm_cleanup(struct gh_vm *vm)
{
	gh_vmid_t vmid = vm->vmid;
	int vm_status = vm->status.vm_status;
	int ret;

	switch (vm_status) {
	case GH_RM_VM_STATUS_EXITED:
	case GH_RM_VM_STATUS_RUNNING:
	case GH_RM_VM_STATUS_READY:
		ret = gh_rm_unpopulate_hyp_res(vmid, vm->fw_name);
		if (ret)
			pr_warn("Failed to unpopulate hyp resources: %d\n", ret);
	case GH_RM_VM_STATUS_INIT:
	case GH_RM_VM_STATUS_AUTH:
		ret = gh_rm_vm_reset(vmid);
		if (!ret) {
			gh_wait_for_vm_status(vm, GH_RM_VM_STATUS_RESET);
		} else
			pr_warn("Reset is unsuccessful for VM:%d\n", vmid);

		ret = gh_virtio_mmio_exit(vmid, vm->fw_name);
		if (ret)
			pr_warn("Failed to free virtio resources : %d\n", ret);

		if (vm->is_secure_vm) {
			ret = gh_secure_vm_loader_reclaim_fw(vm);
			if (ret)
				pr_warn("Failed to reclaim mem VMID: %d: %d\n", vmid, ret);
		}
	case GH_RM_VM_STATUS_LOAD:
		ret = gh_rm_vm_dealloc_vmid(vmid);
		if (ret)
			pr_warn("Failed to dealloc VMID: %d: %d\n", vmid, ret);
	}

	vm->status.vm_status = GH_RM_VM_STATUS_NO_STATE;
}

static int gh_exit_vm(struct gh_vm *vm, u32 stop_reason, u8 stop_flags)
{
	gh_vmid_t vmid = vm->vmid;
	int ret = -EINVAL;

	if (!vmid)
		return -ENODEV;

	mutex_lock(&vm->vm_lock);
	if (vm->status.vm_status != GH_RM_VM_STATUS_RUNNING) {
		pr_err("VM:%d is not running\n", vmid);
		mutex_unlock(&vm->vm_lock);
		return -ENODEV;
	}

	ret = gh_rm_vm_stop(vmid, stop_reason, stop_flags);
	if (ret) {
		pr_err("Failed to stop the VM:%d ret %d\n", vmid, ret);
		mutex_unlock(&vm->vm_lock);
		return ret;
	}
	mutex_unlock(&vm->vm_lock);

	gh_wait_for_vm_status(vm, GH_RM_VM_STATUS_EXITED);

	return ret;
}

static int gh_stop_vm(struct gh_vm *vm)
{
	gh_vmid_t vmid = vm->vmid;
	int ret = -EINVAL;

	ret = gh_exit_vm(vm, GH_VM_STOP_RESTART,
					GH_RM_VM_STOP_FLAG_FORCE_STOP);
	if (ret && ret != -ENODEV)
		goto err_vm_force_stop;

	return ret;

err_vm_force_stop:
	ret = gh_exit_vm(vm, GH_VM_STOP_FORCE_STOP,
					GH_RM_VM_STOP_FLAG_FORCE_STOP);
	if (ret)
		pr_err("VM:%d force stop has failed\n", vmid);
	return ret;
}

void gh_destroy_vcpu(struct gh_vcpu *vcpu)
{
	struct gh_vm *vm = vcpu->vm;
	u32 id = vcpu->vcpu_id;

	kfree(vcpu);
	vm->vcpus[id] = NULL;
	vm->created_vcpus--;
}

static void gh_destroy_vm(struct kref *kref)
{
	int vcpu_id = 0;
	struct gh_vm *vm = container_of(kref, struct gh_vm, kref);

	if (vm->status.vm_status == GH_RM_VM_STATUS_NO_STATE)
		goto clean_vm;

	gh_stop_vm(vm);

	while (vm->created_vcpus && vcpu_id < GH_MAX_VCPUS) {
		if (!vm->vcpus[vcpu_id])
			continue;
		gh_destroy_vcpu(vm->vcpus[vcpu_id]);
		vcpu_id++;
	}

	gh_notify_clients(vm, GH_VM_EARLY_POWEROFF);
	gh_vm_cleanup(vm);

	gh_uevent_notify_change(GH_EVENT_DESTROY_VM, vm);
	gh_notify_clients(vm, GH_VM_POWEROFF);
	memset(vm->fw_name, 0, GH_VM_FW_NAME_MAX);

clean_vm:
	spin_lock(&vm_list_lock);
	list_del(&vm->list);
	spin_unlock(&vm_list_lock);
	gh_rm_unregister_notifier(&vm->rm_nb);
	mutex_destroy(&vm->vm_lock);
	kfree(vm);
}

static int __must_check gh_get_vm(struct gh_vm *vm)
{
	return kref_get_unless_zero(&vm->kref);
}

static void gh_put_vm(struct gh_vm *vm)
{
	kref_put(&vm->kref, gh_destroy_vm);
}

static struct gh_vm *find_and_get_vm_by_name(const char *vm_name)
{
	struct gh_vm *vm = NULL, *tmp;

	spin_lock(&vm_list_lock);
	list_for_each_entry(tmp, &vm_list, list) {
		if (!strcmp(tmp->fw_name, vm_name) && gh_get_vm(tmp)) {
			vm = tmp;
			break;
		}
	}
	spin_unlock(&vm_list_lock);

	return vm;
}

static int gh_vcpu_release(struct inode *inode, struct file *filp)
{
	struct gh_vcpu *vcpu = filp->private_data;

	gh_put_vm(vcpu->vm);
	return 0;
}

static int gh_vcpu_ioctl_run(struct gh_vcpu *vcpu)
{
	struct gh_hcall_vcpu_run_resp vcpu_run;
	struct gh_vm *vm = vcpu->vm;
	int ret = 0;

	mutex_lock(&vm->vm_lock);

	if (vm->status.vm_status == GH_RM_VM_STATUS_RUNNING) {
		mutex_unlock(&vm->vm_lock);
		goto start_vcpu_run;
	}

	if (vm->vm_run_once &&
		vm->status.vm_status != GH_RM_VM_STATUS_RUNNING) {
		pr_err("VM:%d has failed to run before\n", vm->vmid);
		mutex_unlock(&vm->vm_lock);
		return -EINVAL;
	}

	vm->vm_run_once = true;

	if (vm->is_secure_vm &&
		vm->created_vcpus != vm->allowed_vcpus) {
		pr_err("VCPUs created %d doesn't match with allowed %d for VM %d\n",
			vm->created_vcpus, vm->allowed_vcpus,
							vm->vmid);
		ret = -EINVAL;
		mutex_unlock(&vm->vm_lock);
		return ret;
	}

	if (vm->status.vm_status != GH_RM_VM_STATUS_READY) {
		pr_err("VM:%d not ready to start\n", vm->vmid);
		ret = -EINVAL;
		mutex_unlock(&vm->vm_lock);
		return ret;
	}

	gh_notify_clients(vm, GH_VM_BEFORE_POWERUP);

	ret = gh_vm_start(vm, GH_RM_VM_STATUS_RUNNING);
	if (ret) {
		pr_err("Failed to start VM:%d %d\n", vm->vmid, ret);
		mutex_unlock(&vm->vm_lock);
		goto err_powerup;
	}
	pr_info("VM:%d started running\n", vm->vmid);

	mutex_unlock(&vm->vm_lock);

start_vcpu_run:
	/*
	 * proxy scheduling APIs
	 */
	if (gh_vm_supports_proxy_sched(vm->vmid)) {
		ret = gh_vcpu_run(vm->vmid, vcpu->vcpu_id,
						0, 0, 0, &vcpu_run);
		if (ret < 0) {
			pr_err("Failed vcpu_run %d\n", ret);
			return ret;
		}
	}

	else {
		ret = gh_wait_for_vm_status_intr(vm, GH_RM_VM_STATUS_EXITED);
		if (ret)
			return ret;

		ret = vm->exit_type;

		gh_virtio_mmio_app_exit(vm->vmid, vm->fw_name);
	}

	return ret;

err_powerup:
	gh_notify_clients(vm, GH_VM_POWERUP_FAIL);
	return ret;
}

static long gh_vcpu_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct gh_vcpu *vcpu = filp->private_data;
	int ret = -EINVAL;

	switch (cmd) {
	case GH_VCPU_RUN:
		ret = gh_vcpu_ioctl_run(vcpu);
		break;
	default:
		pr_err("Invalid gunyah VCPU ioctl 0x%lx\n", cmd);
		break;
	}
	return ret;
}

static const struct file_operations gh_vcpu_fops = {
	.unlocked_ioctl = gh_vcpu_ioctl,
	.release = gh_vcpu_release,
	.llseek = noop_llseek,
};

static int gh_vm_ioctl_get_vcpu_count(struct gh_vm *vm)
{
	if (!vm->is_secure_vm)
		return -EINVAL;

	if (vm->status.vm_status != GH_RM_VM_STATUS_READY)
		return -EAGAIN;

	return vm->allowed_vcpus;
}

static long gh_vm_ioctl_create_vcpu(struct gh_vm *vm, u32 id)
{
	struct gh_vcpu *vcpu;
	struct file *file;
	char name[MAX_VCPU_NAME];
	int fd, err = 0;

	if (id >= GH_MAX_VCPUS)
		return -EINVAL;

	mutex_lock(&vm->vm_lock);
	if (vm->vcpus[id]) {
		err = -EEXIST;
		mutex_unlock(&vm->vm_lock);
		return err;
	}

	vcpu = kzalloc(sizeof(*vcpu), GFP_KERNEL);
	if (!vcpu) {
		err = -ENOMEM;
		mutex_unlock(&vm->vm_lock);
		return err;
	}

	vcpu->vcpu_id = id;
	vcpu->vm = vm;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		err = fd;
		goto err_destroy_vcpu;
	}

	snprintf(name, sizeof(name), "gh-vcpu:%d", id);
	file = anon_inode_getfile(name, &gh_vcpu_fops, vcpu, O_RDWR);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto err_put_fd;
	}

	if (unlikely(!gh_get_vm(vm)))
		goto err_put_fd;

	fd_install(fd, file);

	vm->vcpus[id] = vcpu;
	vm->created_vcpus++;
	mutex_unlock(&vm->vm_lock);
	return fd;

err_put_fd:
	put_unused_fd(fd);
err_destroy_vcpu:
	kfree(vcpu);
	mutex_unlock(&vm->vm_lock);
	return err;
}

int gh_reclaim_mem(struct gh_vm *vm, struct gh_mem_parcel *mem_parcels,
					u32 mem_parcel_count, bool is_system_vm)
{
	int vmid = vm->vmid;
	struct qcom_scm_vmperm destVM[1] = {{QCOM_SCM_VMID_HLOS, QCOM_SCM_PERM_RWX}};
	u64 srcvmid = BIT(QCOM_SCM_VMID_HLOS) | BIT(vmid);
	phys_addr_t phys;
	ssize_t size;
	int i;
	int ret = 0;

	if (!is_system_vm) {
		ret = gh_rm_mem_reclaim(vm->mem_handle, 0);

		if (ret)
			pr_err("Failed to reclaim memory for %d, %d\n",
						vm->vmid, ret);
	}

	for (i = 0; i < mem_parcel_count; i++) {
		phys = mem_parcels[i].mem_phys;
		size = mem_parcels[i].mem_size;
		ret = qcom_scm_assign_mem(phys, size, &srcvmid, destVM, ARRAY_SIZE(destVM));
		if (ret) {
			pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
				&phys, size, vmid, ret);
			return ret;
		}

		srcvmid = BIT(QCOM_SCM_VMID_HLOS) | BIT(vmid);
	}

	return ret;
}

static void set_vmperm_bit(struct qcom_scm_vmperm *vmperm, u64 *bit_vmid,
									 gh_vmid_t vmid, gh_vm_perm_t perm)
{
	vmperm->vmid = vmid;
	vmperm->perm = perm;
	*bit_vmid |= BIT(vmid);
}

int gh_reclaim_shmem(struct gh_vm *vm, struct gh_shmem *shmems)
{
	gh_vmid_t vmid = shmems->is_phantom ? (shmems->gunyah_label & 0xff) : vm->vmid;
	int i;
	int ret = 0;
	int dst_vmids_count = 1;
	struct qcom_scm_vmperm *dstVM;
	phys_addr_t phys = shmems->base;
	ssize_t size = shmems->size;
	u64 srcvmid = 0;
	u64 dstvmid = 0;

	dst_vmids_count += shmems->src_vmids_count;

	dstVM = kzalloc(dst_vmids_count * sizeof(struct qcom_scm_vmperm),
						GFP_KERNEL);
	if (!dstVM)
		return -ENOMEM;

	for (i = 0; i < shmems->src_vmids_count; i++)
		set_vmperm_bit(&dstVM[i], &dstvmid, shmems->src_vmids[i], shmems->src_perms[i]);

	set_vmperm_bit(&dstVM[i], &dstvmid, QCOM_SCM_VMID_HLOS, QCOM_SCM_PERM_RWX);

	for (i = 0; i < shmems->dst_vmids_count; i++)
		srcvmid |= BIT(shmems->dst_vmids[i]);

	srcvmid |= BIT(vmid);

	if (shmems->is_shared)
		srcvmid |= BIT(QCOM_SCM_VMID_HLOS);

	ret = gh_rm_mem_reclaim(shmems->shmem_handle, 0);
	if (ret)
		pr_err("Failed to reclaim memory for %d, %d\n",
					vm->vmid, ret);

	ret = qcom_scm_assign_mem(phys, size, &srcvmid, dstVM, dst_vmids_count);
	if (ret)
		pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
						 &phys, size, dstVM[0].vmid, ret);

	return ret;
}

int gh_provide_mem(struct gh_vm *vm, struct gh_mem_parcel *mem_parcels,
					u32 mem_parcel_count, bool is_system_vm)
{
	gh_vmid_t vmid = vm->vmid;
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	struct qcom_scm_vmperm srcVM[1] = {{QCOM_SCM_VMID_HLOS, QCOM_SCM_PERM_RWX}};
	struct qcom_scm_vmperm destVM[2] = {{QCOM_SCM_VMID_HLOS, QCOM_SCM_PERM_RWX},
						{vmid, QCOM_SCM_PERM_RWX}};
	u64 srcvmid = BIT(srcVM[0].vmid);
	u64 dstvmid = BIT(destVM[0].vmid) | BIT(destVM[1].vmid);
	phys_addr_t phys;
	ssize_t size;
	int i;
	int ret = 0;

	acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[2]),
			GFP_KERNEL);
	if (!acl_desc)
		return -ENOMEM;

	acl_desc->n_acl_entries = 2;
	acl_desc->acl_entries[0].vmid = QCOM_SCM_VMID_HLOS;
	acl_desc->acl_entries[0].perms =
				GH_RM_ACL_X | GH_RM_ACL_R | GH_RM_ACL_W;

	acl_desc->acl_entries[1].vmid = vmid;
	acl_desc->acl_entries[1].perms =
				GH_RM_ACL_X | GH_RM_ACL_R | GH_RM_ACL_W;

	sgl_desc = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[mem_parcel_count]),
			GFP_KERNEL);
	if (!sgl_desc) {
		kfree(acl_desc);
		return -ENOMEM;
	}

	sgl_desc->n_sgl_entries = mem_parcel_count;

	for (i = 0; i < mem_parcel_count; i++) {
		phys = mem_parcels[i].mem_phys;
		size = mem_parcels[i].mem_size;
		sgl_desc->sgl_entries[i].ipa_base = phys;
		sgl_desc->sgl_entries[i].size = size;

		ret = qcom_scm_assign_mem(phys, size, &srcvmid, destVM, ARRAY_SIZE(destVM));
		if (ret) {
			pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
				&phys, size, vmid, ret);
			goto err_assign_mem;
		}

		srcvmid = BIT(srcVM[0].vmid);
	}

	/*
	 * A system VM is deemed critical for the functioning of the
	 * system. The memory donated to this VM can't be reclaimed
	 * by host OS at any point in time after donating it.
	 * Whereas any memory lent to a non system VM, can be reclaimed
	 * when VM terminates.
	 */
	if (is_system_vm)
		ret = gh_rm_mem_donate(GH_RM_MEM_TYPE_NORMAL, 0, 0,
			acl_desc, sgl_desc, NULL, &vm->mem_handle);
	else
		ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, 0, 0, acl_desc,
				sgl_desc, NULL, &vm->mem_handle);

	if (ret)
		for(i = 0; i < mem_parcel_count; i++) {
			phys = mem_parcels[i].mem_phys;
			size = mem_parcels[i].mem_size;
			ret = qcom_scm_assign_mem(phys, size, &dstvmid, srcVM, ARRAY_SIZE(srcVM));
			if (ret)
				pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
						&phys, size, srcVM[0].vmid, ret);

			dstvmid = BIT(destVM[0].vmid) | BIT(destVM[1].vmid);
		}

err_assign_mem:
	kfree(acl_desc);
	kfree(sgl_desc);
	return ret;
}

int gh_provide_shmem(struct gh_vm *vm, struct gh_shmem *shmems)
{
	gh_vmid_t vmid = shmems->is_phantom ? (shmems->gunyah_label & 0xff) : vm->vmid;
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	int i;
	int ret = 0;
	int src_vmids_count = 1;
	int dst_vmids_count = 1;
	struct qcom_scm_vmperm *srcVM;
	struct qcom_scm_vmperm *dstVM;
	phys_addr_t phys = shmems->base;
	ssize_t size = shmems->size;
	u64 srcvmid = 0;
	u64 dstvmid = 0;

	src_vmids_count += shmems->src_vmids_count;
	srcVM = kzalloc(src_vmids_count * sizeof(struct qcom_scm_vmperm),
						GFP_KERNEL);
	if (!srcVM)
		return -ENOMEM;

	for (i = 0; i < shmems->src_vmids_count; i++)
		set_vmperm_bit(&srcVM[i], &srcvmid, shmems->src_vmids[i], shmems->src_perms[i]);

	set_vmperm_bit(&srcVM[i], &srcvmid, QCOM_SCM_VMID_HLOS, QCOM_SCM_PERM_RWX);

	if (shmems->is_shared) {
		dst_vmids_count = shmems->dst_vmids_count + 2;
		dstVM = kzalloc(dst_vmids_count * sizeof(struct qcom_scm_vmperm),
							GFP_KERNEL);
		if (!dstVM) {
			kfree(srcVM);
			return -ENOMEM;
		}

		for (i = 0; i < shmems->dst_vmids_count; i++)
			set_vmperm_bit(&dstVM[i], &dstvmid, shmems->dst_vmids[i], shmems->dst_perms[i]);

		set_vmperm_bit(&dstVM[i], &dstvmid, QCOM_SCM_VMID_HLOS, QCOM_SCM_PERM_RWX);
		set_vmperm_bit(&dstVM[++i], &dstvmid, vmid, QCOM_SCM_PERM_RWX);
	} else {
		dst_vmids_count += shmems->dst_vmids_count;
		dstVM = kzalloc(dst_vmids_count * sizeof(struct qcom_scm_vmperm),
							GFP_KERNEL);
		if (!dstVM) {
			kfree(srcVM);
			return -ENOMEM;
		}

		for (i = 0; i < shmems->dst_vmids_count; i++)
			set_vmperm_bit(&dstVM[i], &dstvmid, shmems->dst_vmids[i], shmems->dst_perms[i]);

		set_vmperm_bit(&dstVM[i], &dstvmid, vmid, QCOM_SCM_PERM_RWX);
	}

	acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[dst_vmids_count]),
			GFP_KERNEL);
	if (!acl_desc) {
		kfree(srcVM);
		kfree(dstVM);
		return -ENOMEM;
	}

	acl_desc->n_acl_entries = dst_vmids_count;

	for (i = 0; i < dst_vmids_count; i++) {
		acl_desc->acl_entries[i].vmid = dstVM[i].vmid;
		acl_desc->acl_entries[i].perms = dstVM[i].perm;
	}

	sgl_desc = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[1]),
			GFP_KERNEL);
	if (!sgl_desc) {
		kfree(srcVM);
		kfree(dstVM);
		kfree(acl_desc);
		return -ENOMEM;
	}

	sgl_desc->n_sgl_entries = 1;
	sgl_desc->sgl_entries[0].ipa_base = phys;
	sgl_desc->sgl_entries[0].size = size;

	ret = qcom_scm_assign_mem(phys, size, &srcvmid, dstVM, dst_vmids_count);
	if (ret) {
		pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
			 &phys, size, vmid, ret);
		goto err_assign_mem;
	}

	if (shmems->is_shared) {
		ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, 0, shmems->gunyah_label, acl_desc,
								sgl_desc, NULL, &shmems->shmem_handle);
	} else {
		ret = gh_rm_mem_lend (GH_RM_MEM_TYPE_NORMAL, 0, shmems->gunyah_label, acl_desc,
								         sgl_desc, NULL, &shmems->shmem_handle);
	}
	if (ret) {
		ret = qcom_scm_assign_mem(phys, size, &dstvmid, srcVM, src_vmids_count);
		if (ret)
			pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
					&phys, size, srcVM[0].vmid, ret);
	}

err_assign_mem:
	kfree(srcVM);
	kfree(dstVM);
	kfree(acl_desc);
	kfree(sgl_desc);
	return ret;
}

long gh_vm_configure(u16 auth_mech, u64 image_offset,
			u64 image_size, u64 dtb_offset, u64 dtb_size,
			u32 pas_id, const char *fw_name, struct gh_vm *vm)
{
	struct gh_vm_auth_param_entry entry;
	long ret = -EINVAL;
	int nr_vcpus = 0;

	switch (auth_mech) {
	case GH_VM_AUTH_PIL_ELF:
		ret = gh_rm_vm_config_image(vm->vmid, auth_mech,
				vm->mem_handle, image_offset,
				image_size, dtb_offset, dtb_size);
		if (ret) {
			pr_err("VM_CONFIG failed for VM:%d %d\n",
						vm->vmid, ret);
			return ret;
		}
		vm->status.vm_status = GH_RM_VM_STATUS_AUTH;
		if (!pas_id) {
			pr_err("Incorrect pas_id %d for VM:%d\n", pas_id,
						vm->vmid);
			return -EINVAL;
		}
		entry.auth_param_type = GH_VM_AUTH_PARAM_PAS_ID;
		entry.auth_param = pas_id;

		ret = gh_rm_vm_auth_image(vm->vmid, 1, &entry);
		if (ret) {
			pr_err("VM_AUTH_IMAGE failed for VM:%d %d\n",
						vm->vmid, ret);
			return ret;
		}
		vm->status.vm_status = GH_RM_VM_STATUS_INIT;
		break;
	default:
		pr_err("Invalid auth mechanism for VM\n");
		return ret;
	}

	ret = gh_rm_vm_init(vm->vmid);
	if (ret) {
		pr_err("VM_INIT_IMAGE failed for VM:%d %d\n",
						vm->vmid, ret);
		return ret;
	}

	gh_wait_for_vm_status(vm, GH_RM_VM_STATUS_READY);

	ret = gh_rm_populate_hyp_res(vm->vmid, fw_name);
	if (ret < 0) {
		pr_err("Failed to populate resources %d\n", ret);
		return ret;
	}

	if (vm->is_secure_vm) {
		nr_vcpus = gh_get_nr_vcpus(vm->vmid);

		if (nr_vcpus < 0) {
			pr_err("Failed to get vcpu count for vm %d ret%d\n",
				vm->vmid, nr_vcpus);
			ret = nr_vcpus;
			return ret;
		} else if (!nr_vcpus) /* Hypervisor scheduled case when at least 1 vcpu is needed */
			nr_vcpus = 1;

		vm->allowed_vcpus = nr_vcpus;
	}

	return ret;
}

static long gh_vm_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct gh_vm *vm = filp->private_data;
	long ret = -EINVAL;

	switch (cmd) {
	case GH_CREATE_VCPU:
		ret = gh_vm_ioctl_create_vcpu(vm, arg);
		break;
	case GH_VM_SET_FW_NAME:
		ret = gh_vm_ioctl_set_fw_name(vm, arg);
		break;
	case GH_VM_GET_FW_NAME:
		ret = gh_vm_ioctl_get_fw_name(vm, arg);
		break;
	case GH_VM_GET_VCPU_COUNT:
		ret = gh_vm_ioctl_get_vcpu_count(vm);
		break;
	case GH_VM_GET_MEM_COUNT:
		ret = gh_vm_ioctl_get_mem_count(vm);
		break;
	case GH_VM_GET_MEM_REGION:
		ret = gh_vm_ioctl_get_mem_region(vm, arg);
		break;
	default:
		ret = gh_virtio_backend_ioctl(vm->fw_name, cmd, arg);
		break;
	}
	return ret;
}

static int gh_vm_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gh_vm *vm = file->private_data;
	int ret = -EINVAL;

	ret = gh_virtio_backend_mmap(vm->fw_name, vma);

	return ret;
}

static int gh_vm_release(struct inode *inode, struct file *filp)
{
	struct gh_vm *vm = filp->private_data;

	gh_put_vm(vm);
	return 0;
}

/**
 * gh_vm_llseek - llseek implementation just for passing sanity check before mmap
 * @file:	file structure to seek on
 * @offset:	file offset to seek to
 * @whence:	type of seek
 *
 * Note! This is an implementation of ->llseek useable for the special case
 * when userspace expects the seek checking to succeed and never leads to
 * extend EOF before doing mmap. Therefore, It seems that file buffer is
 * LONG_MAX bytes.
 */

static loff_t gh_vm_llseek(struct file *file, loff_t offset, int whence) {
	loff_t new_pos;
	switch (whence) {
		case SEEK_CUR:
			new_pos = file->f_pos + offset;
			break;
		case SEEK_END:
			new_pos = LONG_MAX + offset;
			break;
		case SEEK_SET:
			new_pos = offset;
			break;
		default:
			pr_err("whence %d is not support!\n", whence);
			return -EINVAL;
	}

	if (new_pos < 0 || new_pos > LONG_MAX) {
		pr_err("out of boundary\n");
		return -EINVAL;
	}

	file->f_pos = new_pos;

	return new_pos;
}

static const struct file_operations gh_vm_fops = {
	.unlocked_ioctl = gh_vm_ioctl,
	.mmap = gh_vm_mmap,
	.release = gh_vm_release,
	.llseek = gh_vm_llseek,
};

static struct gh_vm *gh_create_vm(void)
{
	struct gh_vm *vm;
	int ret;

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return ERR_PTR(-ENOMEM);

	mutex_init(&vm->vm_lock);
	spin_lock_init(&vm->susp_vm_lock);
	vm->rm_nb.priority = 1;
	vm->rm_nb.notifier_call = gh_vm_rm_notifier_fn;
	ret = gh_rm_register_notifier(&vm->rm_nb);
	if (ret) {
		mutex_destroy(&vm->vm_lock);
		kfree(vm);
		return ERR_PTR(ret);
	}
	kref_init(&vm->kref);
	init_waitqueue_head(&vm->vm_status_wait);
	init_waitqueue_head(&vm->vm_exit_ioc_wait);
	vm->status.vm_status = GH_RM_VM_STATUS_NO_STATE;
	vm->exit_type = -EINVAL;
	vm->susp_irq = -EINVAL;
	vm->vm_suspend_type = VM_STATE_RUNNING;
	spin_lock(&vm_list_lock);
	list_add(&vm->list, &vm_list);
	spin_unlock(&vm_list_lock);

	return vm;
}

static long gh_dev_ioctl_create_vm(unsigned long arg)
{
	struct gh_vm *vm;
	struct file *file;
	int fd, err;

	vm = gh_create_vm();
	if (IS_ERR_OR_NULL(vm))
		return PTR_ERR(vm);

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		err = fd;
		goto err_destroy_vm;
	}

	file = anon_inode_getfile("gunyah-vm", &gh_vm_fops, vm, O_RDWR);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto err_put_fd;
	}

	file->f_mode |= FMODE_LSEEK;

	fd_install(fd, file);

	return fd;

err_put_fd:
	put_unused_fd(fd);
err_destroy_vm:
	gh_put_vm(vm);
	return err;
}

static long gh_dev_ioctl_wait_for_exit(unsigned long arg)
{
	struct gh_vm *vm;
	struct gh_fw_name_and_exit_status vm_name_and_status;
	int ret = 0;

	if (copy_from_user(&vm_name_and_status, (void __user *)arg,
				sizeof(vm_name_and_status)))
		return -EFAULT;

	vm = find_and_get_vm_by_name(vm_name_and_status.name);
	if (!vm)
		return -EINVAL;

	ret = gh_ioc_wait_for_vm_status(vm, GH_RM_VM_STATUS_EXITED);
	if (ret)
		goto err_put_vm;

	vm_name_and_status.reason = (u32)vm->exit_type;
	if (copy_to_user((void __user *)arg, &vm_name_and_status,
				sizeof(vm_name_and_status))) {
		ret = -EFAULT;
		goto err_put_vm;
	}

err_put_vm:
	gh_put_vm(vm);
	return ret;
}

static long gh_dev_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	long ret = -EINVAL;

	switch (cmd) {
	case GH_CREATE_VM:
		ret = gh_dev_ioctl_create_vm(arg);
		break;
	case GH_VM_WAIT_FOR_EXIT:
		ret = gh_dev_ioctl_wait_for_exit(arg);
		break;
	default:
		pr_err("Invalid gunyah dev ioctl 0x%lx\n", cmd);
		break;
	}

	return ret;
}

static const struct file_operations gh_dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gh_dev_ioctl,
	.llseek = noop_llseek,
};

static struct miscdevice gh_dev = {
	.name = "gunyah",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &gh_dev_fops,
};

void gh_uevent_notify_change(unsigned int type, struct gh_vm *vm)
{
	struct kobj_uevent_env *env;

	env = kzalloc(sizeof(*env), GFP_KERNEL_ACCOUNT);
	if (!env)
		return;

	if (type == GH_EVENT_CREATE_VM)
		add_uevent_var(env, "EVENT=create");
	else if (type == GH_EVENT_DESTROY_VM) {
		add_uevent_var(env, "EVENT=destroy");
		add_uevent_var(env, "vm_exit=%d", vm->exit_type);
	}
	else if (type == GH_EVENT_VM_SUSPENDED) {
		add_uevent_var(env, "EVENT=suspended");
		add_uevent_var(env, "vm_suspend_type=%lld", vm->vm_suspend_type);
	}

	add_uevent_var(env, "vm_name=%s", vm->fw_name);
	env->envp[env->envp_idx++] = NULL;
	kobject_uevent_env(&gh_dev.this_device->kobj, KOBJ_CHANGE, env->envp);
	kfree(env);
}

/*
static irqreturn_t gh_susp_irq_handler(int irq, void *data)
{
	int ret;
	uint64_t vpmg_state;
	gh_capid_t vpmg_cap_id = 0;
	struct gh_vm *vm;
	unsigned long flags;

	vm = find_vm_by_susp_irq(irq);
	if (!vm){
		pr_err("Failed to get vm for irq=%d\n", irq);
		return IRQ_HANDLED;
	}

	ret = gh_hcall_vpm_group_get_state(vm->cap_id, &vpmg_state);
	if (ret) {
		pr_err("Failed to get VPM Group state for cap_id=%llu ret=%d\n",
			vpmg_cap_id, ret);
		return IRQ_HANDLED;
	}

	if (vpmg_state == VM_STATE_RUNNING) {
		pr_debug("VM is in running state\n");
	}
	else if (vpmg_state == VM_STATE_CPU_SUSPENDED ||
		 vpmg_state == VM_STATE_SYSTEM_SUSPENDED) {
		spin_lock_irqsave(&vm->susp_vm_lock, flags);
		vm->vm_suspend_type = vpmg_state;
		spin_unlock_irqrestore(&vm->susp_vm_lock, flags);
		gh_uevent_notify_change(GH_EVENT_VM_SUSPENDED, vm);
		pr_debug("VM is in system suspend state\n");
	}
	else
		pr_err("VPM Group state invalid/non-existent\n");

	return IRQ_HANDLED;
}

static int set_vm_vpm_grp_info(gh_vmid_t vmid, gh_capid_t cap_id, int virq_num)
{
	int ret = 0;
	struct gh_vm *vm;
	char susp_label[MAX_VM_SUSP_LABEL];

	if (virq_num < 0) {
		pr_err("%s: Invalid IRQ number\n", __func__);
		return -EINVAL;
	}

	snprintf(susp_label, sizeof(susp_label), "vm_%d_susp_irq", vmid);
	ret = request_irq(virq_num, gh_susp_irq_handler, 0, susp_label, NULL);
	if (ret < 0) {
		pr_err("%s: IRQ registration failed ret=%d\n", __func__, ret);
		return ret;
	}

	vm = find_vm_by_id(vmid);
	if (vm) {
		vm->cap_id = cap_id;
		vm->susp_irq = virq_num;
	}

	return ret;
}

static int reset_vm_vpm_grp_info(gh_vmid_t vmid, int *irq)
{
	struct gh_vm *vm;

	vm = find_vm_by_id(vmid);
	if (vm && vm->susp_irq != -EINVAL) {
		mutex_lock(&vm->vm_lock);
		*irq = vm->susp_irq;
		free_irq(vm->susp_irq, NULL);
		vm->susp_irq = -EINVAL;
		mutex_unlock(&vm->vm_lock);
	}

	return 0;
}
*/

static int __init gh_init(void)
{
	int ret;

	ret = gh_secure_vm_loader_init();
	if (ret)
		pr_err("gunyah: secure loader init failed %d\n", ret);

	ret = gh_proxy_sched_init();
	if (ret)
		pr_err("gunyah: proxy scheduler init failed %d\n", ret);

	/*
	ret = gh_rm_set_vpm_grp_cb(&set_vm_vpm_grp_info);
	if (ret)
		pr_err("gunyah: rm set vpm callback failed %d\n", ret);

	ret = gh_rm_reset_vpm_grp_cb(&reset_vm_vpm_grp_info);
	if (ret) {
		pr_err("gunyah: rm reset vpm callback failed\n");
		return ret;
	}
	*/

	ret = misc_register(&gh_dev);
	if (ret) {
		pr_err("gunyah: misc device register failed %d\n", ret);
		goto err_gh_init;
	}

	ret = gh_virtio_backend_init();
	if (ret)
		pr_err("gunyah: virtio backend init failed %d\n", ret);

	enable_gvm_ramdump_debugfs();

	return ret;

err_gh_init:
	gh_proxy_sched_exit();
	gh_secure_vm_loader_exit();
	return 0;
}
module_init(gh_init);

static void __exit gh_exit(void)
{
	misc_deregister(&gh_dev);
	gh_proxy_sched_exit();
	cleanup_gvm_ramdump_list();
	gh_secure_vm_loader_exit();
	gh_virtio_backend_exit();
}
module_exit(gh_exit);

MODULE_LICENSE("GPL v2");
