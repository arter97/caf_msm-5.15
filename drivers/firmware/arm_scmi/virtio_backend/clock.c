// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved. */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/scmi_protocol.h>
#include <linux/scmi_virtio_backend.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "common.h"
#include "backend.h"
#include "backend_protocol.h"
#include "client_handle.h"
#include "../common.h"

static DEFINE_MUTEX(clock_lock);
static LIST_HEAD(clock_list);
unsigned int num_clocks;

struct clock {
	struct clk *clk;
	const char *remote_name;
	u32 id;
	u32 rate;
	bool enabled;

	struct list_head list;
};

struct clock_server {
	struct device *dev;
	const struct scmi_virtio_protocol_h *prot;
};

struct clock_server_client {
	//TODO
};

static struct clock_server *clock_server;

enum clock_commands {
	CLOCK_ATTRIBUTES = 0x3,
	CLOCK_DESCRIBE_RATES = 0x4,
	CLOCK_RATE_SET = 0x5,
	CLOCK_RATE_GET = 0x6,
	CLOCK_CONFIG_SET = 0x7,
};

struct clock_msg_prot_version_resp {
	__le32 status;
	__le32 version;
};

struct clock_msg_prot_attrs_resp {
	__le32 status;
	__le16 num_clocks;
	u8 max_async_req;
	u8 reserved;
};

struct clock_msg_prot_msg_attrs_req {
	__le32 msg_id;
};

struct clock_msg_prot_msg_attrs_resp {
	__le32 status;
	__le32 attributes;
};

struct clock_msg_attrs_req {
	__le32 clock_id;
};

struct clock_msg_attrs_resp {
	__le32 status;
	__le32 attributes;
#define	CLOCK_ENABLE	BIT(0)
	u8 name[SCMI_MAX_STR_SIZE];
};

struct clock_msg_describe_rates_req {
	__le32 id;
	__le32 rate_index;
};

struct clock_msg_describe_rates_resp {
	__le32 status;
	__le32 num_rates_flags;
#define NUM_RETURNED(x)		((x) & 0xfff)
#define RATE_DISCRETE(x)	((!(x)) << 12)
#define NUM_REMAINING(x)	((x) << 16)
#define RATE_IDX_MIN		0
#define RATE_IDX_MAX		1
#define RATE_IDX_STEP		2
	struct {
		__le32 value_low;
		__le32 value_high;
	} rate[3];
};

struct clock_msg_rate_set_req {
	__le32 flags;
#define CLOCK_SET_ASYNC		BIT(0)
#define CLOCK_SET_IGNORE_RESP	BIT(1)
#define CLOCK_SET_ROUND_UP	BIT(2)
#define CLOCK_SET_ROUND_AUTO	BIT(3)
	__le32 id;
	__le32 value_low;
	__le32 value_high;
};

struct clock_msg_rate_set_resp {
	__le32 status;
};

struct clock_msg_rate_get_req {
	__le32 id;
};

struct clock_msg_rate_get_resp {
	__le32 status;
	__le32 value_low;
	__le32 value_high;
};

struct clock_msg_config_set_req {
	__le32 id;
	__le32 attributes;
};

struct clock_msg_config_set_resp {
	__le32 status;
};

struct clock *get_clock(u32 id)
{
	struct clock *clock = NULL;
	struct clock *tmp;

	mutex_lock(&clock_lock);

	list_for_each_entry(tmp, &clock_list, list) {
		if (tmp->id == id) {
			clock = tmp;
			break;
		}
	}

	mutex_unlock(&clock_lock);

	return clock;
}

static int debug_buf_size(const struct scmi_virtio_be_payload_buf *req,
			     u8 msg_id, size_t required_size, const char *label)
{
	if (req->payload_size < required_size) {
		dev_err(clock_server->dev, "Invalid %s buffer size: %#zx required: %#zx for protocol: %#x msg: %#x\n",
			label, req->payload_size, required_size,
			SCMI_PROTOCOL_VOLTAGE, msg_id);
		return -EINVAL;
	}

	return 0;
}

static int debug_req_size(const struct scmi_virtio_be_payload_buf *req,
			     u8 msg_id, size_t required_size)
{
	return debug_buf_size(req, msg_id, required_size, "request");
}

static int debug_resp_size(struct scmi_virtio_be_payload_buf *resp, u8 msg_id,
			      size_t required_size)
{
	memset(resp->payload, 0, resp->payload_size);
	return debug_buf_size(resp, msg_id, required_size, "response");
}

static int handle_protocol_ver(struct scmi_virtio_be_payload_buf *resp)
{
	struct clock_msg_prot_version_resp *resp_data = resp->payload;
	size_t resp_size = sizeof(*resp_data);
	int err;

	err = debug_resp_size(resp, PROTOCOL_VERSION, resp_size);
	if (err)
		return err;

	resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_SUCCESS);
	resp_data->version = cpu_to_le32(SCMI_VIRTIO_BE_PROTO_VERSION(1, 0));
	resp->payload_size = resp_size;

	return 0;
}

static int handle_protocol_attrs(struct scmi_virtio_be_payload_buf *resp)
{
	struct clock_msg_prot_attrs_resp *resp_data = resp->payload;
	size_t resp_size = sizeof(*resp_data);
	int err;

	err = debug_resp_size(resp, PROTOCOL_ATTRIBUTES, resp_size);
	if (err)
		return err;

	//TODO: Is resp->payload already zero-initialized?
	resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_SUCCESS);
	resp_data->num_clocks = num_clocks;
	resp_data->max_async_req = 0;

	resp->payload_size = resp_size;

	return 0;
}

static s32 clock_msg_supported(u32 msg_id)
{
	switch (msg_id) {
	case PROTOCOL_VERSION:
	case PROTOCOL_ATTRIBUTES:
	case PROTOCOL_MESSAGE_ATTRIBUTES:
	case CLOCK_ATTRIBUTES:
	case CLOCK_DESCRIBE_RATES:
	case CLOCK_RATE_SET:
	case CLOCK_RATE_GET:
	case CLOCK_CONFIG_SET:
		return SCMI_VIRTIO_BE_SUCCESS;
	default:
		return SCMI_VIRTIO_BE_NOT_FOUND;
	}
}

static int handle_protocol_msg_attrs(const struct scmi_virtio_be_payload_buf *req,
				     struct scmi_virtio_be_payload_buf *resp)
{
	const struct clock_msg_prot_msg_attrs_req *req_data = req->payload;
	struct clock_msg_prot_msg_attrs_resp *resp_data = resp->payload;
	size_t resp_size = sizeof(*resp_data);
	s32 ret;
	int err;

	err = debug_req_size(req, PROTOCOL_MESSAGE_ATTRIBUTES, sizeof(*req_data));
	if (err)
		return err;

	err = debug_resp_size(resp, PROTOCOL_MESSAGE_ATTRIBUTES, resp_size);
	if (err)
		return err;

	ret = clock_msg_supported(le32_to_cpu(req_data->msg_id));
	resp_data->status = cpu_to_le32(ret);
	resp_data->attributes = cpu_to_le32(0);
	resp->payload_size = resp_size;

	return 0;
}

static int handle_attrs(const struct scmi_virtio_be_payload_buf *req,
			struct scmi_virtio_be_payload_buf *resp)
{
	const struct clock_msg_attrs_req *req_data = req->payload;
	struct clock_msg_attrs_resp *resp_data = resp->payload;
	size_t resp_size = sizeof(*resp_data);
	struct clock *clock;
	u32 attributes;
	u32 id;
	int err;

	err = debug_req_size(req, CLOCK_ATTRIBUTES, sizeof(*req_data));
	if (err)
		return err;

	err = debug_resp_size(resp, CLOCK_ATTRIBUTES, resp_size);
	if (err)
		return err;

	resp->payload_size = resp_size;
	id = le32_to_cpu(req_data->clock_id);

	clock = get_clock(id);
	if (!clock) {
		resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_NOT_FOUND);
		return 0;
	}

	attributes = 0;

	if (clock->enabled)
		attributes |= CLOCK_ENABLE;

	resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_SUCCESS);
	resp_data->attributes = cpu_to_le32(attributes);
	strscpy(resp_data->name, clock->remote_name, SCMI_MAX_STR_SIZE);

	return 0;
}

static int handle_describe_rates(const struct scmi_virtio_be_payload_buf *req,
				 struct scmi_virtio_be_payload_buf *resp)
{
	const struct clock_msg_describe_rates_req *req_data = req->payload;
	struct clock_msg_describe_rates_resp *resp_data = resp->payload;
	size_t resp_size = sizeof(*resp_data);
	struct clock *clock;
	u32 num_rates_flags;
	int err;

	err = debug_req_size(req, CLOCK_ATTRIBUTES, sizeof(*req_data));
	if (err)
		return err;

	err = debug_resp_size(resp, CLOCK_ATTRIBUTES, resp_size);
	if (err)
		return err;

	resp->payload_size = resp_size;

	clock = get_clock(le32_to_cpu(req_data->id));
	if (!clock) {
		resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_NOT_FOUND);
		return 0;
	}

	resp_data->rate[RATE_IDX_MIN].value_low = 0;
	resp_data->rate[RATE_IDX_MAX].value_low = U32_MAX;
	resp_data->rate[RATE_IDX_STEP].value_low = 1;

	num_rates_flags = 0;
	num_rates_flags |= RATE_DISCRETE(0);
	num_rates_flags |= NUM_RETURNED(3);

	resp_data->num_rates_flags = cpu_to_le32(num_rates_flags);
	resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_SUCCESS);

	return 0;
}

static int handle_rate_set(const struct scmi_virtio_be_payload_buf *req,
			   struct scmi_virtio_be_payload_buf *resp)
{
	const struct clock_msg_rate_set_req *req_data = req->payload;
	struct clock_msg_rate_set_resp *resp_data = resp->payload;
	size_t resp_size = sizeof(*resp_data);
	struct clock *clock;
	u32 rate;
	int err;

	err = debug_req_size(req, CLOCK_ATTRIBUTES, sizeof(*req_data));
	if (err)
		return err;

	err = debug_resp_size(resp, CLOCK_ATTRIBUTES, resp_size);
	if (err)
		return err;

	resp->payload_size = resp_size;

	clock = get_clock(le32_to_cpu(req_data->id));
	if (!clock) {
		resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_NOT_FOUND);
		return 0;
	}

	rate = le32_to_cpu(req_data->value_low);

	mutex_lock(&clock_lock);

	err = clk_set_rate(clock->clk, rate);
	if (!err)
		clock->rate = rate;

	mutex_unlock(&clock_lock);

	resp_data->status = cpu_to_le32(scmi_virtio_linux_err_remap(err));

	return 0;
}

static int handle_rate_get(const struct scmi_virtio_be_payload_buf *req,
			   struct scmi_virtio_be_payload_buf *resp)
{
	const struct clock_msg_rate_get_req *req_data = req->payload;
	struct clock_msg_rate_get_resp *resp_data = resp->payload;
	size_t resp_size = sizeof(*resp_data);
	struct clock *clock;
	int err;

	err = debug_req_size(req, CLOCK_ATTRIBUTES, sizeof(*req_data));
	if (err)
		return err;

	err = debug_resp_size(resp, CLOCK_ATTRIBUTES, resp_size);
	if (err)
		return err;

	resp->payload_size = resp_size;

	clock = get_clock(le32_to_cpu(req_data->id));
	if (!clock) {
		resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_NOT_FOUND);
		return 0;
	}

	resp_data->value_low = cpu_to_le32(clock->rate);
	resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_SUCCESS);

	return 0;
}

static int handle_config_set(const struct scmi_virtio_be_payload_buf *req,
			     struct scmi_virtio_be_payload_buf *resp)
{
	const struct clock_msg_config_set_req *req_data = req->payload;
	struct clock_msg_config_set_resp *resp_data = resp->payload;
	size_t resp_size = sizeof(*resp_data);
	struct clock *clock;
	u32 attributes;
	int err;

	err = debug_req_size(req, CLOCK_ATTRIBUTES, sizeof(*req_data));
	if (err)
		return err;

	err = debug_resp_size(resp, CLOCK_ATTRIBUTES, resp_size);
	if (err)
		return err;

	resp->payload_size = resp_size;

	clock = get_clock(le32_to_cpu(req_data->id));
	if (!clock) {
		resp_data->status = cpu_to_le32(SCMI_VIRTIO_BE_NOT_FOUND);
		return 0;
	}

	attributes = le32_to_cpu(req_data->attributes);

	if (attributes & CLOCK_ENABLE)
		err = clk_prepare_enable(clock->clk);
	else
		clk_disable_unprepare(clock->clk);

	resp_data->status = cpu_to_le32(scmi_virtio_linux_err_remap(err));

	return 0;
}

static int
clock_server_handle_message(const struct scmi_virtio_client_h *client_h,
			    const struct scmi_virtio_be_msg_hdr *header,
			    const struct scmi_virtio_be_payload_buf *req,
			    struct scmi_virtio_be_payload_buf *resp)
{
	struct clock_server_client *client;
	size_t resp_size;
	int err;

	client = scmi_virtio_get_protocol_priv(clock_server->prot, client_h);

	WARN_ON(header->protocol_id != SCMI_PROTOCOL_CLOCK);

	switch (header->msg_id) {
	case PROTOCOL_VERSION:
		return handle_protocol_ver(resp);

	case PROTOCOL_ATTRIBUTES:
		return handle_protocol_attrs(resp);

	case PROTOCOL_MESSAGE_ATTRIBUTES:
		return handle_protocol_msg_attrs(req, resp);

	case CLOCK_ATTRIBUTES:
		return handle_attrs(req, resp);

	case CLOCK_DESCRIBE_RATES:
		return handle_describe_rates(req, resp);

	case CLOCK_RATE_SET:
		return handle_rate_set(req, resp);

	case CLOCK_RATE_GET:
		return handle_rate_get(req, resp);

	case CLOCK_CONFIG_SET:
		return handle_config_set(req, resp);

	default:
		pr_err("Msg id %#x not supported for clock protocol\n",
		       header->msg_id);

		resp_size = sizeof(u32);
		err = debug_resp_size(resp, header->msg_id, resp_size);
		if (err)
			return err;

		*(__le32 *)resp->payload = cpu_to_le32(SCMI_VIRTIO_BE_NOT_FOUND);
		resp->payload_size = sizeof(u32);

		break;
	}

	return 0;
}

int scmi_virtio_register_clock(struct clk_hw *hw, const char *remote_name)
{
	struct clock *clock;

	clock = kzalloc(sizeof(*clock), GFP_KERNEL);
	if (!clock)
		return -ENOMEM;

	clock->remote_name = kstrdup_const(remote_name, GFP_KERNEL);
	if (!clock->remote_name)
		return -ENOMEM;

	clock->clk = hw->clk;

	mutex_lock(&clock_lock);

	clock->id = num_clocks++;
	list_add_tail(&clock->list, &clock_list);

	mutex_unlock(&clock_lock);

	return 0;
}
EXPORT_SYMBOL(scmi_virtio_register_clock);

static int clock_server_open(const struct scmi_virtio_client_h *client_h)
{
	struct clock_server_client *client;

	client = devm_kzalloc(clock_server->dev, sizeof(*client), GFP_KERNEL);
	scmi_virtio_set_protocol_priv(clock_server->prot, client_h, client);

	return 0;
}

static int clock_server_close(const struct scmi_virtio_client_h *client_h)
{
	struct clock_server_client *client;

	client = scmi_virtio_get_protocol_priv(clock_server->prot, client_h);
	return 0;
}

static const struct scmi_virtio_protocol_ops clock_server_ops = {
	.open = clock_server_open,
	.msg_handle = clock_server_handle_message,
	.close = clock_server_close,
};

static int clock_server_init(const struct scmi_virtio_protocol_h *prot)
{
	struct device *dev = prot->dev;

	clock_server = devm_kzalloc(dev, sizeof(*clock_server), GFP_KERNEL);
	if (!clock_server)
		return -ENOMEM;

	clock_server->dev = dev;
	clock_server->prot = prot;

	return 0;
}

static int clock_server_exit(const struct scmi_virtio_protocol_h *prot)
{
	clock_server = NULL;

	return 0;
}

static const struct scmi_virtio_protocol clock_server_protocol = {
	.id = SCMI_PROTOCOL_CLOCK,
	.prot_init_fn = clock_server_init,
	.prot_exit_fn = clock_server_exit,
	.prot_ops = &clock_server_ops,
};

DEFINE_SCMI_VIRTIO_PROT_REG_UNREG(clock, clock_server_protocol);
