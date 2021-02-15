#   
#   BSD 3-Clause License
#   
#   Copyright (c) 2020, Kurt Kennett
#   All rights reserved.
#   
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are met:
#   
#   1. Redistributions of source code must retain the above copyright notice, this
#      list of conditions and the following disclaimer.
#   
#   2. Redistributions in binary form must reproduce the above copyright notice,
#      this list of conditions and the following disclaimer in the documentation
#      and/or other materials provided with the distribution.
#   
#   3. Neither the name of the copyright holder nor the names of its
#      contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.
#   
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#========================================================================================
# TARGET_TYPE == IMAGE
#========================================================================================
ifeq ($(TARGET_TYPE),IMAGE)

K2_TARGET_NAME_SPEC := builtin.img
K2_TARGET_OUT_PATH := $(K2_TARGET_PATH)/$(K2_SUBPATH)

K2_TARGET_DISK_PATH := $(K2_TARGET_OUT_PATH)/bootdisk
K2_TARGET_BUILTIN_PATH := $(K2_TARGET_OUT_PATH)/builtin
K2_TARGET_BUILTIN_KERN_PATH := $(K2_TARGET_BUILTIN_PATH)/kern

K2_TARGET_EFI_PATH := $(K2_TARGET_DISK_PATH)/EFI/BOOT
K2_TARGET_OS_PATH := $(K2_TARGET_DISK_PATH)/K2OS
K2_TARGET_OS_KERN_PATH := $(K2_TARGET_OS_PATH)/$(K2_ARCH)/KERN

K2_TARGET_FULL_SPEC := $(K2_TARGET_OS_KERN_PATH)/$(K2_TARGET_NAME_SPEC)

default: $(K2_TARGET_FULL_SPEC)

#========================================================================================

STOCK_IMAGE_KERN_DLX := @$(K2_OS)/crt/crtkern/$(K2_ARCH)/k2oscrt
STOCK_IMAGE_KERN_DLX += @$(K2_OS)/kern/$(K2_ARCH)/k2oskern 
STOCK_IMAGE_KERN_DLX += @$(K2_OS)/kern/k2osacpi 
STOCK_IMAGE_KERN_DLX += @$(K2_OS)/kern/k2osexec 

ONE_K2_STOCK_KERNEL_DLX = stock_$(basename $(1))
EXPAND_ONE_STOCK_KERNEL_DLX = $(if $(findstring @,$(dlxdep)), $(call ONE_K2_STOCK_KERNEL_DLX,$(subst @,,$(dlxdep))),$(dlxdep))
BUILT_IMAGE_HAL_DLX = $(foreach dlxdep, $(firstword $(IMAGE_HAL_DLX)), $(EXPAND_ONE_STOCK_KERNEL_DLX))
BUILT_STOCK_IMAGE_KERN_DLX = $(foreach dlxdep, $(STOCK_IMAGE_KERN_DLX), $(EXPAND_ONE_STOCK_KERNEL_DLX))

$(BUILT_IMAGE_HAL_DLX) $(BUILT_STOCK_IMAGE_KERN_DLX):
	@MAKE -S -C $(K2_ROOT)/src/$(subst stock_,,$@)
	@-if not exist $(subst /,\,$(K2_TARGET_OS_KERN_PATH)) md $(subst /,\,$(K2_TARGET_OS_KERN_PATH))
	@copy /Y $(subst /,\,$(K2_TARGET_BASE)/dlx/kern/$(K2_BUILD_SPEC)/$(@F).dlx) $(subst /,\,$(K2_TARGET_OS_KERN_PATH)) 1>NUL
	@echo.

CHECK_REF_ONE_K2_HAL = checkhal_$(1)
CHECK_HAL_DLX = $(if $(findstring @,$(haldlx)), $(call CHECK_REF_ONE_K2_HAL,$(subst @,,$(haldlx))),$(haldlx))
CHECK_HAL = $(foreach haldlx, $(firstword $(IMAGE_HAL_DLX)), $(CHECK_HAL_DLX))
$(CHECK_HAL):
	@$(foreach wrong_thing, $(subst k2oshal,,$(@F)), $(error IMAGE_HAL_DLX must end in DLX named 'k2oshal'))

#========================================================================================

ONE_K2_BULITIN_KERNEL_DLX = builtin_$(basename $(1))
EXPAND_ONE_BUILTIN_KERNEL_DLX = $(if $(findstring @,$(dlxdep)), $(call ONE_K2_BULITIN_KERNEL_DLX,$(subst @,,$(dlxdep))),$(dlxdep))
BUILTIN_KERNEL_DRIVER_DLX = $(foreach dlxdep, $(BUILTIN_KERNEL_DRIVERS), $(EXPAND_ONE_BUILTIN_KERNEL_DLX))

$(BUILTIN_KERNEL_DRIVER_DLX):
	@-if not exist $(subst /,\,$(K2_TARGET_BUILTIN_KERN_PATH)) md $(subst /,\,$(K2_TARGET_BUILTIN_KERN_PATH))
	@MAKE -S -C $(K2_ROOT)/src/$(subst builtin_,,$@)
	@copy /Y $(subst /,\,$(K2_TARGET_BASE)/dlx/kern/$(K2_BUILD_SPEC)/$(@F).dlx) $(subst /,\,$(K2_TARGET_BUILTIN_KERN_PATH)) 1>NUL
	@echo.

#========================================================================================

$(K2_TARGET_FULL_SPEC): $(CHECK_HAL) $(BUILTIN_KERNEL_DRIVER_DLX) $(BUILT_IMAGE_HAL_DLX) $(BUILT_STOCK_IMAGE_KERN_DLX) $(BUILD_CONTROL_FILES)
	@-if not exist $(subst /,\,$(K2_TARGET_OUT_PATH)) md $(subst /,\,$(K2_TARGET_OUT_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_DISK_PATH)) md $(subst /,\,$(K2_TARGET_DISK_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_BUILTIN_PATH)) md $(subst /,\,$(K2_TARGET_BUILTIN_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_EFI_PATH)) md $(subst /,\,$(K2_TARGET_EFI_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_OS_KERN_PATH)) md $(subst /,\,$(K2_TARGET_OS_KERN_PATH))
	@echo -------- Creating IMAGE $@ --------
	@copy /Y $(subst /,\,$(K2_ROOT)/src/$(K2_OS)/boot/*) $(subst /,\,$(K2_TARGET_EFI_PATH)) 1>NUL
	@k2zipper $(subst /,\,$(K2_TARGET_BUILTIN_PATH)) $(subst /,\,$@)

endif

