#include <string.h>

#include <h2o.h>

#include <yueah/shared.h>

void print_hex_unsigned(const char *label, const unsigned char *str,
                        size_t size) {
  printf("%s: ", label);
  size_t i = 0;
  while (i < size) {
    printf("%02x ", str[i]);
    i++;
  }
  printf("\n");
}

void print_hex(const char *label, const yueah_string_t *str) {
  printf("%s: ", label);
  size_t i = 0;
  while (i < str->len) {
    printf("%02x ", str->data[i]);
    i++;
  }
  printf("\n");
}
