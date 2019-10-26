#include "Nexus.hh"

#include <nexus/tests/Test.hh>

#include <iostream>

void nx::Nexus::applyCmdArgs(int argc, char** argv)
{
    // TODO
}

int nx::Nexus::run()
{
    auto const& tests = detail::get_all_tests();

    std::cout << "[nexus] nexus version 0.0.1" << std::endl;
    std::cout << "[nexus] run with '--help' for options" << std::endl;
    std::cout << "[nexus] detected " << tests.size() << (tests.size() == 1 ? " test" : " tests") << std::endl;
    std::cout << "=======================================================" << std::endl;
    // TODO

    return 0;
}
