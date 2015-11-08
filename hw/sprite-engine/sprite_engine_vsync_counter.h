#ifndef SPRITE_ENGINE_VSYNC_COUNTER_H
#define SPRITE_ENGINE_VSYNC_COUNTER_H

#include "qemu/main-loop.h"

void init_sprite_engine_vsync_count(void);
int get_sprite_engine_vsync_count(void);
void inc_sprite_engine_vsync_count(void);

#endif //SPRITE_ENGINE_VSYNC_COUNTER_H
