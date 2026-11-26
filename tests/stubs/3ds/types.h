#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32; typedef int64_t s64;
typedef volatile u32 vu32; typedef s32 Result; typedef u32 Handle;
#define BIT(n) (1U<<(n))
