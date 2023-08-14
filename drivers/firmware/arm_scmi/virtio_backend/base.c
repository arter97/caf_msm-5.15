// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/scmi_protocol.h>
#include <linux/scmi_virtio_backend.h>
#include <linux/slab.h>
#include <linux/bitfield.h>
#include <linux/byteorder/generic.h>

#include "common.h"
#include "backend.h"
#include "client_handle.h"

#include "../common.h"
#include "backend_protocol.h"

#define SCMI_VIRTIO_BASE_NUM_AGENTS	1
#define SCMI_VIRTIO_BASE_AGENT_MASK	GENMASK(15, 8)
#define SCMI_VIRTIO_BASE_NUM_AGENT(num) \
	((u32)FIELD_PREP(SCMI_VIRTIO_BASE_AGENT_MASK, (num)))
#define SCMI_VIRTIO_BASE_NUM_PROT_MASK	GENMASK(7, 0)
#define SCMI_VIRTIO_BASE_NUM_PROT(num) \
	((u32)FIELD_PREP(SCMI_VIRTIO_BASE_NUM_PROT_MASK, (num)))
#define SCMI_VIRTIO_BASE_NUM_AGENT_PROTOCOL(num) \
	(SCMI_VIRTIO_BASE_NUM_AGENT(SCMI_VIRTIO_BASE_NUM_AGENTS) | \
	SCMI_VIRTIO_BASE_NUM_PROT(num))

#define SCMI_VIRTIO_BASE_MAX_VENDORID_STR	16
#define SCMI_VIRTIO_BASE_IMP_VER	1

/*
 * Number of protocols returned in single call to
 * DISCOVER_LIST_PROTOCOLS.
 */
#define SCMI_VIRTIO_BASE_PROTO_LIST_LEN	8

enum scmi_base_protocol_cmd {
	BASE_DISCOVER_VENDOR = 0x3,
	BASE_DISCOVER_SUB_VENDOR = 0x4,
	BASE_DISCOVER_IMPLEMENT_VERSION = 0x5,
	BASE_DISCOVER_LIST_PROTOCOLS = 0x6,
	BASE_DISCOVER_AGENT = 0x7,
	BASE_NOTIFY_ERRORS = 0x8,
	BASE_SET_DEVICE_PERMISSIONS = 0x9,
	BASE_SET_PROTOCOL_PERMISSIONS = 0xa,
	BASE_RESET_AGENT_CONFIGURATION = 0xb,
};

struct scmi_virtio_base_info {
	const struct scmi_virtio_protocol_h *prot_h;
};

static struct scmi_virtio_base_info *scmi_virtio_base_proto_info;

struct scmi_virtio_base_client_info {
	const struct scmi_virtio_client_h *client_h;
	u8 *implemented_protocols;
	int implemented_proto_num;
};

struct scmi_virtio_base_version_resp {
	__le32 status;
	__le32 version;
} __packed;

struct scmi_virtio_base_attr_resp {
	__le32 status;
	__le32 attributes;
} __packed;

struct scmi_virtio_base_msg_attr_resp {
	__le32 status;
	__le32 attributes;
} __packed;

struct scmi_virtio_base_imp_version_resp {
	__le32 status;
	__le32 imp_version;
} __packed;

struct scmi_virtio_base_list_protocols_resp {
	__le32 num_protocols;
	u8 *protocols;
} __packed;

static int scmi_virtio_base_open(
	const struct scmi_virtio_client_h *client_h)
{
	struct scmi_virtio_base_client_info *base_client_info =
		kzalloc(sizeof(*base_client_info),
			GFP_KERNEL);
	if (!base_client_info)
		return -ENOMEM;
	base_client_info->client_h = client_h;
	base_client_info->implemented_protocols = kcalloc(
		sizeof(u8), MAX_PROTOCOLS_IMP,
		GFP_KERNEL);
	if (!base_client_info->implemented_protocols)
		return -ENOMEM;
	base_client_info->implemented_proto_num = MAX_PROTOCOLS_IMP;
	scmi_virtio_implemented_protocols(
		scmi_virtio_base_proto_info->prot_h,
		client_h, base_client_info->implemented_protocols,
		&base_client_info->implemented_proto_num);
	scmi_virtio_set_protocol_priv(
		scmi_virtio_base_proto_info->prot_h,
		client_h, base_client_info);

	return 0;
}

static s32 scmi_virtio_base_version_get(u32 *version)
{
	*version = SCMI_VIRTIO_BE_PROTO_VERSION(2, 0);
	return SCMI_VIRTIO_BE_SUCCESS;
}

static s32 scmi_virtio_base_attributes_get(int num_protocols, u32 *attributes)
{
	*attributes = SCMI_VIRTIO_BASE_NUM_AGENT_PROTOCOL(num_protocols);
	return SCMI_VIRTIO_BE_SUCCESS;
}

static s32 scmi_virtio_base_msg_attibutes_get(u32 msg_id, u32 *attributes)
{
	*attributes = 0;
	switch (msg_id) {
	case PROTOCOL_VERSION:
	case PROTOCOL_ATTRIBUTES:
	case PROTOCOL_MESSAGE_ATTRIBUTES:
	case BASE_DISCOVER_VENDOR:
	case BASE_DISCOVER_IMPLEMENT_VERSION:
	case BASE_DISCOVER_LIST_PROTOCOLS:
		return SCMI_VIRTIO_BE_SUCCESS;
	case BASE_DISCOVER_AGENT:
	case BASE_NOTIFY_ERRORS:
	case BASE_SET_DEVICE_PERMISSIONS:
	case BASE_SET_PROTOCOL_PERMISSIONS:
	case BASE_RESET_AGENT_CONFIGURATION:
		return SCMI_VIRTIO_BE_NOT_FOUND;
	default:
		return SCMI_VIRTIO_BE_NOT_FOUND;
	}
}

static s32 scmi_virtio_base_vendor_get(char *vendor_id)
{
	char vendor[SCMI_VIRTIO_BASE_MAX_VENDORID_STR] = "SCMI-VIRTIO-BE";

	strscpy(vendor_id, vendor, SCMI_VIRTIO_BASE_MAX_VENDORID_STR);
	return SCMI_VIRTIO_BE_SUCCESS;
}

static s32 scmi_virtio_base_imp_version_get(u32 *imp_version)
{
	*imp_version = SCMI_VIRTIO_BASE_IMP_VER;
	return SCMI_VIRTIO_BE_SUCCESS;
}

static s32 scmi_virtio_protocol_list_get(
	const u8 *implemented_protocols, const int num_protocols_imp,
	u32 skip, u32 *num_protocols,
	u8 *protocols, int protocols_len)
{
	int i = 0;

	if (skip >= num_protocols_imp) {
		*num_protocols = 0;
		return SCMI_VIRTIO_BE_INVALID_PARAMETERS;
	}
	while (i < protocols_len && skip < num_protocols_imp) {
		protocols[i] = implemented_protocols[skip];
		i++;
		skip++;
	}
	*num_protocols = i;
	return SCMI_VIRTIO_BE_SUCCESS;
}

static int debug_resp_size(
	struct scmi_virtio_be_payload_buf *resp_payload_buf,
	u8 msg_id, size_t required_size)
{
	if (resp_payload_buf->payload_size < required_size) {
		pr_err("Invalid response buffer size : %#zx required: %#zx for protocol: %#x msg: %#x\n",
			resp_payload_buf->payload_size, required_size,
			SCMI_PROTOCOL_BASE, msg_id);
		return -EINVAL;
	}
	return 0;
}

static int handle_protocol_ver(
	struct scmi_virtio_be_payload_buf *resp_payload_buf)
{
	int err;
	s32 ret;
	u32 version = 0;
	size_t msg_resp_size;
	struct scmi_virtio_base_version_resp *ver_response;

	msg_resp_size = sizeof(version) + sizeof(ret);
	err = debug_resp_size(resp_payload_buf, PROTOCOL_VERSION,
				msg_resp_size);
	if (err)
		return err;
	ret = scmi_virtio_base_version_get(&version);
	ver_response = (struct scmi_virtio_base_version_resp *)resp_payload_buf->payload;
	ver_response->status = cpu_to_le32(ret);
	ver_response->version = cpu_to_le32(version);
	resp_payload_buf->payload_size = msg_resp_size;

	return 0;
}

static int handle_protocol_attrs(int num_protocols,
	struct scmi_virtio_be_payload_buf *resp_payload_buf)
{
	int err;
	s32 ret;
	u32 attributes = 0;
	size_t msg_resp_size;
	struct scmi_virtio_base_attr_resp *attr_response;

	msg_resp_size = sizeof(attributes) + sizeof(ret);
	err = debug_resp_size(resp_payload_buf, PROTOCOL_ATTRIBUTES,
				 msg_resp_size);
	if (err)
		return err;
	ret = scmi_virtio_base_attributes_get(num_protocols, &attributes);
	attr_response = (struct scmi_virtio_base_attr_resp *)resp_payload_buf->payload;
	attr_response->status = cpu_to_le32(ret);
	attr_response->attributes = cpu_to_le32(attributes);
	resp_payload_buf->payload_size = msg_resp_size;

	return 0;
}

static int handle_msg_attrs(
	const struct scmi_virtio_be_payload_buf *req_payload_buf,
	struct scmi_virtio_be_payload_buf *resp_payload_buf)
{
	int err;
	s32 ret;
	u32 req_msg_id = 0;
	size_t msg_resp_size;
	u32 msg_attributes = 0;
	struct scmi_virtio_base_msg_attr_resp *msg_attr_response;

	msg_resp_size = sizeof(msg_attributes) + sizeof(ret);
	err = debug_resp_size(
		resp_payload_buf, PROTOCOL_MESSAGE_ATTRIBUTES,
		msg_resp_size);
	if (err)
		return err;
	if (req_payload_buf->payload_size < sizeof(req_msg_id)) {
		pr_err("Invalid request buffer size : %#zx required: %#zx for protocol: %#x msg: %#x\n",
			req_payload_buf->payload_size, sizeof(req_msg_id),
			SCMI_PROTOCOL_BASE, PROTOCOL_MESSAGE_ATTRIBUTES);
		return -EINVAL;
	}
	req_msg_id = le32_to_cpu(*((__le32 *)req_payload_buf->payload));
	ret = scmi_virtio_base_msg_attibutes_get(req_msg_id, &msg_attributes);
	msg_attr_response =
		(struct scmi_virtio_base_msg_attr_resp *)resp_payload_buf->payload;
	msg_attr_response->status = cpu_to_le32(ret);
	msg_attr_response->attributes = cpu_to_le32(msg_attributes);
	resp_payload_buf->payload_size = msg_resp_size;

	return 0;
}

static int handle_discover_vendor(
	struct scmi_virtio_be_payload_buf *resp_payload_buf)
{
	int err;
	s32 ret;
	size_t vendor_id_sz;
	size_t msg_resp_size;
	void *vendor_payload;
	char vendor[SCMI_VIRTIO_BASE_MAX_VENDORID_STR];

	msg_resp_size = (size_t)SCMI_VIRTIO_BASE_MAX_VENDORID_STR +
			sizeof(ret);
	err = debug_resp_size(
		resp_payload_buf, BASE_DISCOVER_VENDOR,
		msg_resp_size);
	ret = scmi_virtio_base_vendor_get(vendor);
	WARN_ON(strlen(vendor) >= SCMI_VIRTIO_BASE_MAX_VENDORID_STR);
	vendor_id_sz = (size_t)min_t(size_t, (size_t)(strlen(vendor) + 1),
					(size_t)SCMI_VIRTIO_BASE_MAX_VENDORID_STR);
	*(__le32 *)resp_payload_buf->payload = cpu_to_le32(ret);
	vendor_payload =
		(void *)((unsigned long)(uintptr_t)resp_payload_buf->payload +
			 (unsigned long)sizeof(ret));
	memcpy((u8 *)vendor_payload, vendor, vendor_id_sz);
	resp_payload_buf->payload_size = vendor_id_sz + sizeof(ret);

	return 0;
}

static int handle_discover_imp_ver(
	struct scmi_virtio_be_payload_buf *resp_payload_buf)
{
	int err;
	s32 ret;
	u32 imp_version = 0;
	size_t msg_resp_size;
	struct scmi_virtio_base_imp_version_resp *imp_ver_response;

	msg_resp_size = sizeof(imp_version) + sizeof(ret);
	err = debug_resp_size(resp_payload_buf,
				 BASE_DISCOVER_IMPLEMENT_VERSION,
				 msg_resp_size);
	if (err)
		return err;
	ret = scmi_virtio_base_imp_version_get(&imp_version);
	imp_ver_response =
		(struct scmi_virtio_base_imp_version_resp *)resp_payload_buf->payload;
	imp_ver_response->status = cpu_to_le32(ret);
	imp_ver_response->imp_version = cpu_to_le32(imp_version);
	resp_payload_buf->payload_size = msg_resp_size;

	return 0;
}

static int handle_discover_list_protocols(
	const struct scmi_virtio_be_payload_buf *req_payload_buf,
	struct scmi_virtio_be_payload_buf *resp_payload_buf,
	u8 *implemented_protocols, int num_protocols_imp)
{
	int err;
	s32 ret;
	u32 num_protocols = 0;
	size_t protocol_list_size = sizeof(u32);
	size_t msg_resp_size;
	u32 protocol_skip_count;
	void *protocol_list_payload;
	void *protocol_num_payload;
	u8 proto_list[SCMI_VIRTIO_BASE_PROTO_LIST_LEN];

	msg_resp_size = sizeof(u8) * SCMI_VIRTIO_BASE_PROTO_LIST_LEN +
			sizeof(ret);
	err = debug_resp_size(
		resp_payload_buf, BASE_DISCOVER_LIST_PROTOCOLS,
		msg_resp_size);
	if (err)
		return err;
	if (req_payload_buf->payload_size < sizeof(protocol_skip_count)) {
		pr_err("Invalid request buffer size : %#zx required: %#zx for protocol: %#x msg: %#x\n",
			req_payload_buf->payload_size, sizeof(protocol_skip_count),
			SCMI_PROTOCOL_BASE, BASE_DISCOVER_LIST_PROTOCOLS);
		return -EINVAL;
	}
	protocol_skip_count =
		le32_to_cpu((*(__le32 *)req_payload_buf->payload));
	ret = scmi_virtio_protocol_list_get(
		implemented_protocols, num_protocols_imp,
		protocol_skip_count, &num_protocols,
		proto_list, SCMI_VIRTIO_BASE_PROTO_LIST_LEN);
	*(__le32 *)resp_payload_buf->payload = cpu_to_le32(ret);
	protocol_num_payload =
		(void *)((unsigned long)(uintptr_t)resp_payload_buf->payload +
			 (unsigned long)sizeof(ret));
	*(__le32 *)protocol_num_payload = cpu_to_le32(num_protocols);
	if (num_protocols > 0) {
		pr_info("%s: num_protocols:%d\n", __func__, num_protocols);
		protocol_list_payload =
			(void *)((unsigned long)(uintptr_t)resp_payload_buf->payload +
			 (unsigned long)sizeof(ret) + (unsigned long)sizeof(num_protocols));
		memcpy((u8 *)protocol_list_payload, proto_list, num_protocols);
		protocol_list_size = (((num_protocols - 1) / 4) + 1) * sizeof(u32);
	}
	resp_payload_buf->payload_size =
		protocol_list_size + sizeof(num_protocols) + sizeof(ret);

	return 0;
}

static int scmi_virtio_base_msg_handle(
	const struct scmi_virtio_client_h *client_h,
	const struct scmi_virtio_be_msg_hdr *msg_hdr,
	const struct scmi_virtio_be_payload_buf *req_payload_buf,
	struct scmi_virtio_be_payload_buf *resp_payload_buf)
{
	int err;
	size_t msg_resp_size;
	struct scmi_virtio_base_client_info *base_client_info;

	base_client_info = (struct scmi_virtio_base_client_info *)
		scmi_virtio_get_protocol_priv(
			scmi_virtio_base_proto_info->prot_h,
			client_h);
	WARN_ON(client_h != base_client_info->client_h);

	WARN_ON(msg_hdr->protocol_id != SCMI_PROTOCOL_BASE);
	switch (msg_hdr->msg_id) {
	case PROTOCOL_VERSION:
		return handle_protocol_ver(resp_payload_buf);
	case PROTOCOL_ATTRIBUTES:
		return handle_protocol_attrs(
			base_client_info->implemented_proto_num,
			resp_payload_buf);
	case PROTOCOL_MESSAGE_ATTRIBUTES:
		return handle_msg_attrs(req_payload_buf, resp_payload_buf);
	case BASE_DISCOVER_VENDOR:
		return handle_discover_vendor(resp_payload_buf);
	case BASE_DISCOVER_IMPLEMENT_VERSION:
		return handle_discover_imp_ver(resp_payload_buf);
	case BASE_DISCOVER_LIST_PROTOCOLS:
		return handle_discover_list_protocols(
			req_payload_buf, resp_payload_buf,
			base_client_info->implemented_protocols,
			base_client_info->implemented_proto_num);
	default:
		pr_err("Msg id %#x not supported for base protocol\n",
				msg_hdr->msg_id);
		msg_resp_size = sizeof(u32);
		err = debug_resp_size(resp_payload_buf, msg_hdr->msg_id,
					 msg_resp_size);
		if (err)
			return err;
		*(__le32 *)resp_payload_buf->payload =
				cpu_to_le32(SCMI_VIRTIO_BE_NOT_FOUND);
		resp_payload_buf->payload_size = sizeof(u32);
		break;
	}

	return 0;
}


static int scmi_virtio_base_close(
	const struct scmi_virtio_client_h *client_h)
{
	struct scmi_virtio_base_client_info *base_client_info;

	base_client_info = (struct scmi_virtio_base_client_info *)
		scmi_virtio_get_protocol_priv(
			scmi_virtio_base_proto_info->prot_h,
			client_h);
	WARN_ON(client_h != base_client_info->client_h);
	kfree(base_client_info->implemented_protocols);
	kfree(base_client_info);
	return 0;
}

const struct scmi_virtio_protocol_ops scmi_virtio_base_ops = {
	.open = scmi_virtio_base_open,
	.msg_handle = scmi_virtio_base_msg_handle,
	.close = scmi_virtio_base_close,
};

int scmi_virtio_base_init_fn(
	const struct scmi_virtio_protocol_h *prot_h)
{
	scmi_virtio_base_proto_info = kzalloc(
		sizeof(*scmi_virtio_base_proto_info),
		GFP_KERNEL);
	if (!scmi_virtio_base_proto_info)
		return -ENOMEM;
	scmi_virtio_base_proto_info->prot_h = prot_h;
	return 0;
}

int scmi_virtio_base_exit_fn(
	const struct scmi_virtio_protocol_h *prot_h)
{
	kfree(scmi_virtio_base_proto_info);
	scmi_virtio_base_proto_info = NULL;

	return 0;
}

const struct scmi_virtio_protocol scmi_virtio_base_protocol = {
	.id = SCMI_PROTOCOL_BASE,
	.prot_init_fn = scmi_virtio_base_init_fn,
	.prot_exit_fn = scmi_virtio_base_exit_fn,
	.prot_ops = &scmi_virtio_base_ops,
};

DEFINE_SCMI_VIRTIO_PROT_REG_UNREG(base, scmi_virtio_base_protocol);
