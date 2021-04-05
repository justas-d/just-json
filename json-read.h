/*
  * json-read.h - public domain - decode json - Justas Dabrila 2021
 
  * Just read json data from a file stream.
  * Strings are escaped (decoded as ascii. no utf8 decode).
  * fgetc, ungetc, ftell, fseek, fscanf are used. 
  * No explicit allocations (unless the aformentioned cstdlib funcs decide to allocate.)
  * No formatting or indentation.
  * 1024*8 (comptime constant) max length strings.

  * Usage:
    1. Define JSONREAD_IMPL once while including the file to include the implementation.
    {
      #define JSONREAD_IMPL
      #include "json-read.h"
    }

    2. Include the file in whatever place you want to use the API:
    {
      #include "json-read.h"
    }

    3. Use the API.
       See either the 'High-level API' or 'Low-level API' sections or the example code at the end of
       the file.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define JSONR_STRINGLEN_READ_BUFFER_SIZE (1024*8)
#define JSONREAD_DEF extern

typedef struct {
  FILE * f;
  char c;
  int error; /* set to 1 if we encountered an error. */
  int read;
  unsigned long line;
  unsigned long column;
} JSON_Read_Data;

typedef struct {
  int line;
  int column;
  char c;
  long pos;
  int read;
} JSON_Read_Peek; 

enum {
  JSONR_V_INVALID,
  JSONR_V_NUMBER,
  JSONR_V_ARRAY,
  JSONR_V_TABLE,
  JSONR_V_STRING,
  JSONR_V_BOOL,
  JSONR_V_NULL,
};

/* Init a read context. You'll want to call this to use any of the API. */
JSONREAD_DEF void jsonr_init(JSON_Read_Data *j, FILE *f);

/* ============================================ */
/* ============== High-level API ============== */
/* ============================================ */

/* Macros for parsing tables/arrays. See usage in the example code. */
#define jsonr_v_table(j) for(jsonr_v_table_begin(j); jsonr_v_table_can_read(j); )
#define jsonr_v_array(j) for(jsonr_v_array_begin(j); jsonr_v_array_can_read(j); )

/* Shortcut for checking if the current key matches the given one, consuming it if it is and
 * returning true */
JSONREAD_DEF int jsonr_k_case(JSON_Read_Data *j, const char *key);

/* Check if the key under the cursor matches the given string */
JSONREAD_DEF int jsonr_k_is_stringlen(JSON_Read_Data *j, const char *wants, unsigned long wants_len);
JSONREAD_DEF int jsonr_k_is(JSON_Read_Data *j, const char *key);

/* Skip a key value pair in a table. */
JSONREAD_DEF void jsonr_kv_skip(JSON_Read_Data *j);

/* Advance over the key under the cursor */
JSONREAD_DEF void jsonr_k_eat(JSON_Read_Data *j);

/* Skip the current value under the cursor and advance. Properly handles all value types, so arrays
 * and tables will be skipped recursively. */
JSONREAD_DEF void jsonr_v_skip(JSON_Read_Data *j);


/* Get the type of the value under the cursor */
JSONREAD_DEF int jsonr_v_get_type(JSON_Read_Data *j);

/* Read & advance functions */
JSONREAD_DEF double jsonr_v_number(JSON_Read_Data *j);
JSONREAD_DEF int jsonr_v_bool(JSON_Read_Data *j);

/* Read a string value and advance */
JSONREAD_DEF void jsonr_v_stringlen(JSON_Read_Data *j, const char **val, unsigned long *len);

/* =========================================== */
/* ============== Low-level API ============== */
/* =========================================== */

/* Parses and escapes a string "some\ntext" into an STRINGLEN_READ_BUFFER_SIZE sized buffer. */
JSONREAD_DEF void jsonr_read_stringlen(JSON_Read_Data *j, const char **val, unsigned long *len);

/* Report an error. Past this point, every call to jsonr_* becomes a noop until the error flag
  is reset. */
JSONREAD_DEF void jsonr_error(JSON_Read_Data *j);

/* Begin reading tables/arrays. */
JSONREAD_DEF int jsonr_v_table_begin(JSON_Read_Data *j);
JSONREAD_DEF int jsonr_v_array_begin(JSON_Read_Data *j);

/* Check if the current table/array still has values in it. */
JSONREAD_DEF int jsonr_v_table_can_read(JSON_Read_Data *j);
JSONREAD_DEF int jsonr_v_array_can_read(JSON_Read_Data *j);

/* Read a key under the cursor and advance */
JSONREAD_DEF void jsonr_k(JSON_Read_Data *j, const char **key, unsigned long *len);

/* Store the current cursor information into a variable */
JSONREAD_DEF JSON_Read_Peek jsonr_peek_begin(JSON_Read_Data *j);

/* Restore cursor information */
JSONREAD_DEF void jsonr_peek_end(JSON_Read_Data *j, JSON_Read_Peek peek);

/* ============================================ */
/* ============== Implementation ============== */
/* ============================================ */

#ifdef JSONREAD_IMPL 

#include <ctype.h>
#include <string.h>

JSONREAD_DEF void jsonr_error(JSON_Read_Data *j) {
  j->error = 1;
}

static void _ensure_char(JSON_Read_Data * j) {
  if(j->read) {
    j->read = 0;
    j->c = fgetc(j->f);
    j->column++;
  }
}

static void _advance(JSON_Read_Data * j) {
  j->read = 1;
}

static void _comma(JSON_Read_Data * j) {
  _ensure_char(j);
  if(j->c == ',') {
    _advance(j);
  }
}

static void _skip_whitespace(JSON_Read_Data * j) {
  for(;;) {
    _ensure_char(j);

    if(isspace(j->c)) {
      if(j->c == '\n') {
        j->line++;
        j->column = 0;
      }

      _advance(j);
    }
    else {
      return;
    }
  }
}

static int _str_are_equal(
  const char * a,
  unsigned long a_length,
  const char * b,
  unsigned long b_length
) {
  int i;
  if(a_length != b_length) {
    return 0;
  }

  for(i=0; i < a_length; i++) {
    if(a[i] != b[i]) {
      return 0;
    }
  }
  return 1;
}

JSONREAD_DEF JSON_Read_Peek jsonr_peek_begin(JSON_Read_Data *j) {
  JSON_Read_Peek peek;
  peek.c = j->c;
  peek.line = j->line;
  peek.column = j->column;
  peek.pos = ftell(j->f);
  peek.read = j->read;
  if(peek.pos == -1) {
    jsonr_error(j);
  }

  return peek;
}

JSONREAD_DEF void jsonr_peek_end(JSON_Read_Data *j, JSON_Read_Peek peek) {
  j->c = peek.c;
  j->line = peek.line;
  j->column = peek.column;
  j->read = peek.read;

  if(fseek(j->f, peek.pos, SEEK_SET) != 0) {
    jsonr_error(j);
  }
}

JSONREAD_DEF void jsonr_init(JSON_Read_Data *j, FILE *f) {
  j->f = f;
  j->c = 0;
  j->error = 0;
  j->read = 1;
  j->line = 1;
  j->column = 0;
}

JSONREAD_DEF int jsonr_v_table_begin(JSON_Read_Data *j) {
  if(j->error) return 0;

  _skip_whitespace(j);

  if(j->c == '{') {
    _advance(j);
    return 1;
  }
  else if(j->c == EOF) {
    jsonr_error(j);
    return 0;
  }

  return 0;
}

JSONREAD_DEF int jsonr_v_table_can_read(JSON_Read_Data *j) {
  if(j->error) return 0;

  _skip_whitespace(j);

  if(j->c == '}') {
    _advance(j);
    _comma(j);
    return 0;
  }

  return 1;
}

JSONREAD_DEF int jsonr_v_array_begin(JSON_Read_Data *j) {
  if(j->error) return 0;

  _skip_whitespace(j);

  if(j->c == '[') {
    _advance(j);
    return 1;
  }
  else if(j->c == EOF) {
    jsonr_error(j);
    return 0;
  }

  return 0;
}

JSONREAD_DEF int jsonr_v_array_can_read(JSON_Read_Data *j) {
  if(j->error) return 0;

  _skip_whitespace(j);

  if(j->c == ']') {
    _advance(j);
    _comma(j);
    return 0;
  }

  return 1;
}

JSONREAD_DEF void jsonr_read_stringlen(JSON_Read_Data *j, const char **val, unsigned long *len) {
  static char buf[JSONR_STRINGLEN_READ_BUFFER_SIZE];
  int escaped = 0;

  if(j->error) return;

  _skip_whitespace(j);

  if(j->c != '\"') {
    jsonr_error(j);
    return;
  }

  _advance(j);

  *len = 0;

  for(;;) {
    _ensure_char(j);
    _advance(j);

    if(j->c == EOF) {
      jsonr_error(j);
      return;
    }
    else if(j->c == '\b') {
      jsonr_error(j);
      return;
    }
    else if(j->c == '\f') {
      jsonr_error(j);
      return;
    }
    else if(j->c == '\n') {
      jsonr_error(j);
      return;
    }
    else if(j->c == '\r') {
      jsonr_error(j);
      return;
    }
    else if(j->c == '\t') {
      jsonr_error(j);
      return;
    }
    else if(j->c == '\\') {
      escaped = 1;
    }
    else if(escaped) {
      if(j->c == '\"') {
        buf[*len] = '\"';
      }
      else if(j->c == '\\') {
        buf[*len] = '\\';
      }
      else if(j->c == 'b') {
        buf[*len] = '\b';
      }
      else if(j->c == 'f') {
        buf[*len] = '\f';
      }
      else if(j->c == 'n') {
        buf[*len] = '\n';
      }
      else if(j->c == 'r') {
        buf[*len] = '\r';
      }
      else if(j->c == 't') {
        buf[*len] = '\t';
      }
      else {
        buf[*len] = j->c;
      }
      escaped = 0;
    }
    else if(j->c == '\"') {
      *val= buf;
      return;
    }
    else {
      escaped = 0;
      buf[*len] = j->c;
    }

    *len += 1;
  }
}

JSONREAD_DEF void jsonr_v_stringlen(JSON_Read_Data *j, const char **val, unsigned long *len) {
  jsonr_read_stringlen(j, val, len);
  _comma(j);
}

JSONREAD_DEF void jsonr_k(JSON_Read_Data *j, const char **key, unsigned long *len) {
  fpos_t pos;
  char prev_c;

  if(j->error) return;

  jsonr_read_stringlen(j, key, len);

  _skip_whitespace(j);

  if(j->c != ':') {
    jsonr_error(j);
    return;
  }

  _advance(j);
}

JSONREAD_DEF int jsonr_k_is_stringlen(JSON_Read_Data *j, const char *wants, unsigned long wants_len) {
  const char * has;
  unsigned long has_len;
  int i;

  jsonr_k(j, &has, &has_len);
  if(j->error) return 0;

  return _str_are_equal(has, has_len, wants, wants_len);
}

JSONREAD_DEF int jsonr_k_is(JSON_Read_Data *j, const char *key) {
  int ret;
  JSON_Read_Peek peek = jsonr_peek_begin(j);

  ret = jsonr_k_is_stringlen(j, key, strlen(key));
  jsonr_peek_end(j, peek);

  return ret;
}

JSONREAD_DEF void jsonr_k_eat(JSON_Read_Data *j) {
  const char * key;
  unsigned long len;

  jsonr_k(j, &key, &len);
}

JSONREAD_DEF int jsonr_k_case(JSON_Read_Data *j, const char *key) {
  if(jsonr_k_is(j, key)) {
    jsonr_k_eat(j);
    return 1;
  }
  return 0;
}

JSONREAD_DEF int jsonr_v_get_type(JSON_Read_Data *j) {
  if(j->error) return 0;

  _skip_whitespace(j);

  if(j->c == '\"') {
    return JSONR_V_STRING;
  }
  else if(isdigit(j->c)) {
    return JSONR_V_NUMBER;
  }
  else if(j->c == 't') {
    return JSONR_V_BOOL;
  }
  else if(j->c == 'f') {
    return JSONR_V_BOOL;
  }
  else if(j->c == 'n') {
    return JSONR_V_NULL;
  }
  else if(j->c == '{') {
    return JSONR_V_TABLE;
  }
  else if(j->c == '[') {
    return JSONR_V_ARRAY;
  }

  return JSONR_V_INVALID;
}

JSONREAD_DEF double jsonr_v_number(JSON_Read_Data *j) {
  double val;
  int result;
  long tell_before;

  if(j->error) return 0;

  _skip_whitespace(j);

  tell_before = ftell(j->f);

  if(ungetc(j->c, j->f) != j->c) {
    jsonr_error(j);
    return 0;
  }

  result = fscanf(j->f, "%lf", &val);

  j->column += ftell(j->f) - tell_before;

  if(result == 1) {
    j->read = 1;
    _comma(j);
    return val;
  }
  else if(result != 0) {
    jsonr_error(j);
    return 0;
  }
  else if(result == EOF) {
    jsonr_error(j);
    return 0;
  }

  jsonr_error(j);
  return 0;
}

JSONREAD_DEF int jsonr_v_bool(JSON_Read_Data *j) {
  if(j->error) return 0;

  _skip_whitespace(j);
  if(j->c == 't') {
    _advance(j); if(j->c != 'r') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 'u') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 'e') { jsonr_error(j); return 0; }
    _comma(j);
    return 1;
  }
  else if(j->c == 'f') {
    _advance(j); if(j->c != 'a') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 'l') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 's') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 'e') { jsonr_error(j); return 0; }
    _comma(j);
    return 0;
  }

  j->error = 1;
  return 0;
}

JSONREAD_DEF int jsonr_v_null(JSON_Read_Data *j) {
  if(j->error) return 0;

  _skip_whitespace(j);
  if(j->c == 'n') {
    _advance(j); if(j->c != 'u') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 'l') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 'l') { jsonr_error(j); return 0; }
    _comma(j);
    return 1;
  }

  jsonr_error(j);
  return 0;
}

JSONREAD_DEF void jsonr_v_skip(JSON_Read_Data *j) {
  int type;
  const char * key;
  unsigned long len;

  if(j->error) return;

  type = jsonr_v_get_type(j);

  switch(type) {
    case JSONR_V_INVALID: jsonr_error(j); break; 
    case JSONR_V_NUMBER: jsonr_v_number(j); break;
    case JSONR_V_ARRAY: {
      jsonr_v_array(j) {
        jsonr_v_skip(j);
      }
      break;
    }
    case JSONR_V_TABLE: {
      jsonr_v_table(j) {
        jsonr_k_eat(j);
        jsonr_v_skip(j);
      }
      break;
    }
    case JSONR_V_STRING: {
      jsonr_v_stringlen(j, &key, &len);
      break;
    }
    case JSONR_V_BOOL: jsonr_v_bool(j); break;
    case JSONR_V_NULL: jsonr_v_null(j); break;
  }
}


JSONREAD_DEF void jsonr_kv_skip(JSON_Read_Data *j) {
  jsonr_k_eat(j);
  jsonr_v_skip(j);
}
#endif

#ifdef __cplusplus
}
#endif

/* Example code.
   'read_data.json':
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
        "w" : 1.000000,
        "x" : 1.000000,
        "y" : 1.000000,
        "z" : 1.000000
      },
      "extents" : {
        "x" : 6098.551270,
        "y" : 199.520401
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
      "text" : "The Workshop by Oni\\\\nModern Graphics\\\\nRevision 2021."
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

=====================================

#define JSONREAD_IMPL
#include "json-read.h"
#include <stdlib.h>

static const char * jsonr_v_string_malloc(JSON_Read_Data * j) {
  const char * val;
  unsigned long length;
  jsonr_v_stringlen(j, &val, &length);

  void * ret = malloc(length);
  memcpy(ret, val, length);

  return ret;
}

typedef struct {
  float x,y;
} v2;

typedef struct {
  float x,y,z,w;
} v4;

static v2 jsonr_v_v2(JSON_Read_Data *j) {
  int got_x=0, got_y=0;
  v2 ret;

  jsonr_v_table(j) {
    if(jsonr_k_case(j, "x")) {
      if(got_x) { jsonr_error(j); }
      ret.x = jsonr_v_number(j);
      got_x = 1;
    }
    else if(jsonr_k_case(j, "y")) {
      if(got_y) { jsonr_error(j); }
      ret.y = jsonr_v_number(j);
      got_y = 1;
    }
  }

  if(!got_x) { jsonr_error(j); }
  if(!got_y) { jsonr_error(j); }

  return ret;
}

static v4 jsonr_v_v4(JSON_Read_Data *j) {
  int got_x=0,got_y=0,got_z=0,got_w=0;
  v4 ret;

  jsonr_v_table(j) {
    if(jsonr_k_case(j, "x")) {
      if(got_x) { jsonr_error(j); }
      ret.x = jsonr_v_number(j);
      got_x = 1;
    }
    else if(jsonr_k_case(j, "y")) {
      if(got_y) { jsonr_error(j); }
      ret.y = jsonr_v_number(j);
      got_y = 1;
    }
    else if(jsonr_k_case(j, "z")) {
      if(got_z) { jsonr_error(j); }
      ret.z = jsonr_v_number(j);
      got_z = 1;
    }
    else if(jsonr_k_case(j, "w")) {
      if(got_w) { jsonr_error(j); }
      ret.w = jsonr_v_number(j);
      got_w = 1;
    }
  }

  if(!got_x) { jsonr_error(j); }
  if(!got_y) { jsonr_error(j); }
  if(!got_z) { jsonr_error(j); }
  if(!got_w) { jsonr_error(j); }

  return ret;
}

int main() {
  FILE * f = fopen("read_data.json", "rb");
  if(!f) {
    printf("Can't open read_data.json\n");
    return 1;
  }

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
    printf("Could not find a version in file\n");
    return 1;
  }

  if(ver == 1) {
    fsetpos(f, &start);
    jsonr_init(j, f);

    jsonr_v_table(j) {
      if(jsonr_k_case(j, "last_resource_directory")) {
        const char * last_resource_directory = jsonr_v_string_malloc(j);
        printf("last_resource_directory: '%s'\n", last_resource_directory);
        free((void*)last_resource_directory);
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
              const char * text = jsonr_v_string_malloc(j);
              printf("  text: '%s'\n", text);
              free((void*)text);
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
      printf("Encountered an error during parsing\n");
      return 1;
    }
  }
  else {
    printf("Unknown version: %d\n", ver);
    return 1;
  }

  return 0;
}
*/

/*
  Public Domain (www.unlicense.org)
  This is free and unencumbered software released into the public domain.
  Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
  software, either in source code form or as a compiled binary, for any purpose,
  commercial or non-commercial, and by any means.
  In jurisdictions that recognize copyright laws, the author or authors of this
  software dedicate any and all copyright interest in the software to the public
  domain. We make this dedication for the benefit of the public at large and to
  the detriment of our heirs and successors. We intend this dedication to be an
  overt act of relinquishment in perpetuity of all present and future rights to
  this software under copyright law.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


