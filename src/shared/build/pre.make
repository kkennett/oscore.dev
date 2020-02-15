
#MAKEFLAGS += -s
MAKEFLAGS += --stop

#========================================================================================
# these symbols must be defined before make is invoked
#========================================================================================

ifeq ($(K2_ROOT),)
$(error K2_ROOT not set)
endif

ifeq ($(K2_ARCH),)
$(error error - no arch set. use 'setarch xxx' at command line)
endif
ifneq ($(K2_ARCH),X32)
ifneq ($(K2_ARCH),A32)
$(error K2_ARCH set to invalid/unknown architecture ($(K2_ARCH)))
endif
endif

#========================================================================================
# ALL
#========================================================================================
empty := 
space := $(empty) $(empty)
comma := ,

K2_SUBPATH := $(shell $(K2_ROOT)/k2tools32/k2setsub.bat)
K2_TARGET_NAME := $(notdir $(K2_SUBPATH))
K2_SOURCE_PATH := $(K2_ROOT)/src/$(K2_SUBPATH)
K2_TARGET_BASE ?= $(K2_ROOT)/bld/out/gcc
K2_TEMP_BASE   ?= $(K2_ROOT)/bld/tmp/gcc

GCCOPT_SYSTEM += -D__K2OS__

SOURCES             := 
SOURCE_LIBS         :=
KERN_SOURCE_LIBS    := 
TARGET_LIBS         :=
GCCOPT              := $(GCCOPT_SYSTEM) -fdata-sections --function-sections
GCCOPT_S            :=
GCCOPT_C            :=
GCCOPT_CPP          := -fno-rtti -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables

LDOPT               := --gc-sections 

ifneq ($(K2_MODE),FINAL)
K2_BUILD_SPEC := $(K2_ARCH)/Debug
GCCOPT_DEBUG  := -ggdb -g2
LDOPT         += 
else
K2_BUILD_SPEC := $(K2_ARCH)/NoDebug
LDOPT         += --strip-debug
endif

LIBGCC_PATH := $(K2_TOOLS_BIN_PATH)/libgcc.a

