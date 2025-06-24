#pragma once

#ifdef ENABLE_VTUNE
#include <ittnotify.h>

static __itt_domain* vtune_domain = __itt_domain_create("Benchmarks");

inline void vtune_start_task(const char* name) {
    __itt_string_handle* handle = __itt_string_handle_create(name);
    __itt_task_begin(vtune_domain, __itt_null, __itt_null, handle);
}

inline void vtune_end_task() {
    __itt_task_end(vtune_domain);
}

#else

// Fallbacks when VTune is disabled
inline void vtune_start_task(const char*) {}
inline void vtune_end_task() {}

#endif
