#define _GNU_SOURCE // Required for some environments like older glibc versions
                    // or Windows

#include "kenzutls.h"
#include <string.h>

void remove_trailing_newline(char *str) {
  if (str == NULL)
    return;
  str[strcspn(str, "\n")] = 0;
}
