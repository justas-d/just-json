#define JSONREAD_IMPL
#include "../json-read.h"
#include <cstdlib>
#include <cassert>

const char * data = R"FOO(
{
  "camera_position" : {
    "x" : 6541.335938,
    "y" : 16202.147461
  },
  "camera_zoom" : 0.000075,
  "color" : {
    "w" : 1.000000,
    "x" : 0.117647,
    "y" : 0.137255,
    "z" : 0.156863
  },
  "last_resource_directory" : "/home/user/stuff/",
  "text_inline" : [{
      "color" : {
        "w" : -2.000000,
        "x" : -1.000000 ,
        "y" : -1.000000 ,
        "z" : -1.000000
      },
      "extents" : {
        "x" : .6098,
        "y" : +199.520401
      },
      "id" : 1,
      "origin" : {
        "x" : 1100.647827,
        "y" : 10950.095703
      },
      "scale" : {
        "x" : 0.924223,
        "y" : 0.924223
      },
      "text" : "The Workshop by Oni\nModern Graphics\nRevision 2021."
    }, {
      "color" : {
        "w" : 1.000000,
        "x" : 1.000000,
        "y" : 1.000000,
        "z" : 1.000000
      },
      "extents" : {
        "x" : 3299.284668,
        "y" : 647.637207
      },
      "id" : 5,
      "origin" : {
        "x" : 678.181885,
        "y" : 17667.335938
      },
      "scale" : {
        "x" : 1.000000,
        "y" : 1.000000
      },
      "text" : "Coastal Custodians by faith\nPaintover\nRevision 2021"
    }],
  "version" : 1
}
)FOO";

struct String_Len {
  const char * str;
  unsigned long num_bytes;
};

static String_Len jsonr_v_string_malloc(JSON_Read_Data * j) {
  unsigned long bytes_read = 0;
  unsigned long buffer_size = 32;
  char * buffer = (char*)malloc(buffer_size);

  jsonr_begin_read_string(j);
  while(true) {
    int result = jsonr_read_string(j, buffer, buffer_size, &bytes_read);

    if(result == JSONR_READ_STRING_WANTS_MORE_MEMORY) {
      buffer_size *= 2;
      buffer = (char*)realloc(buffer, buffer_size);
    }
    else if(result == JSONR_READ_STRING_DONE) {
      jsonr_maybe_read_comma(j);

      String_Len ret;
      ret.str = buffer;
      ret.num_bytes = bytes_read;
      return ret;
    }
    else {
      assert(false);
      return {};
    }
  }
}

typedef struct {
  float x,y;
} v2;

typedef struct {
  float x,y,z,w;
} v4;

#define jsonr_error_duplicate_key(key_str) \
  jsonr_error(j, "in '%s': duplicate key: '" key_str "'.", __func__)

#define jsonr_error_missing_key(key_str) \
  jsonr_error(j, "in '%s': missing key: '" key_str "'.", __func__)

static v2 jsonr_v_v2(JSON_Read_Data *j) {
  int got_x=0, got_y=0;
  v2 ret;

  jsonr_v_table(j) {
    if(jsonr_k_case(j, "x")) {
      if(got_x) { jsonr_error_duplicate_key("x"); return {}; };
      ret.x = jsonr_v_number(j);
      got_x = 1;
    }
    else if(jsonr_k_case(j, "y")) {
      if(got_y) { jsonr_error_duplicate_key("y"); return {}; };
      ret.y = jsonr_v_number(j);
      got_y = 1;
    }
  }

  if(!got_x) { jsonr_error_missing_key("x"); return {}; };
  if(!got_y) { jsonr_error_missing_key("y"); return {}; };

  return ret;
}

static v4 jsonr_v_v4(JSON_Read_Data *j) {
  int got_x=0,got_y=0,got_z=0,got_w=0;
  v4 ret;

  jsonr_v_table(j) {
    if(jsonr_k_case(j, "x")) {
      if(got_x) { jsonr_error_duplicate_key("x"); return {}; };
      ret.x = jsonr_v_number(j);
      got_x = 1;
    }
    else if(jsonr_k_case(j, "y")) {
      if(got_y) { jsonr_error_duplicate_key("y"); return {}; };
      ret.y = jsonr_v_number(j);
      got_y = 1;
    }
    else if(jsonr_k_case(j, "z")) {
      if(got_z) { jsonr_error_duplicate_key("z"); return {}; };
      ret.z = jsonr_v_number(j);
      got_z = 1;
    }
    else if(jsonr_k_case(j, "w")) {
      if(got_w) { jsonr_error_duplicate_key("w"); return {}; };
      ret.w = jsonr_v_number(j);
      got_w = 1;
    }
  }

  if(!got_x) { jsonr_error_missing_key("x"); return {}; };
  if(!got_y) { jsonr_error_missing_key("y"); return {}; };
  if(!got_z) { jsonr_error_missing_key("z"); return {}; };
  if(!got_w) { jsonr_error_missing_key("w"); return {}; };

  return ret;
}

int main() {
  FILE * f = fmemopen((void*)data, strlen(data), "rb");

  fpos_t start;
  fgetpos(f, &start);

  JSON_Read_Data json;
  JSON_Read_Data * j = &json;

  jsonr_init(j, f);

  int ver = -1;
  int got_version = 0;

  jsonr_v_table(j) {
    if(jsonr_k_case(j, "version")) {
      ver = jsonr_v_number(j);
      got_version = 1;
    }
    else {
      jsonr_kv_skip(j);
    }
  }

  if(!got_version) {
    if(j->error) {
      printf("Encountered an error during parsing.\n");
      printf("%.*s\n", (int)j->error_msg_length, j->error_msg);
      return 1;
    }

    printf("Could not find a version in file\n");
    return 1;
  }

  if(ver == 1) {
    fsetpos(f, &start);
    jsonr_init(j, f);

    jsonr_v_table(j) {
      if(jsonr_k_case(j, "last_resource_directory")) {
        String_Len str = jsonr_v_string_malloc(j);
        printf("last_resource_directory: '%.*s'\n", (int)str.num_bytes, str.str);
        free((void*)str.str);
      }
      else if(jsonr_k_case(j, "camera_zoom")) {
        float camera_zoom = jsonr_v_number(j);
        printf("camera_zoom: %f\n", camera_zoom);
      }
      else if(jsonr_k_case(j, "camera_position")) {
        v2 camera_position = jsonr_v_v2(j);
        printf("camera_position.x: %f\n", camera_position.x);
        printf("camera_position.y: %f\n", camera_position.y);
      }
      else if(jsonr_k_case(j, "color")) {
        v4 color = jsonr_v_v4(j);
        printf("color.x: %f\n", color.x);
        printf("color.y: %f\n", color.y);
      }
      else if(jsonr_k_case(j, "text_inline")) {
        int count = 0;
        jsonr_v_array(j) {
          printf("text_inline number %d:\n", count);
          jsonr_v_table(j) {
            if(jsonr_k_case(j, "id")) {
              int id = jsonr_v_number(j);
              printf("  id: %d\n", id);
            }
            else if(jsonr_k_case(j, "origin")) {
              v2 origin = jsonr_v_v2(j);
              printf("  origin.x: %f\n", origin.x);
              printf("  origin.y: %f\n", origin.y);
            }
            else if(jsonr_k_case(j, "extents")) {
              v2 extents = jsonr_v_v2(j);
              printf("  extents.x: %f\n", extents.x);
              printf("  extents.y: %f\n", extents.y);
            }
            else if(jsonr_k_case(j, "color")) {
              v4 color = jsonr_v_v4(j);
              printf("  color.x: %f\n", color.x);
              printf("  color.y: %f\n", color.y);
              printf("  color.z: %f\n", color.z);
              printf("  color.w: %f\n", color.w);
            }
            else if(jsonr_k_case(j, "scale")) {
              v2 scale = jsonr_v_v2(j);
              printf("  scale.x: %f\n", scale.x);
              printf("  scale.y: %f\n", scale.y);
            }
            else if(jsonr_k_case(j, "text")) {
              String_Len str = jsonr_v_string_malloc(j);
              printf("  text: '%.*s'\n", (int)str.num_bytes, str.str);
              free((void*)str.str);
            }
            else {
              jsonr_kv_skip(j);
            }
          }
        }
      }
      else {
        jsonr_kv_skip(j);
      }
    }

    if(j->error) {
      printf("Encountered an error during parsing.\n");
      printf("%.*s\n", (int)j->error_msg_length, j->error_msg);
      return 1;
    }
  }
  else {
    printf("Unknown version: %d\n", ver);
    return 1;
  }

  return 0;
}

