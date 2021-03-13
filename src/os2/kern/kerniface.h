//   
//   BSD 3-Clause License
//   
//   Copyright (c) 2020, Kurt Kennett
//   All rights reserved.
//   
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//   
//   1. Redistributions of source code must retain the above copyright notice, this
//      list of conditions and the following disclaimer.
//   
//   2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//   
//   3. Neither the name of the copyright holder nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//   
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef __KERNIFACE_H
#define __KERNIFACE_H

/* --------------------------------------------------------------------------------- */

#define K2OS_SYSCALL_ID_OUTPUT_DEBUG        0
#define K2OS_SYSCALL_ID_DEBUG_BREAK         1
#define K2OS_SYSCALL_ID_CRT_INITDLX         2
#define K2OS_SYSCALL_ID_SIGNAL_NOTIFY       3
#define K2OS_SYSCALL_ID_WAIT_FOR_NOTIFY     4
#define K2OS_SYSCALL_ID_TEST_NOTIFY         5
#define K2OS_SYSCALL_ID_ALLOC_PHYS          6
#define K2OS_SYSCALL_ID_RENDER_PTMAP        7
#define K2OS_SYSCALL_ID_RAISE_EXCEPTION     8
#define K2OS_SYSCALL_ID_NOTIFY_CREATE       9
#define K2OS_SYSCALL_ID_TOKEN_DESTROY       10

/* --------------------------------------------------------------------------------- */

#endif // __KERNIFACE_H