#include "test.hh"

#include <rich-log/log.hh>

#include <nexus/detail/log.hh>
#include <nexus/tests/Test.hh>

#include <clean-core/unique_ptr.hh>
#include <clean-core/vector.hh>

using namespace nx;

cc::vector<cc::unique_ptr<Test>>& nx::detail::get_all_tests()
{
    static cc::vector<cc::unique_ptr<Test>> all_tests;
    return all_tests;
}

void nx::detail::configure(Test* t, before const& v) { t->addBeforePattern(v.pattern); }

void nx::detail::configure(Test* t, after const& v) { t->addAfterPattern(v.pattern); }

void nx::detail::configure(Test* t, exclusive_t const&) { t->setExclusive(); }

void nx::detail::configure(Test* t, should_fail_t const&) { t->setShouldFail(); }

Test* detail::register_test(const char* name, const char* file, int line, char const* fun_name, test_fun_t fun, test_fun_before_t fun_before, test_fun_after_t fun_after, local_check_counters* counters)
{
    auto t = cc::make_unique<Test>(name, file, line, fun_name, fun, fun_before, fun_after, counters);
    auto t_ptr = t.get();
    get_all_tests().push_back(std::move(t));
    return t_ptr;
}

void detail::configure(Test* t, const seed& v) { t->overwriteSeed(v.value); }

size_t nx::get_seed()
{
    auto t = nx::detail::get_current_test();
    CC_ASSERT(t && "no active test");
    return t->seed();
}

bool nx::is_current_test_debug()
{
    auto t = nx::detail::get_current_test();
    CC_ASSERT(t && "no active test");
    return t->isDebug();
}

bool nx::is_current_test_reproduction()
{
    auto t = nx::detail::get_current_test();
    CC_ASSERT(t && "no active test");
    return t->shouldReproduce();
}

void detail::configure(Test* t, const endless_t&) { t->setEndless(); }

void detail::configure(Test* t, const reproduce& r) { t->setReproduce(r); }

void detail::configure(Test* t, const disabled_t&) { t->setDisabled(); }

void detail::configure(Test* t, const debug_t&) { t->setDebug(); }

void detail::configure(Test* t, const verbose_t&) { t->setVerbose(); }

void detail::configure(Test* t, const opt_in_group& g) { t->addOptInGroup(g.name); }

void nx::print_current_test_reproduction()
{
    auto t = nx::detail::get_current_test();
    CC_ASSERT(t && "no active test");
    RICH_LOG("%s", t->makeCurrentReproductionCommand());
}
