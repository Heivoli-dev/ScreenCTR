@   Luma3DS custom SVC wrappers — subset used by ScreenCTR.
@   Taken from Luma3DS sysmodules/rosalina/source/csvc.s (LumaTeam).
@   ALTERED SOURCE VERSION: reduced to the SVCs ScreenCTR needs.
@
@   This paricular file is licensed under the following terms:
@
@   This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable
@   for any damages arising from the use of this software.
@
@   Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it
@   and redistribute it freely, subject to the following restrictions:
@
@    The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
@    If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
@
@    Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
@    This notice may not be removed or altered from any source distribution.

.arm
.balign 4

.macro SVC_BEGIN name
    .section .text.\name, "ax", %progbits
    .global \name
    .type \name, %function
    .align 2
    .cfi_startproc
\name:
.endm

.macro SVC_END
    .cfi_endproc
.endm

SVC_BEGIN svcMapProcessMemoryEx
    push {r4, r5, r6}
    ldr r4, [sp, #12]
    ldr r5, [sp, #16]
    mov r6, r0 @ Move the dst handle to r6 to make room for magic value
    mov r0, #0xFFFFFFF2 @ Set r0 to magic value, which allows for backwards compatibility
    svc 0xA0
    pop {r4, r5, r6}
    bx lr
SVC_END

SVC_BEGIN svcUnmapProcessMemoryEx
    svc 0xA1
    bx lr
SVC_END

SVC_BEGIN svcControlProcess
    svc 0xB3
    bx lr
SVC_END
