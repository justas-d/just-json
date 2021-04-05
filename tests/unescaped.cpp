#define JSONREAD_IMPL
#include "../json-read.h"

const char * data = "{\"";

int main() {
  auto * f = fmemopen(data, strlen(data), "rb");

  JSON_Read_Data json;
  auto * j = &json;
  jsonr_init(j, f);

  jsonr_v_table(j) {
    const char * str;
    unsigned long length;
    jsonr_v_stringlen(j, &str, &length);

    printf("%.*s\n", length, str);
  }

  fclose(f);

  return 0;
}
