#pragma once

#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef _Atomic(i8) atomic_i8;
typedef _Atomic(i16) atomic_i16;
typedef _Atomic(i32) atomic_i32;
typedef _Atomic(i64) atomic_i64;
typedef _Atomic(u8) atomic_u8;
typedef _Atomic(u16) atomic_u16;
typedef _Atomic(u32) atomic_u32;
typedef _Atomic(u64) atomic_u64;
typedef _Atomic(f32) atomic_f32;
typedef _Atomic(f64) atomic_f64;

