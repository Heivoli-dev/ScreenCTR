/* Luma3DS custom SVCs used by ScreenCTR (not part of libctru).
 * Prototypes match Luma3DS sysmodules/rosalina/include/csvc.h; wrappers in csvc.s. */
#pragma once
#include <3ds/types.h>

typedef enum MapExFlags {
    MAPEXFLAGS_PRIVATE = BIT(0), /* map as PRIVATE instead of SHARED */
} MapExFlags;

typedef enum ProcessOp {
    PROCESSOP_GET_ALL_HANDLES,
    PROCESSOP_SET_MMU_TO_RWX,
    PROCESSOP_GET_ON_MEMORY_CHANGE_EVENT,
    PROCESSOP_SIGNAL_ON_EXIT,
    PROCESSOP_GET_PA_FROM_VA,
    PROCESSOP_SCHEDULE_THREADS, /* varg2: 0 = unlock, other = lock the process's threads */
} ProcessOp;

Result svcMapProcessMemoryEx(Handle dstProcessHandle, u32 destAddress, Handle srcProcessHandle, u32 srcAddress, u32 size, MapExFlags flags);
Result svcUnmapProcessMemoryEx(Handle process, u32 destAddress, u32 size);
Result svcControlProcess(Handle process, ProcessOp op, u32 varg2, u32 varg3);
