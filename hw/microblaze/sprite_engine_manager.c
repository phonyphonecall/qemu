/*
 * Model of the sprite engine, for team nsnes
 *
 * Copyright (c) 2015 Scott Hendrickson
 * Copyright (c) 2015 Carter Peterson
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
#include "hw/hw.h"
#include "net/net.h"
#include "hw/block/flash.h"
#include "sysemu/sysemu.h"
#include "hw/devices.h"
#include "hw/boards.h"
#include "sysemu/block-backend.h"
#include "hw/char/serial.h"
#include "exec/address-spaces.h"
#include "hw/ssi.h"

#include "boot.h"

#include "hw/stream.h"

#define LMB_BRAM_SIZE  (4 * 128 * 1024)
#define FLASH_SIZE     (32 * 1024 * 1024)

#define BINARY_DEVICE_TREE_FILE "petalogix-ml605.dtb"

#define NUM_SPI_FLASHES 4

#define MEMORY_BASEADDR 0x50000000
#define FLASH_BASEADDR 0x86000000
#define INTC_BASEADDR 0x81800000
#define TIMER_BASEADDR 0x83c00000
#define UARTLITE_BASEADDR 0x84000000
#define SPRITE_ENGINE_BASEADDR 0xA0000000
#define SPRITE_ENGINE_CONTROLLER_1 0xB0000000
#define SPRITE_ENGINE_CONTROLLER_2 0xB0000004
#define SPRITE_ENGINE_CONTROLLER_3 0xB0000008
#define SPRITE_ENGINE_CONTROLLER_4 0xB000000C

#define AXIDMA_IRQ1         0
#define AXIDMA_IRQ0         1
#define TIMER_IRQ           2
#define AXIENET_IRQ         3
#define UARTLITE_IRQ        4

static void
sprite_engine_init(MachineState *machine)
{
    ram_addr_t ram_size = machine->ram_size;
    MemoryRegion *address_space_mem = get_system_memory();
    DeviceState *dev;
    MicroBlazeCPU *cpu;
    DriveInfo *dinfo;
    int i;
    MemoryRegion *phys_lmb_bram = g_new(MemoryRegion, 1);
    MemoryRegion *phys_ram = g_new(MemoryRegion, 1);
    qemu_irq irq[32];

    /* init CPUs */
    cpu = MICROBLAZE_CPU(object_new(TYPE_MICROBLAZE_CPU));
    /* Use FPU but don't use floating point conversion and square
     * root instructions
     */
    object_property_set_int(OBJECT(cpu), 1, "use-fpu", &error_abort);
    object_property_set_bool(OBJECT(cpu), true, "dcache-writeback",
                             &error_abort);
    object_property_set_bool(OBJECT(cpu), true, "endianness", &error_abort);
    object_property_set_bool(OBJECT(cpu), true, "realized", &error_abort);

    /* Attach emulated BRAM through the LMB.  */
    memory_region_init_ram(phys_lmb_bram, NULL, "sprite_engine.lmb_bram",
                           LMB_BRAM_SIZE, &error_fatal);
    vmstate_register_ram_global(phys_lmb_bram);
    memory_region_add_subregion(address_space_mem, 0x00000000, phys_lmb_bram);

    memory_region_init_ram(phys_ram, NULL, "sprite_engine.ram", ram_size,
                           &error_fatal);
    vmstate_register_ram_global(phys_ram);
    memory_region_add_subregion(address_space_mem, MEMORY_BASEADDR, phys_ram);

    dinfo = drive_get(IF_PFLASH, 0, 0);
    /* 5th parameter 2 means bank-width
     * 10th paremeter 0 means little-endian */
    pflash_cfi01_register(FLASH_BASEADDR,
                          NULL, "sprite_engine.flash", FLASH_SIZE,
                          dinfo ? blk_by_legacy_dinfo(dinfo) : NULL,
                          (64 * 1024), FLASH_SIZE >> 16,
                          2, 0x89, 0x18, 0x0000, 0x0, 0);


    dev = qdev_create(NULL, "xlnx.xps-intc");
    qdev_prop_set_uint32(dev, "kind-of-intr", 1 << TIMER_IRQ);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, INTC_BASEADDR);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(DEVICE(cpu), MB_CPU_IRQ));
    for (i = 0; i < 32; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    // create a uartlite
    sysbus_create_simple("xlnx.xps-uartlite", UARTLITE_BASEADDR,
                         irq[UARTLITE_IRQ]);

    // Create the vsync timer, and connect it to TIMER_IRQ
    dev = qdev_create(NULL, "sprite-engine.vsync");
    qdev_prop_set_uint32(dev, "clock-frequency", 60000);
    qdev_init_nofail(dev);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[TIMER_IRQ]);

    /* sprite engine init */
    dev = qdev_create(NULL, "sprite-engine");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, SPRITE_ENGINE_BASEADDR);

    /* sprite engine controllers init */
    dev = qdev_create(NULL, "sprite-engine-controller");
    qdev_prop_set_uint32(dev, "port", 1986);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, SPRITE_ENGINE_CONTROLLER_1);
    dev = qdev_create(NULL, "sprite-engine-controller");
    qdev_prop_set_uint32(dev, "port", 1987);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, SPRITE_ENGINE_CONTROLLER_2);
    dev = qdev_create(NULL, "sprite-engine-controller");
    qdev_prop_set_uint32(dev, "port", 1988);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, SPRITE_ENGINE_CONTROLLER_3);
    dev = qdev_create(NULL, "sprite-engine-controller");
    qdev_prop_set_uint32(dev, "port", 1989);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, SPRITE_ENGINE_CONTROLLER_4);

    /* setup PVR to match kernel settings */
    cpu->env.pvr.regs[4] = 0xc56b8000;
    cpu->env.pvr.regs[5] = 0xc56be000;
    cpu->env.pvr.regs[10] = 0x0e000000; /* virtex 6 */

    microblaze_load_kernel(cpu, MEMORY_BASEADDR, ram_size,
                           machine->initrd_filename,
                           BINARY_DEVICE_TREE_FILE,
                           NULL);

}

static void sprite_engine_machine_init(MachineClass *mc)
{
    mc->desc = "Sprite engine l33t ski775";
    mc->init = sprite_engine_init;
    mc->is_default = 0;
}

DEFINE_MACHINE("sprite-engine", sprite_engine_machine_init)
