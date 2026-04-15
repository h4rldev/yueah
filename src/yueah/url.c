#include <ctype.h>

#include <yueah/types.h>
#include <yueah/url.h>

void yueah_urldecode(cstr *dst, const cstr *src) {
  char a, b;
  while (*src) {
    if (*src == '%' && ((a = src[1]) && (b = src[2])) && isxdigit(a) &&
        isxdigit(b)) {
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';

      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';

      *dst++ = 16 * a + b;
      src += 3;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}
