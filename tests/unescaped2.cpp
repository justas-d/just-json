#define JSONREAD_IMPL
#include "../json-read.h"
#include <assert.h>

const char * data = "{ \"text\nwith newline\nnon terminating";

int main() {
  auto * f = fmemopen((void*)data, strlen(data), "rb");

  JSON_Read_Data json;
  auto * j = &json;
  jsonr_init(j, f);

  jsonr_v_table(j) {
    char * str;
    unsigned long length;
    jsonr_v_string(j, &str, &length);

    printf("%.*s\n", (int)length, str);
  }

  if(j->error) {
    return 0;
  }

  assert(false);
  return 1;
}
