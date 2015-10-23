/*
 * QEMU model of the vsync signal produced by the sprite engine.
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

#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"

#define D(x)

#define R_TCSR     0
#define R_TLR      1
#define R_TCR      2
#define R_MAX      4

#define TCSR_MDT        (1<<0)
#define TCSR_UDT        (1<<1)
#define TCSR_GENT       (1<<2)
#define TCSR_CAPT       (1<<3)
#define TCSR_ARHT       (1<<4)
#define TCSR_LOAD       (1<<5)
#define TCSR_ENIT       (1<<6)
#define TCSR_ENT        (1<<7)
#define TCSR_TINT       (1<<8)
#define TCSR_PWMA       (1<<9)
#define TCSR_ENALL      (1<<10)

struct vsync_timer
{
    QEMUBH *bh;
    ptimer_state *ptimer;
    void *parent;
    unsigned long timer_div;
    uint32_t regs[R_MAX];
};

#define TYPE_SE_VSYNC "sprite-engine.vsync"
#define SE_VSYNC(obj) \
    OBJECT_CHECK(struct timerblock, (obj), TYPE_SE_VSYNC)

struct timerblock
{
    SysBusDevice parent_obj;

    qemu_irq irq;
    uint32_t freq_hz;
    struct vsync_timer timer;
};


static void timer_update_irq(struct timerblock *t)
{
    // Bring IRQ line high
    qemu_set_irq(t->irq, 1);
}

static void timer_enable(struct vsync_timer *xt)
{
    // 1000000000 == 60hz
    uint64_t count = 1000;
    ptimer_stop(xt->ptimer);

    ptimer_set_limit(xt->ptimer, count, 1);

    // 0 == periodic (not oneshot)
    ptimer_run(xt->ptimer, 0);
}

static void timer_hit(void *opaque)
{
    struct vsync_timer *xt = opaque;
    struct timerblock *t = xt->parent;

    timer_update_irq(t);
}

static void se_vsync_realize(DeviceState *dev, Error **errp)
{
    struct timerblock *t = SE_VSYNC(dev);
    struct vsync_timer *xt = &t->timer;
    xt->parent = t;
    xt->bh = qemu_bh_new(timer_hit, xt);
    xt->ptimer = ptimer_init(xt->bh);
    ptimer_set_freq(xt->ptimer, t->freq_hz);

    // Enable immediately
    timer_enable(xt);
}

static void se_vsync_init(Object *obj)
{
    struct timerblock *t = SE_VSYNC(obj);

    // A single IRQ, to connect to cpu IRQ line
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &t->irq);
}

static Property se_vsync_properties[] = {
    DEFINE_PROP_UINT32("clock-frequency", struct timerblock, freq_hz,
                                                                1000000),
    DEFINE_PROP_END_OF_LIST(),
};

static void se_vsync_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = se_vsync_realize;
    dc->props = se_vsync_properties;
}

static const TypeInfo xilinx_timer_info = {
    .name          = TYPE_SE_VSYNC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(struct timerblock),
    .instance_init = se_vsync_init,
    .class_init    = se_vsync_class_init,
};

static void xilinx_timer_register_types(void)
{
    type_register_static(&xilinx_timer_info);
}

type_init(xilinx_timer_register_types)
