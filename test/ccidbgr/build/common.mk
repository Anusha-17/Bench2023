# This is an automatically generated record.
# The area between QNX Internal Start and QNX Internal End is controlled by
# the QNX IDE properties.

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

include $(AMSS_ROOT)/amss_defs.mk

include ../../../../../build/qnx/overrides.mk

NAME=ccidbgr

#===== INSTALLDIR - Subdirectory where the executable or library is to be installed.
INSTALLDIR=$(CAMERA_OUT_BIN)

ifeq ($(CPULIST),aarch64)
cpulist=aarch64le
else
cpulist=armle-v7
endif

#===== USEFILE - the file containing the usage message for the application.
USEFILE=$(PROJECT_ROOT)/../src/qcx_ccidbgr.use

#===== PINFO - the file containing the packaging information for the application. 
define PINFO
PINFO DESCRIPTION="Qualcomm Camera CCI Debugger"
endef

#===== EXTRA_SRCVPATH - a space-separated list of directories to search for source files.
EXTRA_SRCVPATH+= \
	 $(PROJECT_ROOT)/../../ccidbgr/src

#===== EXTRA_INCVPATH - a space-separated list of directories to search for include files.
EXTRA_INCVPATH+= \
	. \
	../inc \
	$(CDK_PATH)/api/qcarcam \
	$(QCX_ROOT)/platformmanager/api \
	$(QCX_ROOT)/platformmanager/inc \
	$(QCX_ROOT)/utils/inc/os \
	$(QCX_ROOT)/utils/inc \
	$(QCX_ROOT)/utils/inc/queue

#===== EXTRA_LIBVPATH - a space-separated list of directories to search for library files.
EXTRA_LIBVPATH+=$(CAMERA_INCLUDE_LIB_PATH)

#===== LIBS - a space-separated list of library items to be included in the link.
LIBS+= slog2 OSAbstraction aosal qcxplatform qcxosal

#===== CCFLAGS - add the flags to the C compiler command line.
CCFLAGS += \
	-Werror

#===== CXXFLAGS - add the flags to the C++ compiler command line.
CXXFLAGS += $(CCFLAGS)

include $(MKFILES_ROOT)/qmacros.mk
ifndef QNX_INTERNAL
QNX_INTERNAL=$(BSP_ROOT)/.qnx_internal.mk
endif
include $(QNX_INTERNAL)

include $(MKFILES_ROOT)/qtargets.mk

OPTIMIZE_TYPE_g=none
OPTIMIZE_TYPE=$(OPTIMIZE_TYPE_$(filter g, $(VARIANTS)))

