// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/slab.h>
#include <linux/scmi_virtio_backend.h>

#define to_client_info(clienth) \
	container_of((clienth), struct scmi_virtio_client_info, \
		     client_h)

/*
 * scmi_virtio_client_info - Structure respreseting information
 * associated with a client handle.
 *
 * @client_h: Opaque handle used as agent/client idenitifier.
 * @priv: Private data mainted by SCMI Virtio Backend, for
 *               an agent/client.
 *               This pointer can be used by backend, to maintain,
 *               per protocol private information. This pointer is
 *               typically populated by SCMI backend, on open() call
 *               by client, by calling scmi_virtio_set_client_priv().
 */
struct scmi_virtio_client_info {
	struct scmi_virtio_client_h client_h;
	void *priv;
};

struct scmi_virtio_client_h *scmi_virtio_get_client_h(
	const void *handle)
{
	struct scmi_virtio_client_info *client_info =
		kzalloc(sizeof(*client_info), GFP_KERNEL);

	if (!client_info)
		return NULL;
	client_info->client_h.handle = handle;
	return &client_info->client_h;
}
EXPORT_SYMBOL(scmi_virtio_get_client_h);

void scmi_virtio_put_client_h(
	const struct scmi_virtio_client_h *client_h)
{
	struct scmi_virtio_client_info *client_info =
			to_client_info(client_h);

	kfree(client_info);
}
EXPORT_SYMBOL(scmi_virtio_put_client_h);

void *scmi_virtio_get_client_priv(
		const struct scmi_virtio_client_h *client_h)
{
	struct scmi_virtio_client_info *client_info =
			to_client_info(client_h);

	return client_info->priv;
}

void scmi_virtio_set_client_priv(
		const struct scmi_virtio_client_h *client_h,
		void *priv)
{
	struct scmi_virtio_client_info *client_info =
			to_client_info(client_h);

	client_info->priv = priv;
}
