/*************************************************
 * this file is auto generated by gen-entry.sh
 * do not edit !
 *
 * Tue Jul 28 11:35:41 CST 2015
 ************************************************/

#include <asm-arm/io.h>
#include "preloader.h"

#define __F(x) (void *)(x)

struct irom_export irf_4e5a75 = {

    .init_timer          = __F(0x00008808),
    .init_serial         = __F(0x00008654),
    .printf              = __F(0x0000c1d0),
    .udelay              = __F(0x000086f8),
    .memcpy              = __F(0x000011c8),
    .strnlen             = __F(0x00001180),
    .sprintf             = __F(0x0000c018),
    .vs_reset            = __F(0x00004898),
    .vs_read             = __F(0x00004dec),
    .vs_write            = __F(0x000048d4),
    .vs_erase            = __F(0x0000492c),
    .vs_isbad            = __F(0x00004968),
    .vs_scrub            = __F(0x000049a4),
    .vs_special          = __F(0x000049e0),
    .vs_assign_by_name   = __F(0x00004ce8),
    .vs_assign_by_id     = __F(0x00004a78),
    .vs_device_id        = __F(0x00004c98),
    .vs_is_assigned      = __F(0x000047f4),
    .vs_align            = __F(0x00004a1c),
    .vs_is_device        = __F(0x00004d88),
    .vs_is_opt_available = __F(0x0000480c),
    .vs_device_name      = __F(0x00004c0c),
    .boot_device         = __F(0x000005c8),
    .isi_check_header    = __F(0x00001768),
    .cdbg_shell          = __F(0x00005f74),
    .simple_strtol       = __F(0x0000b2a0),
    .strncmp             = __F(0x000010e4),
    .strncpy             = __F(0x00001020),
    .cdbg_log_toggle     = __F(0x00005e10),
    .module_set_clock    = __F(0x000006dc),
    .module_get_clock    = __F(0x0000ad40),
    .module_enable       = __F(0x00000708),
    .set_pll             = __F(0x000005e4),
    .memset              = __F(0x000011a8),
    .ss_jtag_en          = __F(0x0000e1b0),
    .malloc              = __F(0x0000135c),
    .free                = __F(0x000014fc),
    .pads_chmod          = __F(0x00000c0c),
    .pads_pull           = __F(0x00000d3c),
    .get_xom             = __F(0x00000550),
    .hash_init           = __F(0x0000552c),
    .hash_data           = __F(0x00005498),
    .hash_value          = __F(0x00005428),
};

