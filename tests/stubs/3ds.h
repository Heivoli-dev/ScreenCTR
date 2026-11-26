/* Host-only stand-in for <3ds.h>: declares exactly the libctru API used by
 * the console code (signatures copied from libctru 2.x headers as understood
 * by the author) so the sysmodule's own C can be syntax/type checked on a PC.
 * This is NOT a 3DS build and proves nothing about runtime behaviour. */
#pragma once
#include "3ds/types.h"
#include <stdbool.h>
#include <stddef.h>
#define R_FAILED(r) ((r) < 0)
#define R_SUCCEEDED(r) ((r) >= 0)
#define CUR_PROCESS_HANDLE 0xFFFF8001
#define U64_MAX UINT64_MAX
#define SYSCLOCK_ARM11 268111856ULL
#define OS_HEAP_AREA_BEGIN 0x08000000
#define IPC_MakeHeader(c,n,t) (((u32)(c)<<16)|((u32)(n)<<6)|(u32)(t))
typedef struct { u32 *framebuf0_vaddr, *framebuf1_vaddr; u32 format, framebuf_widthbytesize; } GSPGPU_CaptureInfoEntry;
typedef struct { GSPGPU_CaptureInfoEntry screencapture[2]; } GSPGPU_CaptureInfo;
typedef struct { s8 deviceId; s8 allowedAlignments; s16 burstSize, transferSize, burstStride, transferStride; } DmaDeviceConfig;
typedef struct { s8 channelId; s8 endianSwapSize; u8 flags; u8 _padding; DmaDeviceConfig dstCfg, srcCfg; } DmaConfig;
typedef enum { DMASTATE_STARTING=0, DMASTATE_WFP_DST, DMASTATE_WFP_SRC, DMASTATE_RUNNING, DMASTATE_DONE, DMASTATE_CANCELLED } DmaState;
typedef struct { u64 programId; u8 mediaType; u8 padding[7]; } FS_ProgramInfo;
typedef struct { int x; } *Thread; typedef void (*ThreadFunc)(void *);
typedef struct { s32 v; } LightLock;
enum { MEMOP_ALLOC = 3 }; enum { MEMPERM_READ = 1, MEMPERM_WRITE = 2 };
enum { GFX_TOP = 0 }; enum { KEY_START = 8 };
Result svcControlMemory(u32*,u32,u32,u32,u32,u32); Result svcOpenProcess(Handle*,u32); Result svcCloseHandle(Handle);
Result svcGetProcessList(s32*,u32*,s32); Result svcGetProcessInfo(s64*,Handle,u32); Result svcSendSyncRequest(Handle);
Result svcStartInterProcessDma(Handle*,Handle,u32,Handle,u32,u32,const DmaConfig*);
Result svcGetDmaState(DmaState*,Handle); Result svcStopDma(Handle); Result svcInvalidateProcessDataCache(Handle,u32,u32);
Result svcFlushProcessDataCache(Handle,u32,u32);
void svcSleepThread(s64); u64 svcGetSystemTick(void);
Result svcWaitSynchronization(Handle,s64);
u32 *getThreadCommandBuffer(void); u64 osGetTime(void);
void dmaConfigInitDefault(DmaConfig*); Result GSPGPU_ImportDisplayCaptureInfo(GSPGPU_CaptureInfo*);
Result srvInit(void); void srvExit(void); Result srvGetServiceHandle(Handle*,const char*); Result srvEnableNotification(Handle*); Result srvReceiveNotification(u32*);
Result fsInit(void); void fsExit(void); Result archiveMountSdmc(void); Result archiveUnmountAll(void);
void gspInit(void); void gspExit(void); Result pmDbgInit(void); void pmDbgExit(void);
Handle *pmDbgGetSessionHandle(void); Result cfguInit(void); void cfguExit(void); Result CFGU_GetSystemModel(u8*);
Result socInit(u32*,u32); Result socExit(void); long gethostid(void);
Thread threadCreate(ThreadFunc,void*,size_t,int,int,bool); Result threadJoin(Thread,u64); void threadFree(Thread);
void LightLock_Init(LightLock*); void LightLock_Lock(LightLock*); void LightLock_Unlock(LightLock*);
void gfxInitDefault(void); void gfxExit(void); void *consoleInit(int, void*); void nsInit(void); void nsExit(void);
Result NS_LaunchTitle(u64,u32,u32*); bool aptMainLoop(void); void hidScanInput(void); u32 hidKeysDown(void); void gspWaitForVBlank(void);
Result hidInit(void); void hidExit(void); u32 hidKeysHeld(void);
