#include "check.hh"

// TODO: replace with proper log
#include <iostream>

void nx::detail::report_failed_check(nx::detail::check_result const& r, const char* check, const char* file, int line, bool terminate)
{
    std::cerr << "CHECK( " << check << " ) failed." << std::endl;
    std::cerr << "  in " << file << ":" << line << std::endl;
    if (r.lhs && !r.rhs)
        std::cout << "  value: " << r.lhs << std::endl;
    else if (r.lhs && r.rhs)
    {
        std::cout << "  lhs: " << r.lhs << std::endl;
        std::cout << "  rhs: " << r.rhs << std::endl;
    }

    // TODO: delete r.lhs and r.rhs
}
