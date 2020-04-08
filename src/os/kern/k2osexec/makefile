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

include $(K2_ROOT)/src/os/build/k2ospre.make

TARGET_TYPE = DLX
K2_KERNEL := TRUE

KERN_SOURCE_DLX += @os/kern/$(K2_ARCH)/k2oskern 
KERN_SOURCE_DLX += @os/kern/k2osacpi

DLX_INF := k2osexec.inf

GCCOPT += -I $(K2_ROOT)/src/os/kern/k2osacpi/acpica/source/include
GCCOPT += -I $(K2_ROOT)/src/os/kern/k2osacpi/acpica/source/include/platform
GCCOPT += -DACPI_USE_NATIVE_DIVIDE

#
# Executive
#
SOURCES += dlx_entry.c 
SOURCES += run.c

ifeq ($(K2_ARCH),X32)
SOURCE_LIBS += @shared/lib/k2archx32
endif

ifeq ($(K2_ARCH),A32)
SOURCE_LIBS += @shared/lib/k2archa32
endif

include $(K2_ROOT)/src/shared/build/post.make

