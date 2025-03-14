# Makefile for the bcm963xx board drivers
#


obj-y := board.o bcm63xx_flash.o bcm63xx_led.o bcm63xx_gpio.o pushbutton.o

ifneq ($(strip $(BUILD_IEEE1905)),)
obj-y += ieee1905_module.o    
endif

ifneq ($(strip $(SUPPORT_GPL_1)),)
obj-y += aei_bcm63xx_flash.o
endif

ifeq ($(strip $(BRCM_CHIP)),6816)
obj-y += spidevices.o
endif

ifneq ($(findstring _$(strip $(BRCM_CHIP))_,,_63268_63138_63148_6838_),)
ifneq ($(strip $(BUILD_6802_MOCA)),)
    obj-y += bbsi/bbsi.o    
    obj-y += bbsi/bbsiExecSeq.o   
    obj-y += moca/6802/init.o   
    
else
    obj-y += spidevices.o
endif

endif

ifneq ($(strip $(CONFIG_BCM_CPLD1)),)
    obj-y += bcmcpld1.o
	EXTRA_CFLAGS += -DCONFIG_BCM_CPLD1
endif

ifneq ($(strip $(CONFIG_BCM_PLC_BOOT)),)
    obj-y += homeplugd_module.o
endif

ifneq ($(strip $(subst ", ,$(BUILD_BRCM_XDSL_DISTPOINT))),)
    obj-y += dsldsp_operation.o
    obj-y += xdsl_distpoint.o
	EXTRA_CFLAGS += -DBRCM_XDSL_DISTPOINT
endif
ifneq ($(strip $(BUILD_BRCM_FTTDP)),)
	EXTRA_CFLAGS += -DBRCM_FTTDP
endif

ifeq ($(BUILD_IEEE1905),y)
	EXTRA_CFLAGS += -DSUPPORT_IEEE1905
endif
 
ifeq ($(strip $(BRCM_CHIP)),63268)
ifneq ($(strip $(BUILD_SECURE_BOOT)),)
ifeq ($(strip $(BUILD_NVRAM_3K)),y)
	EXTRA_CFLAGS += -DINC_BTRM_BOOT=1
endif
endif
endif
 
EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
ifneq ($(findstring _$(strip $(BRCM_CHIP))_,,_63138_63148_6838_63381_),)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PUB_PATH)/pmc
endif
ifneq ($(strip $(SUPPORT_GPL_1)),)
EXTRA_CFLAGS += $(ACTION_TEC_DFLAGS)
else
ifneq ($(strip $(AEI_VDSL_BCMSDK)),)
EXTRA_CFLAGS += $(ACTION_TEC_DFLAGS)
else
EXTRA_CFLAGS += -Werror -Wfatal-errors
endif
endif

EXTRA_CFLAGS += -g

ifneq ($(strip $(CONFIG_BCM_WLAN)),)
	EXTRA_CFLAGS += -DWIRELESS
endif

-include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s

