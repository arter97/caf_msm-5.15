/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _GH_PRIVATE_H
#define _GH_PRIVATE_H

#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_vm.h>
#include <linux/refcount.h>
#include <linux/gunyah.h>
#include <linux/wait.h>

#define GH_EVENT_CREATE_VM 0
#define GH_EVENT_DESTROY_VM 1
#define GH_EVENT_VM_SUSPENDED 2
#define GH_MAX_VCPUS 8
#define GH_MAX_VMIDS 16

struct gh_mem_parcel {
	phys_addr_t mem_phys;
	ssize_t mem_size;
};

struct gh_vcpu {
	u32 vcpu_id;
	struct gh_vm *vm;
};

struct gh_vm {
	bool is_secure_vm; /* is true for Qcom authenticated secure VMs */
	bool vm_run_once;
	u32 created_vcpus;
	u32 allowed_vcpus;
	gh_vmid_t vmid;
	struct gh_vcpu *vcpus[GH_MAX_VCPUS];
	char fw_name[GH_VM_FW_NAME_MAX];
	struct notifier_block rm_nb;
	struct gh_vm_status status;
	wait_queue_head_t vm_status_wait;
	wait_queue_head_t vm_exit_ioc_wait;
	int exit_type;
	struct kref kref;
	gh_memparcel_handle_t mem_handle;
	struct mutex vm_lock;
	struct list_head list;
	gh_capid_t cap_id;
	int susp_irq;
	uint64_t vm_suspend_type;
	spinlock_t susp_vm_lock;
};

struct gh_shmem {
	phys_addr_t base;
	ssize_t size;
	u32 src_vmids[GH_MAX_VMIDS];
	int src_vmids_count;
	u32 dst_vmids[GH_MAX_VMIDS];
	int dst_vmids_count;
	gh_vm_perm_t src_perms[GH_MAX_VMIDS];
	gh_vm_perm_t dst_perms[GH_MAX_VMIDS];
	gh_label_t gunyah_label;
	bool is_shared;
	bool is_phantom;
	gh_memparcel_handle_t shmem_handle;
};

/*
 * memory lending/donating and reclaiming APIs
 */
int gh_provide_mem(struct gh_vm *vm, struct gh_mem_parcel *mem_parcels,
				u32 mem_parcel_count, bool is_system_vm);
int gh_reclaim_mem(struct gh_vm *vm, struct gh_mem_parcel *mem_parcels,
				u32 mem_parcel_count, bool is_system_vm);
int gh_provide_shmem(struct gh_vm *vm, struct gh_shmem *shmems);
int gh_reclaim_shmem(struct gh_vm *vm, struct gh_shmem *shmems);
long gh_vm_configure(u16 auth_mech, u64 image_offset,
			u64 image_size, u64 dtb_offset, u64 dtb_size,
			u32 pas_id, const char *fw_name, struct gh_vm *vm);
void gh_uevent_notify_change(unsigned int type, struct gh_vm *vm);

#endif /* _GH_PRIVATE_H */
