#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

bool do_system(const char *command);

bool do_exec(int count, ...);

bool do_exec_redirect(const char *outputfile, int count, ...);
