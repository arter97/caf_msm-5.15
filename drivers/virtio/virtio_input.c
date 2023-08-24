// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/input.h>
#include <linux/slab.h>

#include <uapi/linux/virtio_ids.h>
#include <uapi/linux/virtio_input.h>
#include <linux/input/mt.h>

struct virtio_input {
	struct virtio_device       *vdev;
	struct input_dev           *idev;
	char                       name[64];
	char                       serial[64];
	char                       phys[64];
	struct virtqueue           *evt, *sts;
	struct virtio_input_event  evts[64];
	spinlock_t                 lock;
	bool                       ready;
};

static void virtinput_queue_evtbuf(struct virtio_input *vi,
				   struct virtio_input_event *evtbuf)
{
	struct scatterlist sg[1];

	sg_init_one(sg, evtbuf, sizeof(*evtbuf));
	virtqueue_add_inbuf(vi->evt, sg, 1, evtbuf, GFP_ATOMIC);
}

static void virtinput_recv_events(struct virtqueue *vq)
{
	struct virtio_input *vi = vq->vdev->priv;
	struct virtio_input_event *event;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&vi->lock, flags);
	if (vi->ready) {
		while ((event = virtqueue_get_buf(vi->evt, &len)) != NULL) {
			spin_unlock_irqrestore(&vi->lock, flags);
			input_event(vi->idev,
				    le16_to_cpu(event->type),
				    le16_to_cpu(event->code),
				    le32_to_cpu(event->value));
			spin_lock_irqsave(&vi->lock, flags);
			virtinput_queue_evtbuf(vi, event);
		}
		virtqueue_kick(vq);
	}
	spin_unlock_irqrestore(&vi->lock, flags);
}

/*
 * On error we are losing the status update, which isn't critical as
 * this is typically used for stuff like keyboard leds.
 */
static int virtinput_send_status(struct virtio_input *vi,
				 u16 type, u16 code, s32 value)
{
	struct virtio_input_event *stsbuf;
	struct scatterlist sg[1];
	unsigned long flags;
	int rc;

	/*
	 * Since 29cc309d8bf1 (HID: hid-multitouch: forward MSC_TIMESTAMP),
	 * EV_MSC/MSC_TIMESTAMP is added to each before EV_SYN event.
	 * EV_MSC is configured as INPUT_PASS_TO_ALL.
	 * In case of touch device:
	 *   BE pass EV_MSC/MSC_TIMESTAMP to FE on receiving event from evdev.
	 *   FE pass EV_MSC/MSC_TIMESTAMP back to BE.
	 *   BE writes EV_MSC/MSC_TIMESTAMP to evdev due to INPUT_PASS_TO_ALL.
	 *   BE receives extra EV_MSC/MSC_TIMESTAMP and pass to FE.
	 *   >>> Each new frame becomes larger and larger.
	 * Disable EV_MSC/MSC_TIMESTAMP forwarding for MT.
	 */
	if (vi->idev->mt && type == EV_MSC && code == MSC_TIMESTAMP)
		return 0;

	stsbuf = kzalloc(sizeof(*stsbuf), GFP_ATOMIC);
	if (!stsbuf)
		return -ENOMEM;

	stsbuf->type  = cpu_to_le16(type);
	stsbuf->code  = cpu_to_le16(code);
	stsbuf->value = cpu_to_le32(value);
	sg_init_one(sg, stsbuf, sizeof(*stsbuf));

	spin_lock_irqsave(&vi->lock, flags);
	if (vi->ready) {
		rc = virtqueue_add_outbuf(vi->sts, sg, 1, stsbuf, GFP_ATOMIC);
		virtqueue_kick(vi->sts);
	} else {
		rc = -ENODEV;
	}
	spin_unlock_irqrestore(&vi->lock, flags);

	if (rc != 0)
		kfree(stsbuf);
	return rc;
}

static void virtinput_recv_status(struct virtqueue *vq)
{
	struct virtio_input *vi = vq->vdev->priv;
	struct virtio_input_event *stsbuf;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&vi->lock, flags);
	while ((stsbuf = virtqueue_get_buf(vi->sts, &len)) != NULL)
		kfree(stsbuf);
	spin_unlock_irqrestore(&vi->lock, flags);
}

static int virtinput_status(struct input_dev *idev, unsigned int type,
			    unsigned int code, int value)
{
	struct virtio_input *vi = input_get_drvdata(idev);

	return virtinput_send_status(vi, type, code, value);
}

static u8 virtinput_cfg_select(struct virtio_input *vi,
			       u8 select, u8 subsel)
{
	u8 size;

	virtio_cwrite_le(vi->vdev, struct virtio_input_config, select, &select);
	virtio_cwrite_le(vi->vdev, struct virtio_input_config, subsel, &subsel);
	virtio_cread_le(vi->vdev, struct virtio_input_config, size, &size);
	return size;
}

static void virtinput_plat_cfg_bits(struct virtio_input *vi, u8 bytes,
					u16 offset, int subsel,
					unsigned long *bits, u32 bitcount, bool is_evbits)
{
	unsigned int bit;
	u8 *virtio_bits;

	if (!bytes)
		return;
	if (bitcount > bytes * 8)
		bitcount = bytes * 8;

	/*
	 * Bitmap in virtio config space is a simple stream of bytes,
	 * with the first byte carrying bits 0-7, second bits 8-15 and
	 * so on.
	 */
	virtio_bits = kzalloc(bytes, GFP_KERNEL);
	if (!virtio_bits)
		return;
	virtio_cread_bytes(vi->vdev, offset,
			   virtio_bits, bytes);
	for (bit = 0; bit < bitcount; bit++) {
		if (virtio_bits[bit / 8] & (1 << (bit % 8)))
			__set_bit(bit, bits);
	}
	kfree(virtio_bits);

	if (is_evbits)
		__set_bit(subsel, vi->idev->evbit);
}

static void virtinput_cfg_bits(struct virtio_input *vi, int select, int subsel,
			       unsigned long *bits, unsigned int bitcount)
{
	unsigned int bit;
	u8 *virtio_bits;
	u8 bytes;

	bytes = virtinput_cfg_select(vi, select, subsel);
	if (!bytes)
		return;
	if (bitcount > bytes * 8)
		bitcount = bytes * 8;

	/*
	 * Bitmap in virtio config space is a simple stream of bytes,
	 * with the first byte carrying bits 0-7, second bits 8-15 and
	 * so on.
	 */
	virtio_bits = kzalloc(bytes, GFP_KERNEL);
	if (!virtio_bits)
		return;
	virtio_cread_bytes(vi->vdev, offsetof(struct virtio_input_config,
					      u.bitmap),
			   virtio_bits, bytes);
	for (bit = 0; bit < bitcount; bit++) {
		if (virtio_bits[bit / 8] & (1 << (bit % 8)))
			__set_bit(bit, bits);
	}
	kfree(virtio_bits);

	if (select == VIRTIO_INPUT_CFG_EV_BITS)
		__set_bit(subsel, vi->idev->evbit);
}

static void virtinput_flat_cfg_abs(struct virtio_input *vi, unsigned int offset,
									int abs)
{
	u32 mi, ma, re, fu, fl;

	virtio_cread_bytes(vi->vdev, offset, &mi, sizeof(mi));
	virtio_cread_bytes(vi->vdev, offset + sizeof(mi), &ma, sizeof(ma));
	virtio_cread_bytes(vi->vdev, offset + sizeof(ma), &re, sizeof(re));
	virtio_cread_bytes(vi->vdev, offset + sizeof(re), &fu, sizeof(fu));
	virtio_cread_bytes(vi->vdev, offset + sizeof(fu), &fl, sizeof(fl));
	input_set_abs_params(vi->idev, abs, mi, ma, fu, fl);
	input_abs_set_res(vi->idev, abs, re);
}

static void virtinput_cfg_abs(struct virtio_input *vi, int abs)
{
	u32 mi, ma, re, fu, fl;

	virtinput_cfg_select(vi, VIRTIO_INPUT_CFG_ABS_INFO, abs);
	virtio_cread_le(vi->vdev, struct virtio_input_config, u.abs.min, &mi);
	virtio_cread_le(vi->vdev, struct virtio_input_config, u.abs.max, &ma);
	virtio_cread_le(vi->vdev, struct virtio_input_config, u.abs.res, &re);
	virtio_cread_le(vi->vdev, struct virtio_input_config, u.abs.fuzz, &fu);
	virtio_cread_le(vi->vdev, struct virtio_input_config, u.abs.flat, &fl);
	input_set_abs_params(vi->idev, abs, mi, ma, fu, fl);
	input_abs_set_res(vi->idev, abs, re);
}

static int virtinput_init_vqs(struct virtio_input *vi)
{
	struct virtqueue *vqs[2];
	vq_callback_t *cbs[] = { virtinput_recv_events,
				 virtinput_recv_status };
	static const char * const names[] = { "events", "status" };
	int err;

	err = virtio_find_vqs(vi->vdev, 2, vqs, cbs, names, NULL);
	if (err)
		return err;
	vi->evt = vqs[0];
	vi->sts = vqs[1];

	return 0;
}

static void virtinput_fill_evt(struct virtio_input *vi)
{
	unsigned long flags;
	int i, size;

	spin_lock_irqsave(&vi->lock, flags);
	size = virtqueue_get_vring_size(vi->evt);
	if (size > ARRAY_SIZE(vi->evts))
		size = ARRAY_SIZE(vi->evts);
	for (i = 0; i < size; i++)
		virtinput_queue_evtbuf(vi, &vi->evts[i]);
	virtqueue_kick(vi->evt);
	spin_unlock_irqrestore(&vi->lock, flags);
}

static void virtinput_get_config(struct virtio_input *vi)
{
	size_t size;
	int abs;

	size = virtinput_cfg_select(vi, VIRTIO_INPUT_CFG_ID_NAME, 0);
	virtio_cread_bytes(vi->vdev, offsetof(struct virtio_input_config,
					      u.string),
			   vi->name, min(size, sizeof(vi->name)));
	size = virtinput_cfg_select(vi, VIRTIO_INPUT_CFG_ID_SERIAL, 0);
	virtio_cread_bytes(vi->vdev, offsetof(struct virtio_input_config,
					      u.string),
			   vi->serial, min(size, sizeof(vi->serial)));

	size = virtinput_cfg_select(vi, VIRTIO_INPUT_CFG_ID_DEVIDS, 0);
	if (size >= sizeof(struct virtio_input_devids)) {
		virtio_cread_le(vi->vdev, struct virtio_input_config,
				u.ids.bustype, &vi->idev->id.bustype);
		virtio_cread_le(vi->vdev, struct virtio_input_config,
				u.ids.vendor, &vi->idev->id.vendor);
		virtio_cread_le(vi->vdev, struct virtio_input_config,
				u.ids.product, &vi->idev->id.product);
		virtio_cread_le(vi->vdev, struct virtio_input_config,
				u.ids.version, &vi->idev->id.version);
	} else {
		vi->idev->id.bustype = BUS_VIRTUAL;
	}

	virtinput_cfg_bits(vi, VIRTIO_INPUT_CFG_PROP_BITS, 0,
			   vi->idev->propbit, INPUT_PROP_CNT);
	size = virtinput_cfg_select(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_REP);
	if (size)
		__set_bit(EV_REP, vi->idev->evbit);

	/* device -> kernel */
	virtinput_cfg_bits(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_KEY,
			   vi->idev->keybit, KEY_CNT);
	virtinput_cfg_bits(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_REL,
			   vi->idev->relbit, REL_CNT);
	virtinput_cfg_bits(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_ABS,
			   vi->idev->absbit, ABS_CNT);
	virtinput_cfg_bits(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_MSC,
			   vi->idev->mscbit, MSC_CNT);
	virtinput_cfg_bits(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_SW,
			   vi->idev->swbit,  SW_CNT);

	/* kernel -> device */
	virtinput_cfg_bits(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_LED,
			   vi->idev->ledbit, LED_CNT);
	virtinput_cfg_bits(vi, VIRTIO_INPUT_CFG_EV_BITS, EV_SND,
			   vi->idev->sndbit, SND_CNT);

	if (test_bit(EV_ABS, vi->idev->evbit)) {
		for (abs = 0; abs < ABS_CNT; abs++) {
			if (!test_bit(abs, vi->idev->absbit))
				continue;
			virtinput_cfg_abs(vi, abs);
		}
	}
}

static void virtinput_get_flat_config(struct virtio_input *vi)
{
	u32 sum_size;
	u8 size[VIRTIO_INPUT_FLAT_CFG_MAX];
	u16 offset[VIRTIO_INPUT_FLAT_CFG_MAX];
	int abs;
	u8 index;
	unsigned int head_size = sizeof(sum_size) + sizeof(size) + sizeof(offset);
	unsigned int cell_offset;

	virtio_cread_bytes(vi->vdev,
			offsetof(struct virtio_input_flat_config, sum_size),
			&sum_size, sizeof(size));
	pr_info("The total input config data size is %d bytes.\n", sum_size);
	virtio_cread_bytes(vi->vdev,
			offsetof(struct virtio_input_flat_config, size),
			size, sizeof(size));
	virtio_cread_bytes(vi->vdev,
			offsetof(struct virtio_input_flat_config, offset),
			offset, sizeof(offset));

	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_ID_NAME];
	virtio_cread_bytes(vi->vdev, cell_offset, vi->name,
		min((size_t)size[VIRTIO_INPUT_FLAT_CFG_ID_NAME], sizeof(vi->name)));

	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_ID_SERIAL];
	virtio_cread_bytes(vi->vdev, cell_offset, vi->serial,
		min((size_t)size[VIRTIO_INPUT_FLAT_CFG_ID_SERIAL],
			sizeof(vi->serial)));

	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_ID_DEVIDS];
	virtio_cread_bytes(vi->vdev, cell_offset, &vi->idev->id.bustype,
			sizeof(vi->idev->id.bustype));
	cell_offset += sizeof(vi->idev->id.bustype);
	virtio_cread_bytes(vi->vdev, cell_offset, &vi->idev->id.vendor,
			sizeof(vi->idev->id.vendor));
	cell_offset += sizeof(vi->idev->id.vendor);
	virtio_cread_bytes(vi->vdev, cell_offset, &vi->idev->id.product,
			sizeof(vi->idev->id.product));
	cell_offset += sizeof(vi->idev->id.product);
	virtio_cread_bytes(vi->vdev, cell_offset, &vi->idev->id.version,
			sizeof(vi->idev->id.version));

	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_PROP_BITS];
	virtinput_plat_cfg_bits(vi, size[VIRTIO_INPUT_FLAT_CFG_PROP_BITS],
			cell_offset, 0,
			vi->idev->propbit, INPUT_PROP_CNT, false);

	if (size[VIRTIO_INPUT_FLAT_CFG_EV_REP])
		__set_bit(EV_REP, vi->idev->evbit);

	/* device -> kernel */
	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_EV_KEY];
	virtinput_plat_cfg_bits(vi, size[VIRTIO_INPUT_FLAT_CFG_EV_KEY],
			cell_offset, EV_KEY,
			vi->idev->keybit, KEY_CNT, true);

	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_EV_REL];
	virtinput_plat_cfg_bits(vi, size[VIRTIO_INPUT_FLAT_CFG_EV_REL],
			cell_offset, EV_REL,
			vi->idev->relbit, REL_CNT, true);
	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_EV_ABS];
	virtinput_plat_cfg_bits(vi, size[VIRTIO_INPUT_FLAT_CFG_EV_ABS],
			cell_offset, EV_ABS,
			vi->idev->absbit, ABS_CNT, true);
	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_EV_MSC];
	virtinput_plat_cfg_bits(vi, size[VIRTIO_INPUT_FLAT_CFG_EV_MSC],
			cell_offset, EV_MSC,
			vi->idev->mscbit, MSC_CNT, true);
	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_EV_SW];
	virtinput_plat_cfg_bits(vi, size[VIRTIO_INPUT_FLAT_CFG_EV_SW],
			cell_offset, EV_SW,
			vi->idev->swbit, SW_CNT, true);

	/* kernel -> device */
	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_EV_LED];
	virtinput_plat_cfg_bits(vi, size[VIRTIO_INPUT_FLAT_CFG_EV_LED],
			cell_offset, EV_LED,
			vi->idev->ledbit, LED_CNT, true);
	cell_offset = head_size + offset[VIRTIO_INPUT_FLAT_CFG_EV_SND];
	virtinput_plat_cfg_bits(vi, size[VIRTIO_INPUT_FLAT_CFG_EV_SND],
			cell_offset, EV_SND,
			vi->idev->sndbit, SND_CNT, true);

	if (test_bit(EV_ABS, vi->idev->evbit)) {
		index = VIRTIO_INPUT_FLAT_CFG_ABS;
		for (abs = 0; abs < ABS_CNT; abs++) {
			if (!test_bit(abs, vi->idev->absbit))
				continue;
			index += 1;
			cell_offset = head_size + offset[index];
			virtinput_flat_cfg_abs(vi, cell_offset, abs);
		}
	}
}

static int virtinput_probe(struct virtio_device *vdev)
{
	struct virtio_input *vi;
	unsigned long flags;
	int err, nslots;

	if (!virtio_has_feature(vdev, VIRTIO_F_VERSION_1))
		return -ENODEV;

	vi = kzalloc(sizeof(*vi), GFP_KERNEL);
	if (!vi)
		return -ENOMEM;

	vdev->priv = vi;
	vi->vdev = vdev;
	spin_lock_init(&vi->lock);

	err = virtinput_init_vqs(vi);
	if (err)
		goto err_init_vq;

	vi->idev = input_allocate_device();
	if (!vi->idev) {
		err = -ENOMEM;
		goto err_input_alloc;
	}
	input_set_drvdata(vi->idev, vi);

	if (virtio_has_feature(vdev, VIRTIO_INPUT_F_FLAT_CFG)) {
		virtinput_get_flat_config(vi);
	} else {
		virtinput_get_config(vi);
	}
	snprintf(vi->phys, sizeof(vi->phys), "virtio%d/input0", vdev->index);
	vi->idev->phys = vi->phys;
	vi->idev->name = vi->name;
	vi->idev->uniq = vi->serial;
	vi->idev->dev.parent = &vdev->dev;
	vi->idev->event = virtinput_status;

	if (test_bit(EV_ABS, vi->idev->evbit)) {
		if (test_bit(ABS_MT_SLOT, vi->idev->absbit)) {
			nslots = input_abs_get_max(vi->idev, ABS_MT_SLOT) + 1;
			err = input_mt_init_slots(vi->idev, nslots, 0);
			if (err)
				goto err_mt_init_slots;
		}
	}

	virtio_device_ready(vdev);
	vi->ready = true;
	err = input_register_device(vi->idev);
	if (err)
		goto err_input_register;

	virtinput_fill_evt(vi);
	return 0;

err_input_register:
	spin_lock_irqsave(&vi->lock, flags);
	vi->ready = false;
	spin_unlock_irqrestore(&vi->lock, flags);
err_mt_init_slots:
	input_free_device(vi->idev);
err_input_alloc:
	vdev->config->del_vqs(vdev);
err_init_vq:
	kfree(vi);
	return err;
}

static void virtinput_remove(struct virtio_device *vdev)
{
	struct virtio_input *vi = vdev->priv;
	void *buf;
	unsigned long flags;

	spin_lock_irqsave(&vi->lock, flags);
	vi->ready = false;
	spin_unlock_irqrestore(&vi->lock, flags);

	input_unregister_device(vi->idev);
	vdev->config->reset(vdev);
	while ((buf = virtqueue_detach_unused_buf(vi->sts)) != NULL)
		kfree(buf);
	vdev->config->del_vqs(vdev);
	kfree(vi);
}

#ifdef CONFIG_PM_SLEEP
static int virtinput_freeze(struct virtio_device *vdev)
{
	struct virtio_input *vi = vdev->priv;
	unsigned long flags;

	spin_lock_irqsave(&vi->lock, flags);
	vi->ready = false;
	spin_unlock_irqrestore(&vi->lock, flags);

	vdev->config->del_vqs(vdev);
	return 0;
}

static int virtinput_restore(struct virtio_device *vdev)
{
	struct virtio_input *vi = vdev->priv;
	int err;

	err = virtinput_init_vqs(vi);
	if (err)
		return err;

	virtio_device_ready(vdev);
	vi->ready = true;
	virtinput_fill_evt(vi);
	return 0;
}
#endif

static unsigned int features[] = {
	VIRTIO_INPUT_F_FLAT_CFG,
};
static const struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_INPUT, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_input_driver = {
	.driver.name         = KBUILD_MODNAME,
	.driver.owner        = THIS_MODULE,
	.feature_table       = features,
	.feature_table_size  = ARRAY_SIZE(features),
	.id_table            = id_table,
	.probe               = virtinput_probe,
	.remove              = virtinput_remove,
#ifdef CONFIG_PM_SLEEP
	.freeze	             = virtinput_freeze,
	.restore             = virtinput_restore,
#endif
};

module_virtio_driver(virtio_input_driver);
MODULE_DEVICE_TABLE(virtio, id_table);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtio input device driver");
MODULE_AUTHOR("Gerd Hoffmann <kraxel@redhat.com>");
