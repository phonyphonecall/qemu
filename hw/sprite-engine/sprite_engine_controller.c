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

#include <arpa/inet.h>
#include <sys/socket.h>
#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"

#define TYPE_SPRITE_ENGINE_CONTROLLER "sprite-engine-controller"
#define SPRITE_ENGINE_CONTROLLER(obj) \
    OBJECT_CHECK(struct control, (obj), TYPE_SPRITE_ENGINE_CONTROLLER)

// MIN and MAX are defined inclusive
#define CONTROL_REGION   0x00000000

struct control
{
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    uint32_t port;
    QemuThread server_thread;
    QemuMutex reg_lock;
    uint32_t reg;
};

static uint64_t
sprite_engine_controller_read(void *opaque, hwaddr addr, unsigned int size)
{
    struct control* c = (struct control*) opaque;

    assert(addr == CONTROL_REGION);
    assert(size == 4);
    uint32_t val;
    qemu_mutex_lock(&c->reg_lock);
    val = c->reg;
    qemu_mutex_unlock(&c->reg_lock);

    return val;
}

static void
sprite_engine_controller_write(void *opaque, hwaddr addr,
            uint64_t val64, unsigned int size)
{
    int log = open("/tmp/se-control-write.log", O_CREAT | O_RDWR | O_APPEND, 0777);

    dprintf(log, "sprite_engine_controller_write: don't write to these plz thx");
    close(log);
}

static const MemoryRegionOps sprite_engine_controller_ops = {
    .read = sprite_engine_controller_read,
    .write = sprite_engine_controller_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static void sprite_engine_controller_svr_main(void* args) {
    int rv;
    int sockd;
    int connd;
    struct sockaddr_in server;
    struct sockaddr_in client;
    int log = open("/tmp/se-controller-svr-main.log", O_CREAT | O_RDWR | O_APPEND, 0777);
    struct control* c = (struct control*) ((struct control*) args);


    dprintf(log, "sprite_engine_controller_svr_main: binding to port %d\n", c->port);

    sockd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockd == -1) {
        dprintf(log, "sprite_engine_controller_svr_main: failed to create socket");
        return;
    }

    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = PF_INET;
    server.sin_port = htons(c->port);
    rv = bind(sockd, (struct sockaddr *) &server, sizeof(server));
    if (rv == -1) {
        dprintf(log, "sprite_engine_controller_svr_main: failed to bind to port");
        return;
    }

    // backlog of 1. Should only be 1 connection
    rv = listen(sockd, 1);
    if (rv == -1) {
        dprintf(log, "sprite_engine_controller_svr_main: listen failed");
        return;
    }

    int client_length = sizeof(client);
    connd = accept(sockd, (struct sockaddr*) &client, (socklen_t *) &client_length);
    if (connd == -1) {
        dprintf(log, "sprite_engine_controller_svr_main: failed to accept");
        return;
    }

    // continually read new reg values, and store them
    int reg_update;
    while (1) {
        ssize_t read_size = read(connd, &reg_update, sizeof(int)); 
        if (read_size != sizeof(int)) {
            dprintf(log, "sprite_engine_controller_svr_main: weird read size of %zd", read_size);
        } else {
            qemu_mutex_lock(&c->reg_lock);
            c->reg = reg_update;
            qemu_mutex_unlock(&c->reg_lock);
        }
    }
}

static void sprite_engine_controller_realize(DeviceState *dev, Error **errp)
{
    struct control *c = SPRITE_ENGINE_CONTROLLER(dev);

    memory_region_init_io(&c->mmio, OBJECT(c), &sprite_engine_controller_ops, c, "sprite-engine-control", 0x01);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &c->mmio);

    // launch ctrl server
    qemu_thread_create(&c->server_thread, "se_ctrl_th",
                       (void *) sprite_engine_controller_svr_main,
                       (void *) c, QEMU_THREAD_JOINABLE);
}



static void sprite_engine_controller_init(Object *obj)
{
    struct control *c = SPRITE_ENGINE_CONTROLLER(obj);

    qemu_mutex_init(&c->reg_lock);
}

static Property sprite_engine_controller_properties[] = {
    DEFINE_PROP_UINT32("port", struct control, port, -1),
    DEFINE_PROP_END_OF_LIST(),
};

static void sprite_engine_controller_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = sprite_engine_controller_realize;
    dc->props = sprite_engine_controller_properties;
}

static const TypeInfo sprite_engine_controller_info = {
    .name          = TYPE_SPRITE_ENGINE_CONTROLLER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(struct control),
    .instance_init = sprite_engine_controller_init,
    .class_init    = sprite_engine_controller_class_init,
};

static void sprite_engine_controller_register_types(void)
{
    type_register_static(&sprite_engine_controller_info);
}

type_init(sprite_engine_controller_register_types)

