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

ifneq ($(TARGET_SPECIFIC_ARCH),)
ifneq ($(K2_ARCH),$(TARGET_SPECIFIC_ARCH))
$(error This component for $(TARGET_SPECIFIC_ARCH) only)
endif
endif

#========================================================================================
# CHECK TARGET TYPES AND FORM TARGET PATH
#========================================================================================

ifeq ($(TARGET_TYPE),OBJ)
K2_TARGET_PATH := $(K2_TARGET_BASE)/obj/$(K2_BUILD_SPEC)/$(K2_SUBPATH)
else
ifeq ($(TARGET_TYPE),LIB)
K2_TARGET_PATH := $(K2_TARGET_BASE)/srclib/$(K2_BUILD_SPEC)
else
ifeq ($(TARGET_TYPE),DLX)
K2_TARGET_PATH := $(K2_TARGET_BASE)/dlx/$(K2_BUILD_SPEC)
else
ifeq ($(TARGET_TYPE),IMAGE)
K2_TARGET_PATH := $(K2_TARGET_BASE)/image/$(K2_BUILD_SPEC)
else
K2_TARGET_PATH := $(K2_TARGET_BASE)/elf/$(K2_BUILD_SPEC)/$(K2_SUBPATH)
endif
endif
endif
endif

K2_OBJECT_PATH := $(K2_TEMP_BASE)/obj/$(K2_BUILD_SPEC)/$(K2_SUBPATH)

ifneq ($(K2_KERNEL),)
K2_TARGET_PATH := $(K2_TARGET_PATH)/kern
K2_SPEC_KERNEL := -k
else
K2_SPEC_KERNEL := 
endif

.PHONY: default clean

#========================================================================================
# SOURCE LIBRARIES, DLX
#========================================================================================

OBJECT_FROM_SOURCE = $(K2_OBJECT_PATH)/$(addsuffix .o, $(basename $(source)))
OBJECTS = $(foreach source, $(SOURCES), $(OBJECT_FROM_SOURCE))
ONE_OBJECT = $(foreach source, $(firstword $(SOURCES)), $(OBJECT_FROM_SOURCE))

REF_ONE_K2_SRC_LIB = $(K2_TARGET_BASE)/srclib/$(K2_BUILD_SPEC)/$(notdir $(1)).lib
REF_ONE_SRC_LIB = $(if $(findstring @,$(libdep)), $(call REF_ONE_K2_SRC_LIB,$(subst @,,$(libdep))),$(libdep))

REFSRC_LIBS = $(foreach libdep,$(SOURCE_LIBS),$(REF_ONE_SRC_LIB))
REFSRCDLX_LIBS = $(foreach libdep,$(SOURCE_DLX),$(REF_ONE_SRC_LIB))

REF_ONE_K2_KERN_SRC_LIB = $(K2_TARGET_BASE)/srclib/$(K2_BUILD_SPEC)/kern/$(notdir $(1)).lib
REF_ONE_KERN_SRC_LIB = $(if $(findstring @,$(libdep)), $(call REF_ONE_K2_KERN_SRC_LIB,$(subst @,,$(libdep))),$(libdep))

REFKRNSRC_LIBS = $(foreach libdep,$(KERN_SOURCE_LIBS),$(REF_ONE_KERN_SRC_LIB))
REFKRNSRCDLX_LIBS = $(foreach libdep,$(KERN_SOURCE_DLX),$(REF_ONE_KERN_SRC_LIB))

#========================================================================================
# TARGET LIBRARIES, DLX
#========================================================================================

REF_ONE_K2_TRG_LIB = $(K2_TARGET_PRE)/lib/$(K2_BUILD_SPEC)/$(notdir $(1)).lib
REF_ONE_TRG_LIB = $(if $(findstring @,$(libdep)), $(call REF_ONE_K2_TRG_LIB,$(subst @,,$(libdep))),$(libdep))

REFTRG_LIBS = $(foreach libdep,$(TARGET_LIBS),$(REF_ONE_TRG_LIB))
REFTRGDLX_LIBS = $(foreach libdep,$(TARGET_DLX),$(REF_ONE_TRG_LIB))

REF_ONE_K2_KERN_TRG_LIB = $(K2_TARGET_PRE)/lib/$(K2_BUILD_SPEC)/kern/$(notdir $(1)).lib
REF_ONE_KERN_TRG_LIB = $(if $(findstring @,$(libdep)), $(call REF_ONE_K2_KERN_TRG_LIB,$(subst @,,$(libdep))),$(libdep))

REFKRNTRG_LIBS = $(foreach libdep,$(KERN_TARGET_LIBS),$(REF_ONE_KERN_TRG_LIB))
REFKRNTRGDLX_LIBS = $(foreach libdep,$(KERN_TARGET_DLX),$(REF_ONE_KERN_TRG_LIB))

#========================================================================================
# DEFAULT OPTIONS
#========================================================================================
GCCOPT     += -c -nostdinc -fno-common -Wall -I $(K2_ROOT)/src/shared/inc
GCCOPT_S   += 
GCCOPT_C   += 
GCCOPT_CPP += 
LDOPT      += -q -static -nostdlib --no-define-common --no-undefined
LDENTRY    ?= -e __entry

#========================================================================================
# ARCHITECTURE OPTIONS
#========================================================================================
ifeq ($(K2_ARCH),A32)
GCCOPT += -march=armv7-a
endif
ifeq ($(K2_ARCH),X32)
GCCOPT += -march=i686 -m32 -malign-double 
endif

#========================================================================================
# PICK TARGET
#========================================================================================
K2_TARGET_TYPE_DEFINED := FALSE

#========================================================================================
# OBJ
#========================================================================================
ifeq ($(TARGET_TYPE),OBJ)
K2_TARGET_TYPE_DEFINED := TRUE

K2_TARGET_NAME_SPEC := $(K2_TARGET_NAME).o
K2_TARGET_FULL_SPEC := $(K2_TARGET_PATH)/$(K2_TARGET_NAME_SPEC)

default: $(K2_TARGET_FULL_SPEC)

$(K2_TARGET_FULL_SPEC): $(ONE_OBJECT) makefile $(K2_ROOT)/src/shared/build/pre.make $(K2_ROOT)/src/shared/build/post.make
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)) md $(subst /,\,$(K2_TARGET_PATH))
	@echo -------- Placing OBJ $@ --------
	@copy $(subst /,\,$(ONE_OBJECT)) $(subst /,\,$(K2_TARGET_FULL_SPEC)) > nul

endif

#========================================================================================
# LIB
#========================================================================================
ifeq ($(TARGET_TYPE),LIB)
K2_TARGET_TYPE_DEFINED := TRUE

K2_TARGET_NAME_SPEC := $(K2_TARGET_NAME).lib
K2_TARGET_FULL_SPEC := $(K2_TARGET_PATH)/$(K2_TARGET_NAME_SPEC)

default: $(K2_TARGET_FULL_SPEC)

$(K2_TARGET_FULL_SPEC): $(OBJECTS) makefile $(K2_ROOT)/src/shared/build/pre.make $(K2_ROOT)/src/shared/build/post.make
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)) md $(subst /,\,$(K2_TARGET_PATH))
	@echo -------- Creating LIB $@ --------
	@ar rco $@ $(OBJECTS)

endif
	
#========================================================================================
# ELF
#========================================================================================
ifeq ($(TARGET_TYPE),ELF)

K2_TARGET_TYPE_DEFINED := TRUE

K2_TARGET_NAME_SPEC := $(K2_TARGET_NAME).elf
K2_TARGET_FULL_SPEC := $(K2_TARGET_PATH)/$(K2_TARGET_NAME_SPEC)

CHECK_REFSRC_ONE_K2_LIB = checkref_$(1)
CHECK_REFSRC_ONE_LIB = $(if $(findstring @,$(libdep)), $(call CHECK_REFSRC_ONE_K2_LIB,$(subst @,,$(libdep))),$(libdep))
CHECK_REFSRC_LIBS = $(foreach libdep,$(SOURCE_LIBS),$(CHECK_REFSRC_ONE_LIB))
CHECK_REFSRC_KERN_LIBS = $(foreach libdep,$(KERN_SOURCE_LIBS),$(CHECK_REFSRC_ONE_LIB))

default: $(CHECK_REFSRC_LIBS) $(CHECK_REFSRC_KERN_LIBS) $(K2_TARGET_FULL_SPEC)

$(CHECK_REFSRC_LIBS) $(CHECK_REFSRC_KERN_LIBS):
	MAKE -S -C $(K2_ROOT)/src/$(subst checkref_,,$@)

$(K2_TARGET_FULL_SPEC): $(OBJECTS) makefile $(K2_ROOT)/src/shared/build/pre.make $(K2_ROOT)/src/shared/build/post.make $(REFSRC_LIBS) $(REFTRG_LIBS) $(REFKRNSRC_LIBS) $(REFKRNTRG_LIBS)
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)) md $(subst /,\,$(K2_TARGET_PATH))
	@echo --------Creating ELF $@ --------
	@ld $(LDOPT) $(LDENTRY) -o $@ -( $(LIBGCC_PATH) $(OBJECTS) $(REFSRC_LIBS) $(REFTRG_LIBS) $(REFKRNSRC_LIBS) $(REFKRNTRG_LIBS) -)
	
endif

#========================================================================================
# DLX
#========================================================================================
ifeq ($(TARGET_TYPE),DLX)

GCCOPT += -DK2_DLX

LDOPT += --script $(K2_ROOT)/src/shared/build/gcc_link.l

ifeq ($(basename $(K2_TARGET_NAME)),k2oscrt)

ifeq ($(K2_KERNEL),)
LDENTRY := -e __k2oscrt_user_entry
else
LDENTRY := -e __k2oscrt_kern_entry
endif
CRTSTUB_OBJ := 

else

LDENTRY := -e __K2OS_dlx_crt
CRTSTUB_OBJ := $(K2_TARGET_BASE)/obj/$(K2_BUILD_SPEC)/os/crtstub/crtstub.o

ifneq ($(K2_KERNEL),)
#ifeq ($(K2_USE_CRT_SRC),)
#KERN_TARGET_DLX += @os/crt/crtkern/$(K2_ARCH)/k2oscrt
#else
KERN_SOURCE_DLX += @os/crt/crtkern/$(K2_ARCH)/k2oscrt
#endif
else
#ifeq ($(K2_USE_CRT_SRC),)
#TARGET_DLX += @os/crt/crtuser/$(K2_ARCH)/k2oscrt
#else
SOURCE_DLX += @os/crt/crtuser/$(K2_ARCH)/k2oscrt
#endif
endif

endif

K2_TARGET_TYPE_DEFINED := TRUE

K2_TARGET_NAME_SPEC := $(K2_TARGET_NAME).dlx
K2_TARGET_FULL_SPEC := $(K2_TARGET_PATH)/$(K2_TARGET_NAME_SPEC)
K2_TARGET_ELFNAME_SPEC := $(K2_TARGET_NAME).elf
K2_TARGET_ELFFULL_SPEC := $(K2_TARGET_PATH)/srcelf/$(K2_SUBPATH)/$(K2_TARGET_ELFNAME_SPEC)

ifneq ($(K2_KERNEL),)
K2_TARGET_EXPORTLIB_PATH := $(K2_TARGET_BASE)/srclib/$(K2_BUILD_SPEC)/kern
else
K2_TARGET_EXPORTLIB_PATH := $(K2_TARGET_BASE)/srclib/$(K2_BUILD_SPEC)
endif

ifeq ($(DLX_STACK),)
DLX_STACK := 0
endif

ifeq ($(DLX_INF),)
$(error No DLX_INF specified)
else
DLX_INF_O := $(K2_OBJECT_PATH)/exp_$(K2_TARGET_NAME).o
EXPORT_CMD := k2export -i $(DLX_INF) -o $(DLX_INF_O) $(LIBGCC_PATH) $(OBJECTS) $(REFSRC_LIBS) $(REFKRNSRC_LIBS) $(REFKRNSRCDLX_LIBS) $(REFSRCDLX_LIBS) $(REFTRG_LIBS) $(REFKRNTRG_LIBS) $(REFKRNTRGDLX_LIBS) $(REFTRGDLX_LIBS) 
endif

CHECK_REFSRC_ONE_K2_LIB = checkref_$(1)
CHECK_REFSRC_ONE_LIB = $(if $(findstring @,$(libdep)), $(call CHECK_REFSRC_ONE_K2_LIB,$(subst @,,$(libdep))),$(libdep))
CHECK_REFSRC_LIBS = $(foreach libdep,$(SOURCE_LIBS),$(CHECK_REFSRC_ONE_LIB))
CHECK_REFSRC_KERN_LIBS = $(foreach libdep,$(KERN_SOURCE_LIBS),$(CHECK_REFSRC_ONE_LIB))
CHECK_REFSRC_KERN_DLX = $(foreach libdep,$(KERN_SOURCE_DLX),$(CHECK_REFSRC_ONE_LIB))
CHECK_REFSRC_DLX = $(foreach libdep,$(SOURCE_DLX),$(CHECK_REFSRC_ONE_LIB))
CHECK_CRTSTUB_OBJ = checkref_os/crtstub

default: $(CHECK_REFSRC_LIBS) $(CHECK_CRTSTUB_OBJ) $(CHECK_REFSRC_KERN_LIBS) $(CHECK_REFSRC_KERN_DLX) $(CHECK_REFSRC_DLX) $(K2_TARGET_FULL_SPEC)

$(CHECK_CRTSTUB_OBJ) $(CHECK_REFSRC_LIBS) $(CHECK_REFSRC_KERN_LIBS) $(CHECK_REFSRC_KERN_DLX) $(CHECK_REFSRC_DLX):
	MAKE -S -C $(K2_ROOT)/src/$(subst checkref_,,$@)

$(K2_TARGET_ELFFULL_SPEC): $(OBJECTS) $(CRT_OBJ) makefile $(K2_ROOT)/src/shared/build/pre.make $(K2_ROOT)/src/shared/build/post.make $(REFSRC_LIBS) $(REFKRNSRC_LIBS) $(REFKRNSRCDLX_LIBS) $(REFSRCDLX_LIBS) $(REFTRG_LIBS) $(REFKRNTRG_LIBS) $(REFKRNTRGDLX_LIBS) $(REFTRGDLX_LIBS) $(DLX_INF)
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)) md $(subst /,\,$(K2_TARGET_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_EXPORTLIB_PATH)) md $(subst /,\,$(K2_TARGET_EXPORTLIB_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)/srcelf/$(K2_SUBPATH)) md $(subst /,\,$(K2_TARGET_PATH)/srcelf/$(K2_SUBPATH))
	@-if not exist $(subst /,\,$(K2_OBJECT_PATH)) md $(subst /,\,$(K2_OBJECT_PATH))
	@echo -------- Create Exports from ELF for DLX $@ --------
	@$(EXPORT_CMD)
	@echo -------- Linking ELF for DLX $@ --------
	@ld $(LDOPT) $(LDENTRY) -o $@ -( $(LIBGCC_PATH) $(OBJECTS) $(CRT_OBJ) $(REFSRC_LIBS) $(REFKRNSRC_LIBS) $(REFKRNSRCDLX_LIBS) $(REFSRCDLX_LIBS) $(REFTRG_LIBS) $(REFKRNTRG_LIBS) $(REFKRNTRGDLX_LIBS) $(REFTRGDLX_LIBS) $(DLX_INF_O) $(CRTSTUB_OBJ) -)

$(K2_TARGET_FULL_SPEC): $(K2_TARGET_ELFFULL_SPEC)
	@echo -------- Creating DLX from ELF for $@ --------
	@k2elf2dlx $(K2_SPEC_KERNEL) -s $(DLX_STACK) -i $(K2_TARGET_ELFFULL_SPEC) -o $(K2_TARGET_FULL_SPEC) -l $(K2_TARGET_EXPORTLIB_PATH)/$(K2_TARGET_NAME).lib
	
endif

#========================================================================================
# BUILTIN
#========================================================================================
ifeq ($(TARGET_TYPE),BUILTIN)

K2_TARGET_TYPE_DEFINED := TRUE

ifeq ($(K2_ARCH),X32)
KERN_STOCK_SOURCE_DLX := @os/crt/crtkern/x32/k2oscrt
KERN_STOCK_SOURCE_DLX += @os/kern/x32/k2oskern 
else
ifeq ($(K2_ARCH),A32)
KERN_STOCK_SOURCE_DLX := @os/crt/crtkern/a32/k2oscrt
KERN_STOCK_SOURCE_DLX += @os/kern/a32/k2oskern 
else
$(error unknown arch)
endif
endif

KERN_STOCK_SOURCE_DLX += @os/kern/k2osexec
KERN_STOCK_SOURCE_DLX += @os/kern/k2osacpi

KERN_STOCK_SOURCE_DLX += $(IMAGE_HAL)

K2_TARGET_NAME_SPEC := builtin.img
K2_TARGET_FULL_SPEC := $(K2_TARGET_PATH)/$(K2_TARGET_NAME)/$(K2_TARGET_NAME_SPEC)

CHECK_REFSRC_ONE_K2_LIB = checkref_$(1)
CHECK_REFSRC_ONE_LIB = $(if $(findstring @,$(libdep)), $(call CHECK_REFSRC_ONE_K2_LIB,$(subst @,,$(libdep))),$(libdep))
CHECK_REFSRC_KERN_DLX = $(foreach libdep,$(KERN_SOURCE_DLX),$(CHECK_REFSRC_ONE_LIB))
CHECK_REFSRC_DLX = $(foreach libdep,$(SOURCE_DLX),$(CHECK_REFSRC_ONE_LIB))

CHECK_STOCK_KERN_DLX = $(foreach libdep,$(KERN_STOCK_SOURCE_DLX),$(CHECK_REFSRC_ONE_LIB))

default: $(CHECK_STOCK_KERN_DLX) $(CHECK_REFSRC_KERN_DLX) $(CHECK_REFSRC_DLX) $(K2_TARGET_FULL_SPEC)

$(CHECK_STOCK_KERN_DLX) $(CHECK_REFSRC_DLX) $(CHECK_REFSRC_KERN_DLX):
	MAKE -S -C $(K2_ROOT)/src/$(subst checkref_,,$@)

GEN_ONE_IMAGE_DLX = $(K2_TARGET_PATH)\builtin\$(notdir $(1)).dlx
GEN_ONE_IMAGE_KERN_DLX = $(K2_TARGET_PATH)\builtin\kern\$(notdir $(1)).dlx

REFSRCDLX = $(foreach dlxdep,$(SOURCE_DLX),$(call GEN_ONE_IMAGE_DLX,$(dlxdep)))
REFKRNSRCDLX = $(foreach dlxdep,$(KERN_SOURCE_DLX),$(call GEN_ONE_IMAGE_KERN_DLX,$(dlxdep)))

$(REFSRCDLX):
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)) md $(subst /,\,$(K2_TARGET_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)) md $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME))
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin) md $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin)
	@copy $(subst /,\,$(K2_TARGET_BASE)/dlx/$(K2_BUILD_SPEC)/$(notdir $@)) $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin)

$(REFKRNSRCDLX):
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)) md $(subst /,\,$(K2_TARGET_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)) md $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME))
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin) md $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin)
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin\kern) md $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin\kern)
	@copy $(subst /,\,$(K2_TARGET_BASE)/dlx/$(K2_BUILD_SPEC)/kern/$(notdir $@)) $(subst /,\,$(K2_TARGET_PATH)/$(K2_TARGET_NAME)/builtin/kern)

$(K2_TARGET_FULL_SPEC): makefile $(K2_ROOT)/src/shared/build/pre.make $(K2_ROOT)/src/shared/build/post.make $(REFKRNSRCDLX_LIBS) $(REFSRCDLX_LIBS) $(REFKRNTRGDLX_LIBS) $(REFTRGDLX_LIBS) $(REFSRCDLX) $(REFTRGDLX) $(REFKRNSRCDLX) $(REFKRNTRGDLX)
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)) md $(subst /,\,$(K2_TARGET_PATH))
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)) md $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME))
	@-if not exist $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin) md $(subst /,\,$(K2_TARGET_PATH)\$(K2_TARGET_NAME)\builtin)
	@echo -------- Creating image $@ --------
	k2zipper $(subst /,\,$(K2_TARGET_PATH)/$(K2_TARGET_NAME)/builtin) $(subst /,\,$@)

endif

#========================================================================================
# Unknown
#========================================================================================
ifneq ($(K2_TARGET_TYPE_DEFINED),TRUE)
$(error No Valid TARGET_TYPE defined.)
endif

#========================================================================================
# clean target
#========================================================================================
clean:
	@-if exist "$(subst /,\,$(K2_TARGET_FULL_SPEC))" del "$(subst /,\,$(K2_TARGET_FULL_SPEC))"
	@-if exist "$(subst /,\,$(K2_OBJECT_PATH))" rd /s /q "$(subst /,\,$(K2_OBJECT_PATH))"
	@echo --------$(K2_TARGET_FULL_SPEC) Cleaned --------


#========================================================================================
# S
#========================================================================================
$(K2_OBJECT_PATH)/%.o : %.s makefile $(K2_ROOT)/src/shared/build/pre.make $(K2_ROOT)/src/shared/build/post.make
	@-if not exist $(subst /,\,$(dir $@)) md $(subst /,\,$(dir $@))
	@echo $(TARGET_NAMEONLY) - Preprocess $<
	@if exist $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).d) del $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).d)
	@gcc -x assembler-with-cpp -E $(GCCOPT) $(GCCOPT_S) -D_ASSEMBLER -MT '$(K2_OBJECT_PATH)/$(notdir $@)' -MD -MF $(K2_OBJECT_PATH)/$(basename $<).dx -o $(K2_OBJECT_PATH)/$(basename $<).s $<
	@if exist $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).dx) ren $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).dx) $(notdir $(basename $<).d)
	@echo $(TARGET_NAMEONLY) - Assemble $<
	@gcc $(GCCOPT) -o $(K2_OBJECT_PATH)/$(basename $<).o $(K2_OBJECT_PATH)/$(basename $<).s

#========================================================================================
# C and CPP
#========================================================================================
$(K2_OBJECT_PATH)/%.o : %.c makefile $(K2_ROOT)/src/shared/build/pre.make $(K2_ROOT)/src/shared/build/post.make
	@-if not exist $(subst /,\,$(dir $@)) md $(subst /,\,$(dir $@))
	@echo $(TARGET_NAMEONLY) - Compile $<
	@if exist $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).d) del $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).d)
	@gcc -x c $(GCCOPT) $(GCCOPT_C) $(GCCOPT_DEBUG) -MT '$(K2_OBJECT_PATH)/$(notdir $@)' -MD -MF $(K2_OBJECT_PATH)/$(basename $<).dx -o $(K2_OBJECT_PATH)/$(basename $<).o $<
	@if exist $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).dx) ren $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).dx) $(notdir $(basename $<).d)

$(K2_OBJECT_PATH)/%.o : %.cpp makefile $(K2_ROOT)/src/shared/build/pre.make $(K2_ROOT)/src/shared/build/post.make
	@-if not exist $(subst /,\,$(dir $@)) md $(subst /,\,$(dir $@))
	@echo $(TARGET_NAMEONLY) - Compile $<
	@if exist $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).d) del $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).d)
	@gcc -x c++ $(GCCOPT) $(GCCOPT_C) $(GCCOPT_CPP) $(GCCOPT_DEBUG) -MT '$(K2_OBJECT_PATH)/$(notdir $@)' -MD -MF $(K2_OBJECT_PATH)/$(basename $<).dx -o $(K2_OBJECT_PATH)/$(basename $<).o $<
	@if exist $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).dx) ren $(subst /,\,$(K2_OBJECT_PATH)/$(basename $<).dx) $(notdir $(basename $<).d)

#========================================================================================
# Autodependencies
#========================================================================================
-include $(OBJECTS:.o=.d)

#========================================================================================



#========================================================================================
# TARGET_TYPE == ELF
#========================================================================================
ifeq ($(TARGET_TYPE),ELF)

K2_TARGET_NAME_SPEC := $(K2_TARGET_NAME).elf
K2_TARGET_FULL_SPEC := $(K2_TARGET_PATH)/$(K2_TARGET_NAME_SPEC)

FORM_ONE_K2_LIB = $(K2_TARGET_BASE)/lib/$(K2_BUILD_SPEC)/$(1).lib
EXPAND_ONE_LIB = $(if $(findstring @,$(libdep)), $(call FORM_ONE_K2_LIB,$(subst @,,$(libdep))),$(libdep))
LIBRARIES = $(foreach libdep,$(STATIC_LIBRARIES),$(EXPAND_ONE_USER_LIB))

$(K2_TARGET_FULL_SPEC): $(OBJECTS) $(LIBRARIES) $(BUILD_CONTROL_FILES)
	-if not exist $(subst /,\,$(K2_TARGET_PATH)) md $(subst /,\,$(K2_TARGET_PATH))
	echo --------Creating ELF $@ --------
	ld $(LDOPT) $(LDENTRY) -o $@ -( $(LIBGCC_PATH) $(OBJECTS) $(LIBRARIES) -)

default: $(K2_TARGET_FULL_SPEC)
	
endif



