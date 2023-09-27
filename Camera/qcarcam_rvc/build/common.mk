# This is an automatically generated record.
# The area between QNX Internal Start and QNX Internal End is controlled by
# the QNX IDE properties.

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

include $(AMSS_ROOT)/amss_defs.mk

include ../../../../../build/qnx/overrides.mk

NAME=qcarcam_rvc

#===== INSTALLDIR - Subdirectory where the executable or library is to be installed.
INSTALLDIR=$(CAMERA_OUT_BIN)/$(NAME)

ifeq ($(CPULIST),aarch64)
cpulist=aarch64le
else
cpulist=armle-v7
endif

#===== USEFILE - the file containing the usage message for the application. 
USEFILE=$(PROJECT_ROOT)/../src/qcarcam_rvc.use

#===== PINFO - the file containing the packaging information for the application. 
define PINFO
PINFO DESCRIPTION=QCarCam RVC test application
endef

#===== EXTRA_SRCVPATH - a space-separated list of directories to search for source files.
EXTRA_SRCVPATH+= \
        $(PROJECT_ROOT)/../src \
        $(PROJECT_ROOT)/../../test_util/src \
        $(PROJECT_ROOT)/../../test_util/src/qnx

#===== EXTRA_INCVPATH - a space-separated list of directories to search for include files.
EXTRA_INCVPATH+= \
        $(QCX_ROOT)/utils/inc \
        $(QCX_ROOT)/utils/inc/os \
        $(PROJECT_ROOT)/../../test_util/inc \
        $(AMSS_INC) \
        $(MULTIMEDIA_INC) \
        $(INSTALL_ROOT_nto)/usr/include/amss \
        $(INSTALL_ROOT_nto)/usr/include/amss/core \
        $(CAMERA_OUT_HEADERS) \
        $(INSTALL_ROOT_nto)/usr/include/amss/multimedia/display \
        $(AMSS_ROOT)/multimedia/graphics/include/private/C2D \
        $(AMSS_ROOT)/multimedia/graphics/adreno/include/private/C2D \
        $(CAMERA_ROOT)/ext/system

#===== EXTRA_LIBVPATH - a space-separated list of directories to search for library files.
EXTRA_LIBVPATH+= $(INCLUDE_LIB_ASIC_8996) \
        $(INSTALL_ROOT_nto)/$(cpulist)/$(INSTALLDIR_GFX_QC_BASE) \
        $(INSTALL_ROOT_nto)/$(cpulist)/$(CAMERA_OUT_LIB)

#===== LIBS - a space-separated list of library items to be included in the link.
LIBS+= qcxclient qcxosal camera_metadata \
		OSAbstraction screen xml2 pmem_client fdt_utils gpio_client slog2
#LIBS+= c2d30

#===== VERSION_TAG_SO - version tag for SONAME. Use it only if you don't like SONAME_VERSION
override VERSION_TAG_SO=

#===== CCFLAGS - add the flags to the C compiler command line.
CCFLAGS += \
        -Werror \
        -DC2D_DISABLED

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
