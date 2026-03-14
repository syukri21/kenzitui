#define _GNU_SOURCE // Required for some environments like older glibc versions
                    // or Windows

#include "kenzutls.h"
#include <stdio.h>
#include <string.h>

void remove_trailing_newline(char *str) {
  if (str == NULL) {
    return;
  }

  size_t len = strlen(str);
  while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
    str[len - 1] = '\0';
    len--;
  }
}

int shell_quote_single(const char *src, char *out, size_t out_size) {
  if (src == NULL || out == NULL || out_size < 3) {
    return 0;
  }

  size_t w = 0;
  out[w++] = '\'';
  for (size_t i = 0; src[i] != '\0'; i++) {
    if (src[i] == '\'') {
      if (w + 4 >= out_size) {
        return 0;
      }
      out[w++] = '\'';
      out[w++] = '\\';
      out[w++] = '\'';
      out[w++] = '\'';
      continue;
    }
    if (w + 1 >= out_size) {
      return 0;
    }
    out[w++] = src[i];
  }
  if (w + 1 >= out_size) {
    return 0;
  }
  out[w++] = '\'';
  out[w] = '\0';
  return 1;
}

int copy_file_binary(const char *src, const char *dst) {
  if (src == NULL || dst == NULL) {
    return 0;
  }

  FILE *in = fopen(src, "rb");
  if (in == NULL) {
    return 0;
  }
  FILE *out = fopen(dst, "wb");
  if (out == NULL) {
    fclose(in);
    return 0;
  }

  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      fclose(in);
      fclose(out);
      return 0;
    }
  }

  fclose(in);
  fclose(out);
  return 1;
}
