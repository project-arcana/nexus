#include "App.hh"

nx::App::App(const char* name, const char* file, int line, const char* fun_name, nx::app_fun_t fun)
  : mName(name), mFile(file), mLine(line), mFunctionName(fun_name), mFunction(fun)
{
}
