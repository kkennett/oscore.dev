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

TARGET_SPECIFIC_ARCH = A32

K2_KERNEL := TRUE

DLX_INF := ../../k2oskern.inf

SOURCES += a32kern.c
SOURCES += a32setintr.c
SOURCES += a32coreix.c
SOURCES += a32kernex.c
SOURCES += a32map.c
SOURCES += a32cpu.c
SOURCES += a32cpuasm.s
SOURCES += a32thread.c
SOURCES += a32threadasm.s
SOURCES += a32intr.c
SOURCES += a32intrasm.s
SOURCES += a32monitor.c 
SOURCES += a32monitorasm.s
SOURCES += a32auxcoreasm.s
SOURCES += a32time.c

SOURCE_LIBS += @shared/lib/k2archa32
SOURCE_LIBS += @shared/lib/k2heap

SOURCE_LIBS += @os/lib/k2ramheap

KERN_SOURCE_LIBS += @os/kern

include $(K2_ROOT)/src/shared/build/post.make

