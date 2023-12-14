#pragma once

#define NEXUS_IMPL_HAS_INLINE_LIB_VAR
namespace nx
{
inline int g_enable_static_libraries = 0;
}
#ifdef NEXUS_IMPL_HAS_EXTERN_LIB_VAR
#error "This header and "nexus/enable_shared_libraries.hh" must not be included in the same translation unit."
#endif
