/*
 * QEMU model of the Xilinx timer block.
 *
 * Copyright (c) 2009 Edgar E. Iglesias.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include<arpa/inet.h>
#include <sys/socket.h>
#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"

#include "hw/sprite-engine/sprite_engine_commands.h"

#define TYPE_SPRITE_ENGINE "sprite-engine"
#define SPRITE_ENGINE(obj) \
    OBJECT_CHECK(struct engineblock, (obj), TYPE_SPRITE_ENGINE)

// MIN and MAX are defined inclusive
#define SE_REGION_MIN   0x00000000
#define SE_OAM_MIN      (SE_REGION_MIN)
#define SE_OAM_MAX      ((SE_REGION_MIN) + 0x1EC)
#define SE_PRIORITY_CTL ((SE_REGION_MIN) + 0x1FC)
#define SE_INST_MIN     ((SE_REGION_MIN) + 0x200)
#define SE_INST_MAX     ((SE_REGION_MIN) + 0x3FC)
#define SE_CRAM_MIN     ((SE_REGION_MIN) + 0x400)
#define SE_CRAM_MAX     ((SE_REGION_MIN) + 0x4FC)
#define SE_VRAM_MIN     ((SE_REGION_MIN) + 0x800)
#define SE_VRAM_MAX     ((SE_REGION_MIN) + 0x803)
#define SE_REGION_MAX   SE_VRAM_MAX

struct engineblock
{
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    int sockd;
};

static uint64_t
sprite_engine_read(void *opaque, hwaddr addr, unsigned int size)
{
    // TODO: write this if you want it
    return 0;
}

static void
sprite_engine_write(void *opaque, hwaddr addr,
            uint64_t val64, unsigned int size)
{
    union SECommand cmd;
    struct engineblock *engine = opaque;

    int log = open("/tmp/se-write-qemu.log", O_CREAT | O_RDWR | O_APPEND, 0777);

    memset(&cmd, 0, sizeof(union SECommand));

    if (addr >= SE_OAM_MIN && addr <= SE_OAM_MAX) {
        // OAM write
        dprintf(log, "sprite_engine_write (OAM): at addr %llx  val %llu\n", addr, val64);
        int oamRegIndex = addr >> 2;
        uint32_t val = (uint32_t) val64;
        fillUpdateOAM(oamRegIndex, val, &cmd.update_oam);
        debugUpdateOAM(log, &cmd.update_oam);
    } else if (addr == SE_PRIORITY_CTL) {
        // Priority write
        dprintf(log, "sprite_engine_write (PRIORITY): at addr %llx  val %llu\n", addr, val64);
        uint8_t val = (uint8_t) val64;
        fillPriorityControl(val, &cmd.set_priority_control);
        debugPriorityControl(log, &cmd.set_priority_control);
    } else if (addr >= SE_INST_MIN && addr <= SE_INST_MAX) {
        // Instance write
        dprintf(log, "sprite_engine_write (INSTANCE): at addr %llx  val %llu\n", addr, val64);
        int oamRegIndex = addr >> 2;
        fillUpdateInstOAM(oamRegIndex, val64, &cmd.update_oam);
        debugUpdateOAM(log, &cmd.update_oam);
    } else if (addr >= SE_CRAM_MIN && addr <= SE_CRAM_MAX) {
        // CRAM write
        dprintf(log, "sprite_engine_write (CRAM): at addr %llx  val %llu\n", addr, val64);
        int oamRegIndex = addr >> 2;
        uint32_t val = (uint32_t) val64;
        fillUpdateCRAM(oamRegIndex, val, &cmd.update_cram);
        debugUpdateCRAM(log, &cmd.update_cram);
    } else if (addr >= SE_VRAM_MIN && addr <= SE_VRAM_MAX) {
        // VRAM write
        dprintf(log, "sprite_engine_write (VRAM): at addr %llx  val %llu\n", addr, val64);
        int oamRegIndex = addr >> 2;
        uint32_t val = (uint32_t) val64;
        fillUpdateVRAM(oamRegIndex, val, &cmd.update_vram);
        debugUpdateVRAM(log, &cmd.update_vram);
    } else {
        // Out of range. Ignore it?
        dprintf(log, "sprite_engine_write (OUT_OF_RANGE): at addr %llx  val %llu\n", addr, val64);
        close(log);
        return;
    }
    size_t cmd_size = sizeof(union SECommand);
    ssize_t rv = send(engine->sockd, &cmd, cmd_size, 0);
    if (rv != cmd_size) {
        dprintf(log, "sprite_engine_write send failed expected: %zd  observed: %zd\n", cmd_size, rv);
    }
    close(log);
}

static const MemoryRegionOps sprite_engine_ops = {
    .read = sprite_engine_read,
    .write = sprite_engine_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static void sprite_engine_realize(DeviceState *dev, Error **errp)
{
    struct engineblock *engine = SPRITE_ENGINE(dev);

    memory_region_init_io(&engine->mmio, OBJECT(engine), &sprite_engine_ops, engine, "sprite-engine", 0x00002000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &engine->mmio);
}

static void sprite_engine_init(Object *obj)
{
    struct sockaddr_in server;
    struct engineblock *engine = SPRITE_ENGINE(obj);

    int log = open("/tmp/se-init.log", O_CREAT | O_RDWR | O_APPEND, 0777);

    int sockd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockd == -1) {
        dprintf(log, "sprite_engine_init: failed to create socket");
        return;
    }

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = PF_INET;
    server.sin_port = htons( 1985 );
    int rv = connect(sockd, (struct sockaddr*) &server, sizeof(struct sockaddr));
    if (rv != 0) {
        dprintf(log, "sprite_engine_init: failed to connect\n");
        return;
    }
    engine->sockd = sockd;
    close(log);
}

static Property sprite_engine_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void sprite_engine_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = sprite_engine_realize;
    dc->props = sprite_engine_properties;
}

static const TypeInfo sprite_engine_info = {
    .name          = TYPE_SPRITE_ENGINE,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(struct engineblock),
    .instance_init = sprite_engine_init,
    .class_init    = sprite_engine_class_init,
};

static void sprite_engine_register_types(void)
{
    type_register_static(&sprite_engine_info);
}

type_init(sprite_engine_register_types)

