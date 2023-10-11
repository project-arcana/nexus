#include "assertions.hh"

#include <cstdio> // TODO: replace with log

#include <clean-core/assert.hh>

#ifdef NX_HAS_POLYMESH
#include <polymesh/assert.hh>
#endif

#include <nexus/check.hh>
#include <nexus/detail/exception.hh>
#include <nexus/tests/Test.hh>

void nx::detail::overwrite_assertion_handlers()
{
    auto handler = [](auto const& info)
    {
        nx::detail::number_of_assertions()++;
        nx::detail::number_of_failed_assertions()++;

        if (auto t = nx::detail::get_current_test())
            t->setFirstFailInfo(info.expr, info.file, info.line, info.func);

        if (!nx::detail::is_silenced())
        {
            fflush(stdout);

            fprintf(stderr, "assertion `%s' failed.\n", info.expr);
            if constexpr (std::is_same_v<std::decay_t<decltype(info)>, cc::detail::assertion_info>)
                fprintf(stderr, "  ---\n%s\n  ---\n", info.msg);
            fprintf(stderr, "  in %s\n", info.func);
            fprintf(stderr, "  file %s:%d\n", info.file, info.line);
            fflush(stderr);
        }

        throw assertion_failed_exception();
    };
    cc::set_assertion_handler(handler);
#ifdef NX_HAS_POLYMESH
    polymesh ::set_assertion_handler(handler);
#endif
}

void nx::detail::reset_assertion_handlers()
{
    cc::set_assertion_handler(nullptr);
#ifdef NX_HAS_POLYMESH
    polymesh ::set_assertion_handler(nullptr);
#endif
}
