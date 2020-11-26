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
#include <cstring>
#include <sys/stat.h>
#include <ftw.h>
#include <unistd.h>

#define NX_OS_PATH_SEPARATOR "/"

#endif

#include <nexus/apps/App.hh>
#include <nexus/tests/Test.hh>

namespace
{
enum class CreateDirResult
{
    Success,
    SuccessAlreadyExists,
    Error
};

#ifdef CC_OS_WINDOWS

/// recursively deletes a directory, path must be double null-terminated
bool delete_directory(char const* path)
{
    SHFILEOPSTRUCT file_op = {nullptr, FO_DELETE, path, nullptr, FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT, false, nullptr, nullptr};
    int err_del = SHFileOperation(&file_op);
    return (err_del == 0);
}

CreateDirResult create_directory_recursively(char const* path)
{
    // unlike similar functions this creates folders recursively
    auto const res = ::SHCreateDirectoryExA(nullptr, path, nullptr);

    switch (res)
    {
    case ERROR_SUCCESS:
        return CreateDirResult::Success;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return CreateDirResult::SuccessAlreadyExists;
    default:
        return CreateDirResult::Error;
    }
}

#elif defined(CC_OS_LINUX)


CreateDirResult do_mkdir(char const* path, ::mode_t mode)
{
    struct ::stat st = {};
    if (::stat(path, &st) == 0)
    {
        // exists

        if (!S_ISDIR(st.st_mode))
        {
            // .. but is not a directory
            return CreateDirResult::Error;
        }

        return CreateDirResult::SuccessAlreadyExists;
    }
    else
    {
        // does not exist, create
        auto const mkdir_res = ::mkdir(path, mode);

        if (mkdir_res != 0)
            return CreateDirResult::Error;

        return CreateDirResult::Success;
    }
}

CreateDirResult create_directory_recursively(char const* path)
{
    // modified from mkpath, https://github.com/jleffler/soq/blob/master/src/so-0067-5039/mkpath.c
    // Licensed CC SA 3.0, Jonathan Leffler
    /**
    ** mkpath - ensure all directories in path exist
    ** Algorithm takes the pessimistic view and works top-down to ensure
    ** each directory in path exists, rather than optimistically creating
    ** the last element and working backwards.
    */

    ::mode_t const mode = 0700;
    char* pp;
    char* sp;
    CreateDirResult status = CreateDirResult::Success;
    char* copypath = ::strdup(path);

    pp = copypath;
    while (status != CreateDirResult::Error && (sp = std::strchr(pp, '/')) != 0)
    {
        if (sp != pp)
        {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = do_mkdir(copypath, mode);
            *sp = '/';
        }
        pp = sp + 1;
    }

    if (status != CreateDirResult::Error)
        status = do_mkdir(path, mode);

    ::free(copypath);
    return status;
}

int unlink_cb(char const* fpath, struct ::stat const* /*sb*/, int /*typeflag*/, struct ::FTW* /*ftwbuf*/)
{
    int const error_code = ::remove(fpath);

    if (error_code)
    {
        std::fprintf(stderr, "[nexus] warning: failed to remove temporary file %s\n", fpath);
    }

    return error_code;
}

/// recursively deletes a directory
bool delete_directory(char const* path)
{
    // FTW: file tree walk, FTW_DEPTH: depth first traversal
    // callback (unlink_cb) removes each file
    // returns the first nonzero result of the callback, or -1 on other errors
    // result 0: all is good
    int const res = ::nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);

    return res == 0;
}

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

bool nx::read_system_temp_path(cc::span<char> out_path)
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
    if (!read_system_temp_path(os_tempdir_buf))
    {
        std::fprintf(stderr, "[nexus] error: failed to receive OS temporary directory\n");
        return {};
    }

    void const* app_or_test_ptr = nullptr;
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
    CreateDirResult res = create_directory_recursively(full_path);
    if (res == CreateDirResult::Success)
    {
        // folder didn't exist before
    }
    else if (res == CreateDirResult::SuccessAlreadyExists)
    {
        // folder already exists, delete and recreate

        if (!delete_directory(full_path))
        {
            std::fprintf(stderr, "[nexus] error: nx::open_temp_folder() failed to delete directory \"%s\"\n", full_path);
            return {};
        }

        res = create_directory_recursively(full_path);
        if (res == CreateDirResult::Error)
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

    return cc::string(static_cast<char const*>(full_path));
}
