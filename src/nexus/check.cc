#include "check.hh"

// TODO: replace with proper log
#include <iostream>

#include <nexus/tests/Test.hh>

#include <clean-core/assert.hh>

bool nx::detail::report_failed_check(nx::detail::check_result const& r, const char* check, const char* file, int line, char const* function, bool terminate)
{
    auto t = nx::detail::get_current_test();
    CC_ASSERT(t != nullptr && "CHECK(...) is only valid inside tests");

    t->setFirstFailInfo(check, file, line, function);

    // log if not silenced
    if (!nx::detail::is_silenced())
    {
        std::cerr << "CHECK( " << check << " ) failed." << std::endl;

        if (!r.op)
            std::cerr << "  value: " << r.lhs.c_str() << std::endl;
        else
        {
            std::cerr << "  lhs: " << r.lhs.c_str() << std::endl;
            std::cerr << "  rhs: " << r.rhs.c_str() << std::endl;
        }

        std::cerr << "  at line " << file << ":" << line << std::endl;
        std::cerr << "  in test " << t->file() << ":" << t->line() << std::endl;
        if (t->functionName() != function) // TODO: properly?
            std::cerr << "  in function " << function << std::endl;
    }

    return terminate || nx::detail::always_terminate() || t->isDebug() || t->shouldReproduce();
}
