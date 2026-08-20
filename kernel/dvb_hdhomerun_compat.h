/*
 * dvb_hdhomerun_compat.h, compat for various kernel versions
 *
 * Copyright (C) 2010 Villy Thomsen <tfylliv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef __DVB_HDHOMERUN_COMPAT_H__
#define __DVB_HDHOMERUN_COMPAT_H__

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
#define my_kfifo_len __kfifo_len
#define my_kfifo_put kfifo_put
#define my_kfifo_get kfifo_get
#else
#define my_kfifo_len kfifo_len
#define my_kfifo_get kfifo_out
#define my_kfifo_put kfifo_in
#endif

/*
 * class_create() dropped the owner argument in 6.4 (Debian Trixie,
 * Ubuntu 24.04+). Keep a 2-arg wrapper for Bookworm / Ubuntu 22.04.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
#define hdhomerun_class_create(name) class_create(name)
#else
#define hdhomerun_class_create(name) class_create(THIS_MODULE, name)
#endif

/*
 * platform_driver.remove:
 *  < 6.11  : int  (*remove)(struct platform_device *)
 *  6.11    : void (*remove_new)(...); int (*remove)(...) still present
 *  >= 6.12 : void (*remove)(...); remove_new removed
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
#define HDHR_REMOVE_RET void
#define HDHR_REMOVE_RETURN return
#else
#define HDHR_REMOVE_RET int
#define HDHR_REMOVE_RETURN return 0
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0) && \
    LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
#define HDHR_PLATFORM_REMOVE_CB remove_new
#else
#define HDHR_PLATFORM_REMOVE_CB remove
#endif

/*
 * DVB frontend frequency fields were renamed to *_hz in 5.18.
 * .type was removed at the same time.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
#define HDHR_FE_FREQ(_step, _min, _max) \
	.frequency_stepsize_hz = (_step), \
	.frequency_min_hz = (_min), \
	.frequency_max_hz = (_max)
#else
#define HDHR_FE_FREQ(_step, _min, _max) \
	.frequency_stepsize = (_step), \
	.frequency_min = (_min), \
	.frequency_max = (_max)
#endif

#endif /* __DVB_HDHOMERUN_COMPAT_H__ */
