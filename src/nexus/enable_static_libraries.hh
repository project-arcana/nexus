#pragma once

/// This header may be needed, if nexus TESTs or APPs are declared but fail to execute / do not show up in the test list:
/// If nexus is linked into a library and the library is linked into an application, the linker is allowed to strip TESTs/APPs from the executable, see:
/// https://stackoverflow.com/questions/76925604/why-is-the-constructor-of-a-global-variable-not-called-in-a-library
/// https://sillycross.github.io/2022/10/02/2022-10-02/
///
/// Example usage:
///
/// #include <nexus/run.hh>
/// #include <nexus/enable_static_libraries.hh>
/// int main(int argc, char** args)
/// {
///     NEXUS_ENABLE_STATIC_LIBRARIES();
///     return nx::run(argc, args);
/// }

#define NEXUS_IMPL_HAS_EXTERN_LIB_VAR

#ifdef NEXUS_IMPL_HAS_INLINE_LIB_VAR
    #error "This header and "nexus/test.hh"/"nexus/app.hh" must not be included in the same translation unit."
#endif

#define NEXUS_ENABLE_STATIC_LIBRARIES() nx::g_enable_static_libraries++

namespace nx
{
extern int g_enable_static_libraries;
}

