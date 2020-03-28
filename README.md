# nexus

C++ library for tests (unit, fuzz, property), benchmarks, apps

## Usage

### Tests

```cpp
#include <nexus/test.hh>

TEST("my test")
{
    CHECK(1 + 2 == 3);
}
```


### Fuzz tests

Fuzz tests are randomized tests, per default called until a small time budget is exhausted.
If they fail, they provide information how to reproduce them.

```cpp
#include <nexus/fuzz_test.hh>

FUZZ_TEST("my fuzz test")(tg::rng& rng)
{
    auto i = uniform(rng, 0, 10);
    CHECK(i >= 0);
}
```


### Monte Carlo Tests (MCT)

Monte carlo tests are super-powered property tests where you define a set of operations (functions), optionally with some invariants and the framework executes random combinations of operations.

```cpp
#include <nexus/monte_carlo_test.hh>

MONTE_CARLO_TEST("my mct test")
{
    addOp("gen", [](tg::rng& rng) { return uniform(rng, -10, 10) * 2; }
    addOp("add", [](int a, int b) { return a + b; });
    addOp("sub", [](int a, int b) { return a - b; });
    addInvariant("mod 2", [](int i) { CHECK(i % 2 == 0); });
}
```

TODO: write an in-depth guide to MCT tests.


### Options

Options can be passed after the name: `TEST("name", optA, optB, ...)`.
Supported options (can be found in `<nexus/config.hh>`):

* `before("A")` - executes this test before test `A`
* `after("A")` - executes this test after test `A`
* `exclusive` - no other test is executed concurrently (for multi-threaded test execution)
* `should_fail` - this test passes if at least one check fails
* `disabled` - this test is not run by default (only if exactly called by name)

For fuzz and monte carlo tests:

* `seed` - customizes the seed used in this test
* `endless` - repeats this test endlessly
* `debug` - thrown errors are not caught (makes it easier to debug fuzz and monte carlo tesets)
* `verbose` - increases verbosity of test outputs and can be useful for debugging (especially endless loops in a MCT)
* `reproduce(...)` - reproduces a specific test case (exact option is printed when a fuzz or monte carlo tests fails)


### Command Line Args

The simplest way to start nexus is:

```cpp
#include <nexus/run.hh>

int main(int argc, char** argv)
{
    return nx::run(argc, argv);
}
```

Currently, very little command line args are supported.
Most importantly:

`~ test-name` 

This only executes the given test (exact match).
For tests containing spaces, use `~ "my test"`.
Apps or disabled tests can also be executed this way.


### Apps

Nexus apps are a convenient way to have small executable sub-programs in your binary (including argument parsing).

```cpp
#include <nexus/app.hh>

APP("myapp")
{
    // simple built-in argparse library
    int n = 10;
    float t = 1.0f;
    auto args = nx::args("My App", "just a simple app")
        .version("0.0.1-alpha")
        .add("b", "some bool")
        .add({"f", "flag"}, "some flag")
        .group("vars")
        .add(n, "n", "iterations")
        .add(t, {"t", "time"}, "time parameter");

    if (!args.parse()) // takes arguments from app
        return;

    auto b = args.has("b");

    .. do something
}
```

This app can be called with `~ myapp` (and help via `~ myapp -h`).
