#include "check.hh"

// TODO: replace with proper log
#include <iostream>

#include <nexus/tests/Test.hh>

#include <clean-core/assert.hh>
#include <clean-core/string_view.hh>

#include <rich-log/logger.hh>

bool nx::detail::report_failed_check(nx::detail::check_result const& r, const char* check, const char* file, int line, char const* function, bool terminate)
{
    auto t = nx::detail::get_current_test();
    CC_ASSERT(t != nullptr && "CHECK(...) is only valid inside tests");

    t->setFirstFailInfo(check, file, line, function);

    {
        cc::string expanded;
        if (r.op)
            expanded = r.lhs + " " + r.op + " " + r.rhs;
        else
            expanded = r.lhs;
        t->addFailedCheck(check, cc::move(expanded), file, line);
    }

    // log if not silenced
    if (!nx::detail::is_silenced())
    {
        std::cerr << "CHECK( " << check << " ) failed." << std::endl;

        if (!r.op)
            std::cerr << "  value: " << r.lhs.c_str() << std::endl;
        else
        {
            // diff view for "=="
            if (cc::string_view(r.op) == "==")
            {
                auto lhs = cc::string_view(r.lhs);
                auto rhs = cc::string_view(r.rhs);

                size_t s_start = 0;
                while (s_start < lhs.size() && s_start < rhs.size() && lhs[s_start] == rhs[s_start])
                    ++s_start;

                size_t s_end = 0;
                while (s_start + s_end < lhs.size() && s_start + s_end < rhs.size() && lhs[lhs.size() - s_end - 1] == rhs[rhs.size() - s_end - 1])
                    ++s_end;

                auto lhs_pre = cc::string(lhs.subview(0, s_start));
                auto lhs_mid = cc::string(lhs.subview(s_start, lhs.size() - s_start - s_end));
                auto lhs_end = cc::string(lhs.subview(lhs.size() - s_end));

                auto rhs_pre = cc::string(rhs.subview(0, s_start));
                auto rhs_mid = cc::string(rhs.subview(s_start, rhs.size() - s_start - s_end));
                auto rhs_end = cc::string(rhs.subview(rhs.size() - s_end));

                auto const col_reset = rlog::colors_enabled() ? "\u001b[0m" : "";
                auto const col_gray = rlog::colors_enabled() ? "\u001b[38;5;244m" : "";
                std::cerr << "  lhs: " << col_gray << lhs_pre.c_str() << col_reset << lhs_mid.c_str() << col_gray
                          << lhs_end.c_str() << col_reset << std::endl;
                std::cerr << "  rhs: " << col_gray << rhs_pre.c_str() << col_reset << rhs_mid.c_str() << col_gray
                          << rhs_end.c_str() << col_reset << std::endl;

                if (lhs == rhs)
                    std::cout << "  (NOTE: lhs and rhs string representation is identical, but 'lhs == rhs' still evaluates to 'false')" << std::endl;
            }
            else
            {
                std::cerr << "  lhs: " << r.lhs.c_str() << std::endl;
                std::cerr << "  rhs: " << r.rhs.c_str() << std::endl;
            }
        }

        std::cerr << "  at line " << file << ":" << line << std::endl;
        std::cerr << "  in test " << t->file() << ":" << t->line() << std::endl;
        if (t->functionName() != function) // TODO: properly?
            std::cerr << "  in function " << function << std::endl;
    }

    return terminate || nx::detail::always_terminate() || t->isDebug() || t->shouldReproduce();
}
