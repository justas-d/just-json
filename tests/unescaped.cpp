#define JSONREAD_IMPL
#include "../json-read.h"
#include <assert.h>

const char * data = "{\"a key\": 5.0, \"another key\": 1.0,}";

int main() {
  auto * f = fmemopen((void*)data, strlen(data), "rb");

  JSON_Read_Data json;
  auto * j = &json;
  jsonr_init(j, f);

  jsonr_v_table(j) {
    jsonr_kv_skip(j);
  }

  if(j->error) {
    return 0;
  }
  
  assert(false);
  return 1;
}
