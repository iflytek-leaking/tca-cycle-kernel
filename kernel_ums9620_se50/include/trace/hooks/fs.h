/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM fs

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_FS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_FS_H

#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>

#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_ANDROID_VENDOR_HOOKS)
struct file;
DECLARE_RESTRICTED_HOOK(android_vh_check_file_open,
	TP_PROTO(const struct file *file),
	TP_ARGS(file), 1);
#else
#define trace_android_vh_check_file_open(file)
#endif

#endif /* _TRACE_HOOK_FS_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
