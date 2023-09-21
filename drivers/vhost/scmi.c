// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/eventfd.h>
#include <linux/vhost.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/vmalloc.h>
#include <uapi/linux/virtio_scmi.h>
#include <linux/scmi_virtio_backend.h>

#include "vhost.h"
#include "../firmware/arm_scmi/virtio_backend/client_handle.h"

enum {
	VHOST_SCMI_VQ_TX = 0,
	VHOST_SCMI_VQ_RX = 1,
	VHOST_SCMI_VQ_NUM = 2,
};

/*
 * P2A_CHANNELS and SHARED_MEMORY features are negotiated,
 * based on whether backend device supports these features.
 */
enum {
	VHOST_SCMI_FEATURES = VHOST_FEATURES,
};

struct vhost_scmi {
	struct vhost_dev dev;
	struct vhost_virtqueue vqs[VHOST_SCMI_VQ_NUM];
	/* Single request/response msg per VQ */
	void *req_msg_payld;
	void *resp_msg_payld;
	struct scmi_virtio_client_h *client_handle;
	atomic_t release_fe_channels;
	int n_ring_init;
	int n_vring_call_init;
	int n_vring_kick_init;
	int scmi_start_done;
};

static int vhost_scmi_handle_request(struct vhost_virtqueue *vq,
	struct vhost_scmi *vh_scmi, const struct iovec *iov,
	size_t req_sz, size_t *resp_sz)
{
	int ret;
	struct iov_iter req_iter;

	struct scmi_virtio_be_msg req_msg_payld = {
		.msg_payld = vh_scmi->req_msg_payld,
		.msg_sz = req_sz,
	};
	struct scmi_virtio_be_msg resp_msg_payld = {
		.msg_payld = vh_scmi->resp_msg_payld,
		.msg_sz = *resp_sz,
	};

	/* Clear request and response buffers */
	memset(vh_scmi->req_msg_payld, 0, VIRTIO_SCMI_MAX_PDU_SIZE);
	memset(vh_scmi->resp_msg_payld, 0, VIRTIO_SCMI_MAX_PDU_SIZE);

	iov_iter_init(&req_iter, READ, iov, 1, req_sz);

	if (unlikely(!copy_from_iter_full(
		vh_scmi->req_msg_payld, req_sz, &req_iter))) {
		vq_err(vq, "Faulted on SCMI request copy\n");
		return -EFAULT;
	}

	ret = scmi_virtio_be_request(vh_scmi->client_handle, &req_msg_payld,
				   &resp_msg_payld);
	*resp_sz = resp_msg_payld.msg_sz;

	return ret;
}

static int vhost_scmi_send_resp(struct vhost_scmi *vh_scmi,
	const struct iovec *iov, size_t resp_sz)
{
	void *resp = iov->iov_base;

	return copy_to_user(resp, vh_scmi->resp_msg_payld, resp_sz);
}

static void handle_scmi_rx_kick(struct vhost_work *work)
{
	pr_err("%s: unexpected call for rx kick\n", __func__);
}

static void handle_scmi_tx(struct vhost_scmi *vh_scmi)
{
	struct vhost_virtqueue *vq = &vh_scmi->vqs[VHOST_SCMI_VQ_TX];
	unsigned int out, in;
	int head;
	size_t req_sz, resp_sz = 0, orig_resp_sz;
	int ret = 0;

	mutex_lock(&vq->mutex);
	if (!vhost_vq_get_backend(vq)) {
		mutex_unlock(&vq->mutex);
		return;
	}

	vhost_disable_notify(&vh_scmi->dev, vq);

	for (;;) {
		/*
		 * Skip descriptor processing, if teardown has started for the client.
		 * Enforce visibility by using atomic_add_return().
		 */
		if (unlikely(atomic_add_return(0, &vh_scmi->release_fe_channels)))
			break;

		head = vhost_get_vq_desc(vq, vq->iov,
					 ARRAY_SIZE(vq->iov),
					 &out, &in,
					 NULL, NULL);
		/* On error, stop handling until the next kick. */
		if (unlikely(head < 0))
			break;
		/*
		 * Nothing new? Check if any new entry is available -
		 * vhost_enable_notify() returns non-zero.
		 * Otherwise wait for eventfd to tell us that client
		 * refilled.
		 */
		if (head == vq->num) {
			if (unlikely(vhost_enable_notify(&vh_scmi->dev, vq))) {
				vhost_disable_notify(&vh_scmi->dev, vq);
				continue;
			}
			break;
		}

		/*
		 * Each new scmi request over Virtio transport is a descriptor
		 * chain, consisting of 2 descriptors. First descriptor is
		 * the request desc and the second one is for response.
		 * vhost_get_vq_desc() checks the order of these 2 descriptors
		 * in the chain. Check for correct number of each:
		 * "in" = Number of response descriptors.
		 * "out" = Number of request descriptors.
		 *
		 * Note: All descriptor entries, which result in vhost error,
		 * are skipped. This will result in SCMI Virtio clients
		 * for these descs to timeout. At this layer, we do not have
		 * enough information, to populate the response descriptor.
		 * As a possible future enhancement, client can be enhanced
		 * to handle 0 length response descriptors, and we signal
		 * 0 length response descriptor here, on vhost errors.
		 */
		if (in != 1 || out != 1) {
			vq_err(vq, "Unexpected req(%d)/resp(%d) buffers\n",
			       out, in);
			continue;
		}

		req_sz = iov_length(vq->iov, out);
		resp_sz = orig_resp_sz = iov_length(&vq->iov[out], out);
		/* Sanitize request and response buffer size */
		if (!req_sz || !resp_sz
			|| req_sz > VIRTIO_SCMI_MAX_PDU_SIZE
			|| orig_resp_sz > VIRTIO_SCMI_MAX_PDU_SIZE) {
			vq_err(vq,
			  "Unexpected len for SCMI req(%#zx)/resp(%#zx)\n",
			  req_sz, orig_resp_sz);
			goto error_scmi_tx;
		}

		ret = vhost_scmi_handle_request(vq, vh_scmi, vq->iov, req_sz, &resp_sz);
		if (ret) {
			vq_err(vq, "Handle request failed with error : %d\n", ret);
			goto error_scmi_tx;
		}

		if (resp_sz > orig_resp_sz) {
			vq_err(vq,
			  "Unexpected response size: %#zx orig: %#zx\n",
			  resp_sz, orig_resp_sz);
			goto error_scmi_tx;
		}

		ret = vhost_scmi_send_resp(vh_scmi, &vq->iov[in], resp_sz);
		if (ret) {
			vq_err(vq, "Handle request failed with error : %d\n",
				ret);
			goto error_scmi_tx;
		}
		goto scmi_tx_signal;
error_scmi_tx:
		resp_sz = 0;
scmi_tx_signal:
		vhost_add_used_and_signal(&vh_scmi->dev, vq, head, resp_sz);
		/*
		 * Implement vhost_exceeds_weight(), to avoid flooding of SCMI
		 * requests and starvation of other virtio clients?
		 */
	}

	mutex_unlock(&vq->mutex);
}

static void handle_scmi_tx_kick(struct vhost_work *work)
{
	struct vhost_virtqueue *vq = container_of(work, struct vhost_virtqueue,
						  poll.work);
	struct vhost_scmi *vh_scmi = container_of(vq->dev,
					struct vhost_scmi, dev);

	handle_scmi_tx(vh_scmi);
}

static int vhost_scmi_open(struct inode *inode, struct file *f)
{
	struct vhost_scmi *vh_scmi;
	struct vhost_dev *dev;
	struct vhost_virtqueue **vqs;
	int ret = -ENOMEM;

	/*
	 * This struct is large and allocation could fail, fall back to vmalloc
	 * if there is no other way.
	 */
	vh_scmi = kvzalloc(sizeof(*vh_scmi), GFP_KERNEL | __GFP_RETRY_MAYFAIL);
	if (!vh_scmi)
		return ret;

	vqs = kcalloc(VHOST_SCMI_VQ_NUM, sizeof(*vqs), GFP_KERNEL);
	if (!vqs)
		goto free_vh_scmi;

	vh_scmi->req_msg_payld = kzalloc(VIRTIO_SCMI_MAX_PDU_SIZE,
						GFP_KERNEL);
	if (!vh_scmi->req_msg_payld)
		goto free_vqs;

	vh_scmi->resp_msg_payld = kzalloc(VIRTIO_SCMI_MAX_PDU_SIZE,
						GFP_KERNEL);
	if (!vh_scmi->resp_msg_payld)
		goto free_req_msg;

	vh_scmi->client_handle = scmi_virtio_get_client_h(vh_scmi);
	if (!vh_scmi->client_handle)
		goto free_resp_msg;

	dev = &vh_scmi->dev;
	vqs[VHOST_SCMI_VQ_RX] = &vh_scmi->vqs[VHOST_SCMI_VQ_RX];
	vqs[VHOST_SCMI_VQ_TX] = &vh_scmi->vqs[VHOST_SCMI_VQ_TX];
	vh_scmi->vqs[VHOST_SCMI_VQ_RX].handle_kick = handle_scmi_rx_kick;
	vh_scmi->vqs[VHOST_SCMI_VQ_TX].handle_kick = handle_scmi_tx_kick;
	/* Use kworker and disable weight checks */
	vhost_dev_init(dev, vqs, VHOST_SCMI_VQ_NUM, UIO_MAXIOV,
		       0, 0, true, NULL);

	f->private_data = vh_scmi;
	ret = scmi_virtio_be_open(vh_scmi->client_handle);
	if (ret) {
		pr_err("SCMI Virtio backend open() failed with error %d\n", ret);
		goto free_client_handle;
	}
	return ret;

free_client_handle:
	scmi_virtio_put_client_h(vh_scmi->client_handle);
free_resp_msg:
	kfree(vh_scmi->resp_msg_payld);
free_req_msg:
	kfree(vh_scmi->req_msg_payld);
free_vqs:
	kfree(vqs);
free_vh_scmi:
	kvfree(vh_scmi);
	return ret;
}

static int vhost_scmi_start(struct vhost_scmi *vh_scmi)
{
	struct vhost_virtqueue *vq;
	size_t i;
	int ret;

	mutex_lock(&vh_scmi->dev.mutex);

	ret = vhost_dev_check_owner(&vh_scmi->dev);
	if (ret)
		goto err;

	for (i = 0; i < ARRAY_SIZE(vh_scmi->vqs); i++) {
		vq = &vh_scmi->vqs[i];

		mutex_lock(&vq->mutex);

		if (!vhost_vq_access_ok(vq)) {
			ret = -EFAULT;
			goto err_vq;
		}

		if (!vhost_vq_get_backend(vq)) {
			vhost_vq_set_backend(vq, vh_scmi);
			ret = vhost_vq_init_access(vq);
			if (ret)
				goto err_vq;
		}

		mutex_unlock(&vq->mutex);
	}

	mutex_unlock(&vh_scmi->dev.mutex);
	return 0;

err_vq:
	mutex_unlock(&vq->mutex);
	for (i = 0; i < ARRAY_SIZE(vh_scmi->vqs); i++) {
		vq = &vh_scmi->vqs[i];

		mutex_lock(&vq->mutex);
		vhost_vq_set_backend(vq, NULL);
		mutex_unlock(&vq->mutex);
	}
err:
	mutex_unlock(&vh_scmi->dev.mutex);
	return ret;
}

static int vhost_scmi_stop(struct vhost_scmi *vh_scmi, bool check_owner)
{
	int ret = 0, i;
	struct vhost_virtqueue *vq;

	mutex_lock(&vh_scmi->dev.mutex);

	if (check_owner) {
		ret = vhost_dev_check_owner(&vh_scmi->dev);
		if (ret)
			goto err;
	}

	for (i = 0; i < ARRAY_SIZE(vh_scmi->vqs); i++) {
		vq = &vh_scmi->vqs[i];
		mutex_lock(&vq->mutex);
		vhost_vq_set_backend(vq, NULL);
		mutex_unlock(&vq->mutex);
	}
err:
	mutex_unlock(&vh_scmi->dev.mutex);
	return ret;
}

static void vhost_scmi_flush_vq(struct vhost_scmi *vh_scmi, int index)
{
	vhost_work_dev_flush(&vh_scmi->dev);
}

static void vhost_scmi_flush(struct vhost_scmi *vh_scmi)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vh_scmi->vqs); i++)
		vhost_scmi_flush_vq(vh_scmi, i);
}

static int vhost_scmi_release(struct inode *inode, struct file *f)
{
	struct vhost_scmi *vh_scmi = f->private_data;
	int ret = 0;

	atomic_set(&vh_scmi->release_fe_channels, 1);
	vhost_scmi_stop(vh_scmi, false);
	vhost_scmi_flush(vh_scmi);
	vhost_dev_stop(&vh_scmi->dev);
	vhost_dev_cleanup(&vh_scmi->dev);
	/*
	 * We do an extra flush before freeing memory,
	 * since jobs can re-queue themselves.
	 */

	vhost_scmi_flush(vh_scmi);
	ret = scmi_virtio_be_close(vh_scmi->client_handle);
	if (ret)
		pr_err("SCMI Virtio backend  close() failed with error %d\n", ret);
	scmi_virtio_put_client_h(vh_scmi->client_handle);
	kfree(vh_scmi->resp_msg_payld);
	kfree(vh_scmi->req_msg_payld);
	kfree(vh_scmi->dev.vqs);
	kvfree(vh_scmi);

	return ret;
}

static int vhost_scmi_set_features(struct vhost_scmi *vh_scmi, u64 features)
{
	struct vhost_virtqueue *vq;
	int i;

	if (features & ~VHOST_SCMI_FEATURES)
		return -EOPNOTSUPP;

	mutex_lock(&vh_scmi->dev.mutex);
	if ((features & (1 << VHOST_F_LOG_ALL)) &&
	    !vhost_log_access_ok(&vh_scmi->dev)) {
		mutex_unlock(&vh_scmi->dev.mutex);
		return -EFAULT;
	}

	for (i = 0; i < VHOST_SCMI_VQ_NUM; ++i) {
		vq = &vh_scmi->vqs[i];
		mutex_lock(&vq->mutex);
		vq->acked_features = features;
		mutex_unlock(&vq->mutex);
	}
	mutex_unlock(&vh_scmi->dev.mutex);
	return 0;
}

static long vhost_scmi_ioctl(struct file *f, unsigned int ioctl,
			     unsigned long arg)
{
	struct vhost_scmi *vh_scmi = f->private_data;
	void __user *argp = (void __user *)arg;
	u64 __user *featurep = argp;
	u64 features;
	int r;

	switch (ioctl) {
	/* TODO introduce VHOST_SCMI_SET_RUNNING ioctl to get rid of messy code
	 * in the default case?
	 */
	case VHOST_GET_FEATURES:
		features = VHOST_SCMI_FEATURES;
		if (copy_to_user(featurep, &features, sizeof(features)))
			return -EFAULT;
		return 0;
	case VHOST_SET_FEATURES:
		if (copy_from_user(&features, featurep, sizeof(features)))
			return -EFAULT;
		return vhost_scmi_set_features(vh_scmi, features);
	default:
		mutex_lock(&vh_scmi->dev.mutex);
		r = vhost_dev_ioctl(&vh_scmi->dev, ioctl, argp);
		if (r == -ENOIOCTLCMD)
			r = vhost_vring_ioctl(&vh_scmi->dev, ioctl, argp);
		vhost_scmi_flush(vh_scmi);
		mutex_unlock(&vh_scmi->dev.mutex);
		if (r || vh_scmi->scmi_start_done)
			return r;
		if (ioctl == VHOST_SET_VRING_BASE)
			vh_scmi->n_ring_init++;
		else if (ioctl == VHOST_SET_VRING_KICK)
			vh_scmi->n_vring_kick_init++;
		else if (ioctl == VHOST_SET_VRING_CALL)
			vh_scmi->n_vring_call_init++;
		if (vh_scmi->n_ring_init == VHOST_SCMI_VQ_NUM &&
		    vh_scmi->n_vring_kick_init == VHOST_SCMI_VQ_NUM &&
		    vh_scmi->n_vring_call_init == VHOST_SCMI_VQ_NUM) {
			vhost_scmi_start(vh_scmi);
			vh_scmi->scmi_start_done = 1;
		}
		return r;
	}
}

static const struct file_operations vhost_scmi_fops = {
	.owner          = THIS_MODULE,
	.release        = vhost_scmi_release,
	.unlocked_ioctl = vhost_scmi_ioctl,
	.compat_ioctl   = compat_ptr_ioctl,
	.open           = vhost_scmi_open,
	.llseek		= noop_llseek,
};

static struct miscdevice vhost_scmi_misc = {
	MISC_DYNAMIC_MINOR,
	"vhost-scmi",
	&vhost_scmi_fops,
};
module_misc_device(vhost_scmi_misc);

MODULE_SOFTDEP("pre: scmi-virtio-backend");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Host kernel accelerator for SCMI Virtio");
