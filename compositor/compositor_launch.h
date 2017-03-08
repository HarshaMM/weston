/**
* \file: compositor_launch.h
*
* \version: $Id:$
*
* \release: $Name:$
*
* This header exports interface to start compositor in the same process.
* Also exported is the interface to load proprietary weston modules
*
* \component: ivi-share
*
* \author: Harsha MM <harsha.mm@de.adit-jv.com>
*
* \copyright (c) 2017 Advanced Driver Information Technology.
* This code is developed by Advanced Driver Information Technology.
* Copyright of Advanced Driver Information Technology, Bosch, and DENSO.
* All rights reserved.
*
***********************************************************************/
#ifndef _COMPOSITOR_LAUNCH_H
#define _COMPOSITOR_LAUNCH_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdarg.h>

int
compositor_init(int argc, char *argv[],
		int (*log_func)(const char *fmt, va_list ap),
		int (*log_func_continue)(const char *fmt, va_list ap));


int
compositor_loadmodule(int comp_handle, const char* module_name,
        int argc, char *argv[]);

const void *
compositor_get_module_interface(int comp_handle, const char* module_name,
		const char *lookup_name, size_t interface_size);

void *
compositor_get_display(int comp_handle);

int
compositor_run(int comp_handle);

int
compositor_shutdown(int comp_handle);

void *
compositor_get_weston_compositor(int comp_handle);

int
compositor_sigchld_handler(pid_t pid, int status);

#ifdef __cplusplus
} /**/
#endif /* __cplusplus */

#endif
