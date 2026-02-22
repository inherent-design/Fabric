// MimallocOverride.cc
// Force-reference mimalloc so the linker pulls in malloc/free/new/delete
// overrides from mimalloc-static (built with MI_OVERRIDE=ON).
// Without this TU, the static library's symbols may be dead-stripped.
#include <mimalloc.h>
