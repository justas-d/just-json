#define JSONREAD_IMPL
#include "../json-read.h"
#include <assert.h>

const char * data = "{\"key\": \"value\",\":}";

int main() {
  auto * f = fmemopen((void*)data, strlen(data), "rb");

  JSON_Read_Data json;
  auto * j = &json;
  jsonr_init(j, f);

  jsonr_v_table(j) {

    if(jsonr_k_case(j, "key")) {
      char * str;
      unsigned long length;
      jsonr_v_string(j, &str, &length);

      printf("%.*s\n", (int)length, str);
    }
    else {
      jsonr_kv_skip(j);
    }
  }

  if(j->error) {
    printf("%.*s\n", (int)j->error_msg_length, j->error_msg);
    return 0;
  }

  assert(false);
  return 1;
}
