#pragma once

#include <stdatomic.h>
#include <types.h>
#include "core_engine.h"

typedef struct {
    _Atomic(f32) pan;
    _Atomic(f32) vol;
} Fader;

u16 Fader_Create(Fader* fader, f32 defaultPan, f32 defaultVol, CoreEngineContext* ctx);

