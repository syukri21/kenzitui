#ifndef KENZUTLS_H
#define KENZUTLS_H

#include <stddef.h> // For size_t

// Helper function to remove trailing newline character from a string
void remove_trailing_newline(char *str);
int shell_quote_single(const char *src, char *out, size_t out_size);
int copy_file_binary(const char *src, const char *dst);

#endif // !KENZUTLS.H
