/* Host-side stand-in so MinHook's HDE64 builds outside Windows (tools only). */
#pragma once
#include <stdint.h>
#include <string.h>
#define INT8 int8_t
#define INT16 int16_t
#define INT32 int32_t
#define INT64 int64_t
#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define UINT64 uint64_t
typedef uint8_t BYTE;
typedef uint8_t *LPBYTE;
typedef uint8_t *PBYTE;
