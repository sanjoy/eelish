#include "tests.hpp"

#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>

using namespace eelish;
using namespace std;

void ThreadedTest::output(const char *format, ...) {
  if (!quiet_) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
  }
}

void ThreadedTest::always_output(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

void CommandLine::Parse(map<string, CommandLine::Arg> *meta,
                        int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strstr(argv[i], "--") != argv[i]) continue;

    bool was_negation = false;
    char *arg_name = strstr(argv[i] + 2, "no-");
    if (arg_name == NULL) {
      arg_name = argv[i] + 2;
    } else {
      arg_name = argv[i] + 5;
      was_negation = true;
    }

    string str_arg(arg_name);
    if (meta->find(str_arg) == meta->end()) continue;

    CommandLine::Arg info = (*meta)[str_arg];
    switch(info.type) {
      case CommandLine::BOOL:
        info.boolean = !was_negation;
        break;
      case CommandLine::INTEGER:
        if (++i < argc) info.integer = atoi(argv[i]);
        break;
      case CommandLine::STRING:
        if (++i < argc) info.string = argv[i];
        break;
    }
    (*meta)[str_arg] = info;
  }
}
