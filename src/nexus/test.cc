#include "test.hh"

#include <nexus/tests/Test.hh>

#include <clean-core/unique_ptr.hh>
#include <clean-core/vector.hh>

using namespace nx;

static cc::vector<cc::unique_ptr<Test>> sAllTests;

cc::vector<cc::unique_ptr<Test>> const& nx::detail::get_all_tests() { return sAllTests; }

void nx::detail::configure(Test* t, before const& v) { t->addBeforePattern(v.pattern); }

void nx::detail::configure(Test* t, after const& v) { t->addAfterPattern(v.pattern); }

void nx::detail::configure(Test* t, exclusive_t const&) { t->setExclusive(); }

Test* detail::register_test(const char* name, const char* file, int line, test_fun_t fun)
{
    auto t = cc::make_unique<Test>(name, file, line, fun);
    auto t_ptr = t.get();
    sAllTests.push_back(std::move(t));
    return t_ptr;
}
