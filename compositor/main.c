/*
 * Copyright © 2010-2011 Intel Corporation
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2012-2015 Collabora, Ltd.
 * Copyright © 2010-2011 Benjamin Franzke
 * Copyright © 2013 Jason Ekstrand
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "config.h"

#include <wayland-server-protocol.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <compositor.h>
#include <dlfcn.h>
#include <execinfo.h>

#ifdef HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include "compositor_launch.h"
#include "../shared/helpers.h"

static struct weston_compositor *segv_compositor;

#ifdef HAVE_LIBUNWIND

static void
print_backtrace(void)
{
	unw_cursor_t cursor;
	unw_context_t context;
	unw_word_t off;
	unw_proc_info_t pip;
	int ret, i = 0;
	char procname[256];
	const char *filename;
	Dl_info dlinfo;

	pip.unwind_info = NULL;
	ret = unw_getcontext(&context);
	if (ret) {
		weston_log("unw_getcontext: %d\n", ret);
		return;
	}

	ret = unw_init_local(&cursor, &context);
	if (ret) {
		weston_log("unw_init_local: %d\n", ret);
		return;
	}

	ret = unw_step(&cursor);
	while (ret > 0) {
		ret = unw_get_proc_info(&cursor, &pip);
		if (ret) {
			weston_log("unw_get_proc_info: %d\n", ret);
			break;
		}

		ret = unw_get_proc_name(&cursor, procname, 256, &off);
		if (ret && ret != -UNW_ENOMEM) {
			if (ret != -UNW_EUNSPEC)
				weston_log("unw_get_proc_name: %d\n", ret);
			procname[0] = '?';
			procname[1] = 0;
		}

		if (dladdr((void *)(pip.start_ip + off), &dlinfo) && dlinfo.dli_fname &&
		    *dlinfo.dli_fname)
			filename = dlinfo.dli_fname;
		else
			filename = "?";

		weston_log("%u: %s (%s%s+0x%x) [%p]\n", i++, filename, procname,
			   ret == -UNW_ENOMEM ? "..." : "", (int)off, (void *)(pip.start_ip + off));

		ret = unw_step(&cursor);
		if (ret < 0)
			weston_log("unw_step: %d\n", ret);
	}
}

#else

static void
print_backtrace(void)
{
	void *buffer[32];
	int i, count;
	Dl_info info;

	count = backtrace(buffer, ARRAY_LENGTH(buffer));
	for (i = 0; i < count; i++) {
		dladdr(buffer[i], &info);
		weston_log("  [%016lx]  %s  (%s)\n",
			(long) buffer[i],
			info.dli_sname ? info.dli_sname : "--",
			info.dli_fname);
	}
}

#endif

static int
sigchld_handler(int signal_number, void *data)
{
	int status;
	pid_t pid;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		compositor_sigchld_handler(pid, status);
	}

	if (pid < 0 && errno != ECHILD)
		weston_log("waitpid error %m\n");

	return 1;
}


static int on_term_signal(int signal_number, void *data)
{
	struct wl_display *display = data;

	weston_log("caught signal %d\n", signal_number);
	wl_display_terminate(display);

	return 1;
}

static void
on_caught_signal(int s, siginfo_t *siginfo, void *context)
{
	/* This signal handler will do a best-effort backtrace, and
	 * then call the backend restore function, which will switch
	 * back to the vt we launched from or ungrab X etc and then
	 * raise SIGTRAP.  If we run weston under gdb from X or a
	 * different vt, and tell gdb "handle *s* nostop", this
	 * will allow weston to switch back to gdb on crash and then
	 * gdb will catch the crash with SIGTRAP.*/

	weston_log("caught signal: %d\n", s);

	print_backtrace();

	segv_compositor->backend->restore(segv_compositor);

	raise(SIGTRAP);
}

static void
catch_signals(void)
{
	struct sigaction action;

	action.sa_flags = SA_SIGINFO | SA_RESETHAND;
	action.sa_sigaction = on_caught_signal;
	sigemptyset(&action.sa_mask);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGABRT, &action, NULL);
}

int main(int argc, char *argv[])
{
	int ret = -EINVAL;
	int comp_obj;
	int i;
	struct wl_display *display;
	struct wl_event_source *signals[4];
	struct wl_event_loop *loop;


	comp_obj = compositor_init(argc, argv, NULL, NULL);
	if (comp_obj <= 0) {
		weston_log("compositor_init failed with error: %d", comp_obj);
		return ret;
	}

	display = compositor_get_display(comp_obj);
	loop = wl_display_get_event_loop(display);
	signals[0] = wl_event_loop_add_signal(loop, SIGTERM, on_term_signal,
					      display);
	signals[1] = wl_event_loop_add_signal(loop, SIGINT, on_term_signal,
					      display);
	signals[2] = wl_event_loop_add_signal(loop, SIGQUIT, on_term_signal,
					      display);
	signals[3] = wl_event_loop_add_signal(loop, SIGCHLD, sigchld_handler,
					      NULL);

	if (!signals[0] || !signals[1] || !signals[2] || !signals[3]) {
		ret = -ENOMEM;
		goto out;
	}

	catch_signals();
	segv_compositor = (struct weston_compositor *)
							compositor_get_weston_compositor(comp_obj);

	compositor_run(comp_obj);

	for (i = ARRAY_LENGTH(signals) - 1; i >= 0; i--)
		if (signals[i])
			wl_event_source_remove(signals[i]);

out:
	ret = compositor_shutdown(comp_obj);

	return ret;
}
