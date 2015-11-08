#include "hw/sprite-engine/sprite_engine_vsync_counter.h"

#include "qemu/thread.h"


// count/lock pair
static uint32_t vsync_count;
static QemuMutex count_lock;

void init_sprite_engine_vsync_count() {
    qemu_mutex_init(&count_lock);
    vsync_count = 0;
}

int get_sprite_engine_vsync_count() {
    uint32_t ret = 0;
    qemu_mutex_lock(&count_lock);
    ret = vsync_count;
    qemu_mutex_unlock(&count_lock);

    return ret;
}

void inc_sprite_engine_vsync_count() {
    qemu_mutex_lock(&count_lock);
    vsync_count++;
    qemu_mutex_unlock(&count_lock);
}
