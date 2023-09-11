/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _SCMI_VIRTIO_BE_CLIENT_HANDLE_H
#define _SCMI_VIRTIO_BE_CLIENT_HANDLE_H

/**
 * scmi_virtio_get_client_h: This function in used by callers
 * to construct a unique client handle, enclosing the client handle.
 */
struct scmi_virtio_client_h *scmi_virtio_get_client_h(
	const void *handle);


/**
 * scmi_virtio_put_client_h: This function in used by callers
 * to release the client handle, and any private data associated
 * with it.
 */
void scmi_virtio_put_client_h(
	const struct scmi_virtio_client_h *client_h);

#endif /*_SCMI_VIRTIO_BE_CLIENT_HANDLE_H */
