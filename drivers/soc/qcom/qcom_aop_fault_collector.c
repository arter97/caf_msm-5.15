// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/ipc_logging.h>
#include <linux/irq.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/net.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/qmi.h>

#define QMI_AOP_FC_DRIVER		"aop-fault-collector-driver"
#define QMI_AOP_FC_RESP_TOUT		msecs_to_jiffies(100)
#define FM_REPORT_TO_FM_RESP_MSG_V01_MAX_MSG_LEN	11
#define QMI_FM_TO_FM_REPORT_REQ_V01			0x0007
#define PAYLOAD_MAX_LEN				24
#define MAX_NUM_FAULT_DATA			20
#define FAULT_MANAGER_SERVICE_ID_V01		0x43B
#define FAULT_MANAGER_SERVICE_VERS_V01		0x01
#define QMI_INSTANSE_ID			0
#define PAYLOAD_VALID			1
#define IPC_LOGPAGES			10
#define FM_REPORT_TO_FM_REQ_MSG_V01_MAX_MSG_LEN	1070

#define AOP_FC_DBG(dev, msg, args...) do {		\
		pr_debug("%s:" msg, __func__, args);	\
		if ((dev) && (dev)->ipc_log) {		\
			ipc_log_string((dev)->ipc_log,	\
			"%s: " msg " [%s]\n",		\
			__func__, args, current->comm);	\
		}					\
	} while (0)

struct aop_fault_data {
	uint32_t	module_id;
	uint32_t	fault_id;
	uint64_t	fault_begin_time;
	uint32_t	severity_level;
	uint32_t	is_fault_active;
	char	payload[PAYLOAD_MAX_LEN];
};

struct qmi_aop_fc_instance {
	struct	device *dev;
	struct	qmi_handle handle;
	struct	mutex lock;
	void	__iomem *msgram;
	struct	mbox_client mbox_client;
	struct	mbox_chan *mbox_chan;
	int		irq_num;
	void	*ipc_log;
	bool	connection_active;
	uint32_t	fault_count;
	struct	aop_fault_data aop_fc_buf[MAX_NUM_FAULT_DATA];
};

enum fm_subsystem_type_enum_t_v01 {
	FM_SUBSYSTEM_TYPE_ENUM_T_MIN_VAL_V01 = INT_MIN,
	AOP_V01			= 2,
	FM_SUBSYSTEM_TYPE_ENUM_T_MAX_VAL_V01 = INT_MAX,
};

struct fm_report_to_fm_resp_msg_v01 {
	struct	qmi_response_type_v01 resp;
	uint8_t	received_payload_valid;
	uint8_t	received_payload;
};

enum fm_source_type_enum_t_v01 {
	FM_SOURCE_TYPE_ENUM_T_MIN_VAL_V01 = INT_MIN,
	APPS_FAULT_MGR_MPLANE_V01	= 0,
	APPS_FAULT_MGR_THERM_V01	= 1,
	APPS_FAULT_MGR_SPLANE_V01	= 2,
	APPS_FAULT_MGR_SOURCE_MAX_V01	= 3,
	AOP_FAULT_MGR_PMIC_V01	= 0,
	AOP_FAULT_MGR_SOURCE_MAX_V01	= 1,
	FM_SOURCE_TYPE_ENUM_T_MAX_VAL_V01	= INT_MAX,
};

enum fm_fault_severity_type_enum_t_v01 {
	FM_FAULT_SEVERITY_TYPE_ENUM_T_MIN_VAL_V01	= INT_MIN,
	FM_ALARM_SEVERITY_CRITICAL_V01	= 0,
	FM_ALARM_SEVERITY_MAJOR_V01		= 1,
	FM_ALARM_SEVERITY_MINOR_V01		= 2,
	FM_ALARM_SEVERITY_WARNING_V01	= 3,
	FM_FAULT_SEVERITY_TYPE_ENUM_T_MAX_VAL_V01 = INT_MAX,
};

struct fm_report_to_fm_req_msg_v01 {
	uint32_t	internal_fault_id;
	enum	fm_subsystem_type_enum_t_v01 subsystem;
	enum	fm_source_type_enum_t_v01 source;
	enum	fm_fault_severity_type_enum_t_v01 fault_severity;
	uint8_t	is_active;
	uint64_t	event_time;
	uint8_t	payload_valid;
	char	payload[1024];
};

struct qmi_elem_info fm_report_to_fm_resp_msg_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
					   fm_report_to_fm_resp_msg_v01,
					   resp),
		.ei_array	= qmi_response_type_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   fm_report_to_fm_resp_msg_v01,
					   received_payload_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   fm_report_to_fm_resp_msg_v01,
					   received_payload),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};
struct qmi_elem_info fm_report_to_fm_req_msg_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(struct
					   fm_report_to_fm_req_msg_v01,
					   internal_fault_id),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum fm_subsystem_type_enum_t_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
					   fm_report_to_fm_req_msg_v01,
					   subsystem),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum fm_source_type_enum_t_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x03,
		.offset		= offsetof(struct
					   fm_report_to_fm_req_msg_v01,
					   source),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum fm_fault_severity_type_enum_t_v01),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x04,
		.offset		= offsetof(struct
					   fm_report_to_fm_req_msg_v01,
					   fault_severity),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x05,
		.offset		= offsetof(struct
					   fm_report_to_fm_req_msg_v01,
					   is_active),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u64),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x06,
		.offset		= offsetof(struct
					   fm_report_to_fm_req_msg_v01,
					   event_time),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(u8),
		.array_type	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   fm_report_to_fm_req_msg_v01,
					   payload_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1024,
		.elem_size	= sizeof(char),
		.array_type	= STATIC_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(struct
					   fm_report_to_fm_req_msg_v01,
					   payload),
	},
	{
		.data_type	= QMI_EOTI,
		.array_type	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static void aop_fc_ack(struct mbox_chan *mbox_chan)
{
	mbox_send_message(mbox_chan, NULL);
	mbox_client_txdone(mbox_chan, 0);
}

static int send_fault_details_qmi(struct qmi_aop_fc_instance *aop_fc)
{
	struct fm_report_to_fm_req_msg_v01 req;
	struct fm_report_to_fm_resp_msg_v01 aop_fc_resp;
	struct qmi_txn txn;
	int ret = 0, idx = 0;

	memset(&req, 0, sizeof(req));
	memset(&aop_fc_resp, 0, sizeof(aop_fc_resp));

	if (!aop_fc->connection_active) {
		pr_err("QMI server is not connected\n");
		return -ENODEV;
	}

	mutex_lock(&aop_fc->lock);
	for (idx = 0; idx < aop_fc->fault_count; idx++) {
		req.internal_fault_id = aop_fc->aop_fc_buf[idx].fault_id;
		req.subsystem = AOP_V01;
		req.is_active = aop_fc->aop_fc_buf[idx].is_fault_active;
		req.event_time = aop_fc->aop_fc_buf[idx].fault_begin_time;
		req.source = aop_fc->aop_fc_buf[idx].module_id;
		req.payload_valid = PAYLOAD_VALID;
		memcpy(req.payload, aop_fc->aop_fc_buf[idx].payload,
				sizeof(aop_fc->aop_fc_buf[idx].payload));

		ret = qmi_txn_init(&aop_fc->handle, &txn,
				fm_report_to_fm_resp_msg_v01_ei, &aop_fc_resp);
		if (ret < 0) {
			pr_err("Transaction Init error for inst_id:0x%x ret:%d\n",
					QMI_INSTANSE_ID, ret);
			goto reg_exit;
		}

		ret = qmi_send_request(&aop_fc->handle, NULL, &txn,
				QMI_FM_TO_FM_REPORT_REQ_V01,
				FM_REPORT_TO_FM_REQ_MSG_V01_MAX_MSG_LEN,
				fm_report_to_fm_req_msg_v01_ei,
				&req);

		if (ret < 0) {
			qmi_txn_cancel(&txn);
			goto reg_exit;
		}

		ret = qmi_txn_wait(&txn, QMI_AOP_FC_RESP_TOUT);
		if (ret < 0) {
			pr_err("Transaction wait error for inst_id:0x%x ret:%d\n",
					QMI_INSTANSE_ID, ret);
			goto reg_exit;
		}

		if (aop_fc_resp.resp.result != QMI_RESULT_SUCCESS_V01) {
			ret = aop_fc_resp.resp.result;
			pr_err("Get AOP fault details NOT success for inst_id:0x%x ret:%d\n",
					QMI_INSTANSE_ID, ret);
			goto  reg_exit;
		}

		AOP_FC_DBG(aop_fc,
				"%s: fault_id: %d is_fault_active: %d fault_begin_time: %lld\n",
				__func__,
				aop_fc->aop_fc_buf[idx].fault_id,
				aop_fc->aop_fc_buf[idx].is_fault_active,
				aop_fc->aop_fc_buf[idx].fault_begin_time);
	}

reg_exit:
	mutex_unlock(&aop_fc->lock);

	return ret;
}

static irqreturn_t aop_fc_intr(int irq, void *data)
{
	struct qmi_aop_fc_instance *aop_fc = data;
	uint32_t count = 0, len;

	count = readl_relaxed(aop_fc->msgram);
	AOP_FC_DBG(aop_fc, "%s count : %d\n", __func__, count);
	aop_fc->fault_count = count;
	len = count * sizeof(struct aop_fault_data);

	if (count) {
		__ioread32_copy(aop_fc->aop_fc_buf,
			aop_fc->msgram + sizeof(uint64_t),
			len / sizeof(u32));

		send_fault_details_qmi(aop_fc);
	}

	aop_fc_ack(aop_fc->mbox_chan);

	return IRQ_HANDLED;
}

static void aop_fc_qmi_del_server(struct qmi_handle *qmi,
				struct qmi_service *service)
{
	struct qmi_aop_fc_instance *aop_fc = container_of(qmi,
						struct qmi_aop_fc_instance,
						handle);

	aop_fc->connection_active = false;
	AOP_FC_DBG(aop_fc, "%s: is_fault_active: %d\n", __func__,
			aop_fc->connection_active);
}

static int aop_fc_qmi_new_server(struct qmi_handle *qmi,
				struct qmi_service *service)
{
	struct qmi_aop_fc_instance *aop_fc = container_of(qmi,
						struct qmi_aop_fc_instance,
						handle);
	struct sockaddr_qrtr sq = {AF_QIPCRTR, service->node, service->port};

	mutex_lock(&aop_fc->lock);
	kernel_connect(qmi->sock, (struct sockaddr *)&sq, sizeof(sq), 0);
	mutex_unlock(&aop_fc->lock);
	aop_fc->connection_active = true;
	enable_irq(aop_fc->irq_num);
	enable_irq_wake(aop_fc->irq_num);
	aop_fc_ack(aop_fc->mbox_chan);
	AOP_FC_DBG(aop_fc, "%s: is_fault_active: %d\n", __func__, aop_fc->connection_active);

	return 0;
}

static struct qmi_ops aop_fc_qmi_event_ops = {
	.new_server = aop_fc_qmi_new_server,
	.del_server = aop_fc_qmi_del_server,
};

static int qcom_aop_fc_remove(struct platform_device *pdev)
{
	struct qmi_aop_fc_instance *aop_fc = platform_get_drvdata(pdev);

	aop_fc->connection_active = false;
	if (aop_fc->ipc_log)
		ipc_log_context_destroy(aop_fc->ipc_log);
	if (aop_fc->mbox_chan)
		mbox_free_channel(aop_fc->mbox_chan);
	qmi_handle_release(&aop_fc->handle);

	return 0;
}

static int qmi_aop_fc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct qmi_aop_fc_instance *aop_fc;
	int ret = 0, irq;

	aop_fc = devm_kzalloc(&pdev->dev, sizeof(*aop_fc), GFP_KERNEL);
	if (!aop_fc)
		return -ENOMEM;

	aop_fc->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	aop_fc->msgram = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(aop_fc->msgram))
		return PTR_ERR(aop_fc->msgram);

	mutex_init(&aop_fc->lock);
	aop_fc->mbox_client.dev = &pdev->dev;
	aop_fc->mbox_client.knows_txdone = true;
	aop_fc->mbox_chan = mbox_request_channel(&aop_fc->mbox_client, 0);
	if (IS_ERR(aop_fc->mbox_chan)) {
		dev_err(&pdev->dev, "failed to acquire ipc mailbox\n");
		return PTR_ERR(aop_fc->mbox_chan);
	}

	irq = platform_get_irq(pdev, 0);
	aop_fc->irq_num = irq;
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL, aop_fc_intr, IRQF_ONESHOT,
							"aop-fc", aop_fc);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request interrupt\n");
		goto probe_exit;
	}

	aop_fc->ipc_log = ipc_log_context_create(IPC_LOGPAGES, "aop-fc", 0);
	disable_irq_nosync(aop_fc->irq_num);

	ret = qmi_handle_init(&aop_fc->handle,
					FM_REPORT_TO_FM_RESP_MSG_V01_MAX_MSG_LEN,
					&aop_fc_qmi_event_ops, NULL);
	if (ret < 0) {
		dev_err(dev, "QMI handle init failed. err:%d\n", ret);
		goto probe_exit;
	}

	ret = qmi_add_lookup(&aop_fc->handle, FAULT_MANAGER_SERVICE_ID_V01,
				FAULT_MANAGER_SERVICE_VERS_V01,
				QMI_INSTANSE_ID);
	if (ret < 0) {
		dev_err(dev, "QMI register failed for 0x%x, ret:%d\n",
				QMI_INSTANSE_ID, ret);
		goto probe_exit;
	}

	return 0;

probe_exit:
	qcom_aop_fc_remove(pdev);

	return ret;
}

static const struct of_device_id qmi_aop_fc_device_match[] = {
	{.compatible = "qcom,aop-fc"},
	{}
};

static struct platform_driver qmi_aop_fc_driver = {
	.probe          = qmi_aop_fc_probe,
	.driver         = {
		.name   = QMI_AOP_FC_DRIVER,
		.of_match_table = qmi_aop_fc_device_match,
	},
	.remove = qcom_aop_fc_remove,
};

module_platform_driver(qmi_aop_fc_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("QCOM AOP Fault Collector driver");
