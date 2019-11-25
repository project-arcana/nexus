#include "Test.hh"

nx::Test::Test(const char* name, const char* file, int line, const char* fun_name, nx::test_fun_t fun)
  : mName(name), mFile(file), mLine(line), mFunctionName(fun_name), mFunction(fun)
{
}
