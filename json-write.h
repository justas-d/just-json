/*
  * json-write.h - public domain - encode json - Justas Dabrila 2021
 
  * Just write json data to a file stream.
  * Strings are escaped (decoded as ascii. no utf8 decode).
  * fprintf, fputc, assert are used. 
  * No explicit allocations (unless the aformentioned cstdlib funcs decide to allocate.)
  * No formatting or indentation.

  * Usage:
    1. Define JSONWRITE_IMPL once while including the file to include the implementation.
    {
      #define JSONWRITE_IMPL
      #include "json-write.h"
    }

    2. Include the file in whatever place you want to use the API:
    {
      #include "json-write.h"
    }

    3. Use the API.
       See either the 'High-level API', 'Low-level API' sections or example files.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define JSONWRITE_DEF extern

typedef struct {
  FILE *f;
  long table_stack;
  long array_stack;
  int do_comma;
} JSON_Write_Data;

/* Initialize the JSON_Write_Data structure. */
JSONWRITE_DEF void jsonw_init(JSON_Write_Data *json, FILE *f);

/* ============================================ */
/* ============== High-level API ============== */
/* ============================================ */

/* Write a key (i.e "key_value":) with the given string and length of the string. */
JSONWRITE_DEF void jsonw_klen(JSON_Write_Data *json, const char *str, unsigned long length);
JSONWRITE_DEF void jsonw_k(JSON_Write_Data *json, const char * cstr);

/* Write begins/ends of arrays [ ] /tables { }. */
JSONWRITE_DEF void jsonw_v_table_begin(JSON_Write_Data *json);
JSONWRITE_DEF void jsonw_v_table_end(JSON_Write_Data *json);
JSONWRITE_DEF void jsonw_v_array_begin(JSON_Write_Data *json);
JSONWRITE_DEF void jsonw_v_array_end(JSON_Write_Data *json);

/* Write primitive values */
JSONWRITE_DEF void jsonw_v_int(JSON_Write_Data *json, long val);
JSONWRITE_DEF void jsonw_v_uint(JSON_Write_Data *json, unsigned long val);
JSONWRITE_DEF void jsonw_v_float(JSON_Write_Data *json, double val);
JSONWRITE_DEF void jsonw_v_bool(JSON_Write_Data *json, int val);
JSONWRITE_DEF void jsonw_v_stringlen(JSON_Write_Data *json, const char *val, unsigned long len);
JSONWRITE_DEF void jsonw_v_string(JSON_Write_Data *json, const char *val);

/* Write both a key and a value (i.e "key": "my value here"). */
JSONWRITE_DEF void jsonw_kv_int(JSON_Write_Data *json, const char *key, long val);
JSONWRITE_DEF void jsonw_kv_uint(JSON_Write_Data *json, const char *key, unsigned long val);
JSONWRITE_DEF void jsonw_kv_float(JSON_Write_Data *json, const char *key, double val);
JSONWRITE_DEF void jsonw_kv_bool(JSON_Write_Data *json, const char *key, int val);
JSONWRITE_DEF void jsonw_kv_string(JSON_Write_Data *json, const char *key, const char *val);

/* =========================================== */
/* ============== Low-level API ============== */
/* =========================================== */

/* Maybe write the comma if we need to. */
JSONWRITE_DEF void jsonw_maybe_comma(JSON_Write_Data *json);

/* Write an escaped string with the given length. */
JSONWRITE_DEF void jsonw_escaped_string(JSON_Write_Data *json, const char *str, unsigned long length);

/* ============================================ */
/* ============== Implementation ============== */
/* ============================================ */

#ifdef JSONWRITE_IMPL

#include <assert.h>
#include <stdio.h>
#include <string.h>

JSONWRITE_DEF void jsonw_init(JSON_Write_Data *json, FILE *f) {
  json->f = f;
  json->table_stack = 0;
  json->array_stack = 0;
  json->do_comma = 0;
}

JSONWRITE_DEF void jsonw_maybe_comma(JSON_Write_Data *json) {
  if(json->do_comma) {
    fputc(',', json->f);
    json->do_comma = 0;
  }
}

/*
  Generated from:

#include <stdio.h>

int main() {
  static const char ESCAPES[] = { '\"', '\\', '\b', '\f', '\n', '\r', '\t' };

  for(int i = 0; i < 256; i++) {
    int found = 0;
    for(int k = 0; k < sizeof(ESCAPES)/sizeof(ESCAPES[0]); k++) {
      int escape = ESCAPES[k];

      if(i == escape) {
        printf("%d,", k+1);
        found = 1;
        break;
      }
    }

    if(!found) {
      printf("0,");
    }
  }
  return 0;
}

*/

static const char ESCAPE_LUT[256] = {
  0,0,0,0,0,0,0,0,3,7,5,0,4,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0
};

static const char *ESCAPE_STR[] = { "\\\"", "\\\\", "\\b", "\\f", "\\n", "\\r", "\\t" };

JSONWRITE_DEF void jsonw_escaped_string(JSON_Write_Data *json, const char *str, unsigned long length) {
  int char_it;
  char escape;

search:
  for(char_it = 0; char_it < length; char_it++) {
    escape = ESCAPE_LUT[str[char_it]];
    if(escape) {
      fprintf(json->f, "%.*s%s", char_it, str, ESCAPE_STR[escape-1]);
      length -= char_it;
      str += char_it;

      if(length > 0) {
        length -= 1;
        str += 1;
      }
      goto search;
    }
  }

  fprintf(json->f, "%.*s", length, str);
}

JSONWRITE_DEF void jsonw_klen(JSON_Write_Data *json, const char *str, unsigned long length) {
  jsonw_maybe_comma(json);

  fputc('\"', json->f);
  jsonw_escaped_string(json, str, length);

  fputc('\"', json->f);
  fputc(':', json->f);
}

JSONWRITE_DEF void jsonw_k(JSON_Write_Data *json, const char * cstr) {
  jsonw_klen(json, cstr, strlen(cstr));
}

JSONWRITE_DEF void jsonw_v_table_begin(JSON_Write_Data *json) {
  jsonw_maybe_comma(json);

  json->table_stack++;
  fputc('{', json->f);
}

JSONWRITE_DEF void jsonw_v_table_end(JSON_Write_Data *json) {
  if(json->table_stack<=0) {
    assert(0 && "Mismatched jsonw_v_table_start and jsonw_v_table_end calls!");
  }
  json->table_stack--;
  fputc('}', json->f);

  json->do_comma = 1;
}

JSONWRITE_DEF void jsonw_v_array_begin(JSON_Write_Data *json) {
  jsonw_maybe_comma(json);

  json->array_stack++;
  fputc('[', json->f);
}

JSONWRITE_DEF void jsonw_v_array_end(JSON_Write_Data *json) {
  if(json->array_stack<=0) {
    assert(0 && "Mismatched jsonw_v_array_start and jsonw_v_array_end calls!");
  }
  json->array_stack--;
  fputc(']', json->f);

  json->do_comma = 1;
}

JSONWRITE_DEF void jsonw_v_int(JSON_Write_Data *json, long val) {
  jsonw_maybe_comma(json);

  fprintf(json->f, "%lld", val);
  json->do_comma = 1;
}

JSONWRITE_DEF void jsonw_v_uint(JSON_Write_Data *json, unsigned long val) {
  jsonw_maybe_comma(json);

  fprintf(json->f, "%llu", val);
  json->do_comma = 1;
}

JSONWRITE_DEF void jsonw_v_float(JSON_Write_Data *json, double val) {
  jsonw_maybe_comma(json);

  fprintf(json->f, "%f", val);
  json->do_comma = 1;
}

JSONWRITE_DEF void jsonw_v_bool(JSON_Write_Data *json, int val) {
  jsonw_maybe_comma(json);

  fprintf(json->f, "%s", val == 0 ? "false" : "true");
  json->do_comma = 1;
}

JSONWRITE_DEF void jsonw_v_stringlen(JSON_Write_Data *json, const char *val, unsigned long len) {
  jsonw_maybe_comma(json);

  fputc('\"', json->f);
  jsonw_escaped_string(json, val, len);
  fputc('\"', json->f);

  json->do_comma = 1;
}

JSONWRITE_DEF void jsonw_v_string(JSON_Write_Data *json, const char *val) {
  jsonw_v_stringlen(json, val, strlen(val));
}

JSONWRITE_DEF void jsonw_kv_int(JSON_Write_Data *json, const char *key, long val) {
  jsonw_k(json, key);
  jsonw_v_int(json, val);
}

JSONWRITE_DEF void jsonw_kv_uint(JSON_Write_Data *json, const char *key, unsigned long val) {
  jsonw_k(json, key);
  jsonw_v_uint(json, val);
}

JSONWRITE_DEF void jsonw_kv_float(JSON_Write_Data *json, const char *key, double val) {
  jsonw_k(json, key);
  jsonw_v_float(json, val);
}

JSONWRITE_DEF void jsonw_kv_bool(JSON_Write_Data *json, const char *key, int val) {
  jsonw_k(json, key);
  jsonw_v_bool(json, val);
}

JSONWRITE_DEF void jsonw_kv_string(JSON_Write_Data *json, const char *key, const char *val) {
  jsonw_k(json, key);
  jsonw_v_string(json, val);
}
#endif

#ifdef __cplusplus
}
#endif

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

