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

TARGET_TYPE = LIB
K2_KERNEL := TRUE

SOURCES += dlx_entry.c
SOURCES += seqlock.c
SOURCES += debug.c
SOURCES += panic.c
SOURCES += exec.c
SOURCES += exception.c
SOURCES += dlxsupp.c
SOURCES += device.c
SOURCES += initmem.c
SOURCES += esc.c
SOURCES += proc.c 
SOURCES += thread.c 
SOURCES += seg.c
SOURCES += sched.c
SOURCES += hal.c
SOURCES += cpucore.c
SOURCES += monitor.c 
SOURCES += schedex_threadexit.c 
SOURCES += schedex_threadwait.c 
SOURCES += schedex_critsec.c 
SOURCES += schedex_debug.c
SOURCES += schedex_invtlb.c
SOURCES += schedex_purgept.c
SOURCES += sched_time.c
SOURCES += map.c
SOURCES += intr.c
SOURCES += mem.c
SOURCES += object.c
SOURCES += name.c
SOURCES += event.c

SOURCES += osapi_addr.c 
SOURCES += osapi_alarm.c 
SOURCES += osapi_critsec.c 
SOURCES += osapi_debug.c 
SOURCES += osapi_event.c 
SOURCES += osapi_heap.c 
SOURCES += osapi_mailbox.c 
SOURCES += osapi_mailslot.c 
SOURCES += osapi_msg.c 
SOURCES += osapi_name.c 
SOURCES += osapi_process.c 
SOURCES += osapi_rwlock.c 
SOURCES += osapi_sem.c 
SOURCES += osapi_sys.c 
SOURCES += osapi_thread.c 
SOURCES += osapi_time.c 
SOURCES += osapi_token.c 
SOURCES += osapi_virt.c 

include $(K2_ROOT)/src/shared/build/post.make

