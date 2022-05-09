ECPRIDMADRVTOP = $(srctree)/drivers/platform/msm/ecpri/dma/drivers

ccflags-y += -I$(ECPRIDMADRVTOP)/../../../include
ccflags-y += -I$(ECPRIDMADRVTOP)/gsi
ccflags-y += -I$(ECPRIDMADRVTOP)/gsi/gsihal
ccflags-y += -I$(ECPRIDMADRVTOP)/ecpri_dma
ccflags-y += -I$(ECPRIDMADRVTOP)/ecpri_dma/dump
ccflags-y += -I$(ECPRIDMADRVTOP)/ecpri_dma/dmahal
ccflags-y += -I$(ECPRIDMADRVTOP)/ecpri_dma/dma_clients

gsim-y +=	drivers/gsi/gsi.o \
			drivers/gsi/gsihal/gsihal.o \
			drivers/gsi/gsihal/gsihal_reg.o

gsim-$(CONFIG_DEBUG_FS) += drivers/gsi/gsi_dbg.o

ecpri_dmam-y += \
	drivers/ecpri_dma/ecpri_dma.o \
	drivers/ecpri_dma/ecpri_dma_utils.o \
	drivers/ecpri_dma/ecpri_dma_interrupts.o \
	drivers/ecpri_dma/ecpri_dma_debugfs.o \
	drivers/ecpri_dma/ecpri_dma_dp.o \
	drivers/ecpri_dma/dump/ecpri_dma_reg_dump.o \
	drivers/ecpri_dma/dma_clients/ecpri_dma_eth_client.o \
	drivers/ecpri_dma/dma_clients/ecpri_dma_ss_core.o \
	drivers/ecpri_dma/dma_clients/ecpri_dma_mhi_client.o \
	drivers/ecpri_dma/dmahal/dmahal.o \
	drivers/ecpri_dma/dmahal/dmahal_reg.o

ecpri_dmam-$(CONFIG_ECPRI_DMA_UT) += \
	drivers/ecpri_dma/test/ecpri_dma_test_dp.o \
	drivers/ecpri_dma/test/ecpri_dma_eth_dp_test.o \
	drivers/ecpri_dma/test/ecpri_dma_ss_core_test.o \
	drivers/ecpri_dma/test/ecpri_dma_mhi_client_test.o \
	drivers/ecpri_dma/test/ecpri_dma_test_driver_init.o \
	drivers/ecpri_dma/test/ecpri_dma_test_example.o \
	drivers/ecpri_dma/test/ecpri_dma_ut_framework.o

obj-$(CONFIG_ECPRI_GSI) += gsim.o
obj-$(CONFIG_ECPRI_DMA) += ecpri_dmam.o

all:
	make -C $(KERNEL_SRC)/include/linux M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC)/build M=$(PWD) clean
