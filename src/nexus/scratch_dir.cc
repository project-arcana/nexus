#include "scratch_dir.hh"

#include <cstdio>

#include <clean-core/macros.hh>

#ifdef CC_OS_WINDOWS

// clang-format off
#include <clean-core/native/detail/win32_sanitize_before.inl>

#include <Windows.h>
#include <ShlObj_core.h>
#include <shellapi.h>

#include <clean-core/native/detail/win32_sanitize_after.inl>
// clang-format on

#define NX_OS_PATH_SEPARATOR "\\"

#elif defined(CC_OS_LINUX)

#include <cstdlib>

#define NX_OS_PATH_SEPARATOR "/"

#endif

#include <nexus/apps/App.hh>
#include <nexus/tests/Test.hh>

namespace
{
#ifdef CC_OS_WINDOWS

/// recursively deletes a directory, path must be double nullterminated
bool delete_directory(char const* path)
{
    SHFILEOPSTRUCT file_op = {nullptr, FO_DELETE, path, nullptr, FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT, false, nullptr, nullptr};
    int err_del = SHFileOperation(&file_op);
    return (err_del == 0);
}

#elif defined(CC_OS_LINUX)

#endif


struct directory_prefix
{
    char buf[32] = {0};

    // filters letters (a-z, A-Z) from the filter string to
    // build a folder prefix that can be used in a folder name
    void initialize(char const* filter_string)
    {
        auto cursor = 0u;
        while (*filter_string && cursor < sizeof(buf) + 1)
        {
            char c = *filter_string++;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
            {
                buf[cursor++] = c;
            }
        }
        buf[cursor] = '\0';
    }
};
}

bool nx::get_system_temp_directory(cc::span<char> out_path)
{
#ifdef CC_OS_WINDOWS
    DWORD const res = GetTempPathA(DWORD(out_path.size_bytes()), out_path.data());
    if (res > out_path.size_bytes())
    {
        // buffer wasn't large enough
        return false;
    }

    return true;

#elif defined(CC_OS_LINUX)
    char const* tmpdir_env = std::getenv("TMPDIR");
    if (!tmpdir_env)
    {
        tmpdir_env = "/tmp/";
    }

    std::strncpy(out_path.data(), tmpdir_env, out_path.size());
    if (out_path.back() != '\0')
    {
        // buffer wasn't large enough
        return false;
    }

    return true;
#else
    std::fprintf(stderr, "[nexus] error: nx::get_system_temp_directory() not supported on this platform\n");
    return false;
#endif
}

cc::string nx::open_scratch_directory()
{
    char os_tempdir_buf[512];
    if (!get_system_temp_directory(os_tempdir_buf))
    {
        std::fprintf(stderr, "[nexus] error: failed to receive OS temporary directory\n");
        return {};
    }

    void const* app_or_test_ptr = nullptr; // format the address of the app/test for a cheap GUID

    directory_prefix prefix;

    if (Test const* const curr_test = detail::get_current_test())
    {
        app_or_test_ptr = curr_test;
        prefix.initialize(curr_test->name());
    }
    else if (App const* const curr_app = detail::get_current_app())
    {
        app_or_test_ptr = curr_app;
        prefix.initialize(curr_app->name());
    }
    else
    {
        std::fprintf(stderr, "[nexus] warning: nx::open_temp_folder() was called outside of an active app or test\n");
        return {};
    }

    char full_path[1024];
    int num_printed = std::snprintf(full_path, sizeof(full_path) - 1, "%sarcana-nexus" NX_OS_PATH_SEPARATOR "tmpdata_%s" NX_OS_PATH_SEPARATOR, //
                                    os_tempdir_buf, prefix.buf);
    CC_ASSERT(num_printed < int(sizeof(full_path) - 1) && "temporary path truncated");
    full_path[num_printed + 1] = '\0'; // Win32 APIs require double null-terminated strings


    // recursively create directory
#ifdef CC_OS_WINDOWS

    // unlike similar functions this creates folders recursively
    auto const res = ::SHCreateDirectoryExA(nullptr, full_path, nullptr);
    if (res == ERROR_SUCCESS)
    {
        // folder didn't exist before
    }
    else if (res == ERROR_FILE_EXISTS || res == ERROR_ALREADY_EXISTS)
    {
        // folder already exists, delete and recreate
        if (!delete_directory(full_path))
        {
            std::fprintf(stderr, "[nexus] error: nx::open_temp_folder() failed to delete directory \"%s\"\n", full_path);
            return {};
        }

        auto const res2 = ::SHCreateDirectoryExA(nullptr, full_path, nullptr);
        if (res2 != ERROR_SUCCESS)
        {
            std::fprintf(stderr, "[nexus] error: nx::open_temp_folder() failed to re-create directory \"%s\"\n", full_path);
            return {};
        }
    }
    else
    {
        // other error
        std::fprintf(stderr, "[nexus] error: nx::open_temp_folder() failed to create directory \"%s\"\n", full_path);
        return {};
    }
#elif defined(CC_OS_LINUX)
    // TODO
    std::fprintf(stderr, "[nexus] error: nx::open_temp_folder() not supported on this platform\n");
    return {};
#else
    std::fprintf(stderr, "[nexus] error: nx::open_temp_folder() not supported on this platform\n");
    return {};
#endif

    return cc::string(static_cast<char const*>(full_path));
}
