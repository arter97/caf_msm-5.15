/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _SCMI_VIRTIO_BE_H
#define _SCMI_VIRTIO_BE_H

void *scmi_virtio_get_client_priv(
	const struct scmi_virtio_client_h *client_h);

void scmi_virtio_set_client_priv(
	const struct scmi_virtio_client_h *client_h,
	void *priv);

#define DECLARE_SCMI_VIRTIO_REGISTER_UNREGISTER(protocol)          \
	int __init scmi_virtio_##protocol##_register(void);        \
	void __exit scmi_virtio_##protocol##_unregister(void)

DECLARE_SCMI_VIRTIO_REGISTER_UNREGISTER(base);
#endif /* _SCMI_VIRTIO_BE_H */
