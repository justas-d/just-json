#define JSONWRITE_IMPL
#include "../json-write.h"

typedef struct {
  int number;
  const char * text;
} Data;

static const int data_count = 5;
static Data data[] = {
  { .number = 1, .text = "hello" },
  { .number = 1, .text = "this\n has\t\n\r\n\n a bunch \n\n\n\n\b\b\b of escapes \\ \"" },
  { .number = 2, .text = "world" },
  { .number = 3, .text = "ccc" },
  { .number = 5, .text = "revision" },
};

int main() {
  auto * f = stdout;
  int i;

  JSON_Write_Data json;
  JSON_Write_Data * j = &json;

  jsonw_init(j, f);

  jsonw_v_table_begin(j);

    jsonw_kv_int(j, "version", 1);
    jsonw_kv_string(j, "last_resource_directory", "/home/user/data");
    jsonw_kv_float(j, "camera_zoom", 3.1415); 
    jsonw_kv_bool(j, "is_alive", 1); 

    jsonw_k(j, "position");
    jsonw_v_table_begin(j);
      jsonw_kv_float(j, "x", 10);
      jsonw_kv_float(j, "y", 20);
    jsonw_v_table_end(j);

    jsonw_k(j, "bunch_of_data");
    jsonw_v_array_begin(j);
    for(i = 0; i < data_count; i++) {
      jsonw_v_table_begin(j);
        jsonw_kv_int(j, "number", data[i].number);
        jsonw_kv_string(j, "text", data[i].text);
      jsonw_v_table_end(j);
    }
    jsonw_v_array_end(j);

  jsonw_v_table_end(j);

  fclose(f);

  return 0;
}
