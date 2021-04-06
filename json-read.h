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
       See either the 'High-level API', 'Low-level API' sections or example files.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifndef JSONR_STRINGLEN_READ_BUFFER_SIZE
  #define JSONR_STRINGLEN_READ_BUFFER_SIZE (1024*8)
#endif

#ifndef JSONREAD_DEF 
  #define JSONREAD_DEF extern
#endif

typedef struct {
  FILE * f;
  char c;
  int error; /* if set to 1: we encountered an error. */
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

enum {
  JSONR_READ_STRINGLEN_WANTS_MORE_MEMORY,
  JSONR_READ_STRINGLEN_DONE,
};

/* Init a read context. You'll want to call this to use any of the API. */
JSONREAD_DEF void jsonr_init(JSON_Read_Data *j, FILE *f);

/* ============================================ */
/* ============== High-level API ============== */
/* ============================================ */

/* Macros for parsing tables/arrays. See usage in the example code. */
#define jsonr_v_table(j) for(jsonr_v_table_begin(j); jsonr_v_table_can_read(j); )
#define jsonr_v_array(j) for(jsonr_v_array_begin(j); jsonr_v_array_can_read(j); )

/* Parses and escapes a string into a STRINGLEN_READ_BUFFER_SIZE sized buffer. 
 * Used by all key reading functions for simplicity. 
 * You'll have to DIY if you need to parse keys of any size. */
JSONREAD_DEF void jsonr_read_string_fixed_size(JSON_Read_Data *j, char **val, unsigned long *len);

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
JSONREAD_DEF void jsonr_v_string(JSON_Read_Data *j, char **val, unsigned long *len);

/* =========================================== */
/* ============== Low-level API ============== */
/* =========================================== */

/* Parses and escapes a string "some\ntext" into the given buffer. 
 * Begin reading with jsonr_begin_read_string, then call jsonr_read_string until it returns
 * JSONR_READ_STRINGLEN_DONE. If JSONR_READ_STRINGLEN_WANTS_MORE_MEMORY is returned, the
 * read function needs more memory. bytes_read doubles as a write cursor, so be careful to properly
 * rebase it if you're not realloc'ing the read buffer.
 * */
JSONREAD_DEF void jsonr_begin_read_string(JSON_Read_Data *j);
JSONREAD_DEF int jsonr_read_string(JSON_Read_Data *j, char *buf, unsigned long buf_length, unsigned long *bytes_read);

/* Read a comma if there is one */
JSONREAD_DEF int jsonr_maybe_read_comma(JSON_Read_Data * j);

/* Skip through a string that we begun reading by calling 'jsonr_begin_read_string' */
JSONREAD_DEF void jsonr_skip_remaining_string(JSON_Read_Data *j);

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
JSONREAD_DEF void jsonr_k(JSON_Read_Data *j, char **key, unsigned long *len);

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

    if(j->c == '\n') {
      j->line++;
      j->column = 0;
    }
    else {
      j->column++;
    }
  }
}

static void _advance(JSON_Read_Data * j) {
  j->read = 1;
}

static void _skip_whitespace(JSON_Read_Data * j) {
  for(;;) {
    _ensure_char(j);

    if(isspace(j->c)) {
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

static int _is_unescaped_char(char c) {
  switch(c) {
    case '\b':
    case '\f':
    case '\n':
    case '\r':
    case '\t': return true;
  }
  return false;
}

JSONREAD_DEF int jsonr_maybe_read_comma(JSON_Read_Data * j) {
  _skip_whitespace(j);

  if(j->c == ',') {
    _advance(j);
    return 1;
  }
  return 0;
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
    jsonr_maybe_read_comma(j);
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
    jsonr_maybe_read_comma(j);
    return 0;
  }

  return 1;
}

JSONREAD_DEF void jsonr_begin_read_string(JSON_Read_Data *j) {
  if(j->error) return;

  _skip_whitespace(j);

  if(j->c != '\"') {
    jsonr_error(j);
    return;
  }

  _advance(j);
}

JSONREAD_DEF int jsonr_read_string(JSON_Read_Data *j, char *buf, unsigned long buf_length, unsigned long *bytes_read) {
  int escaped = 0;
  char set;

  if(j->error) return JSONR_READ_STRINGLEN_DONE;

  for(;;) {
    if(*bytes_read >= buf_length) {
      return JSONR_READ_STRINGLEN_WANTS_MORE_MEMORY;
    }

    set = 0;
    _ensure_char(j);
    _advance(j);

    if(j->c == EOF) {
      jsonr_error(j);
      return JSONR_READ_STRINGLEN_DONE;
    }
    else if(_is_unescaped_char(j->c)) {
      jsonr_error(j);
      return JSONR_READ_STRINGLEN_DONE;
    }
    else if(escaped) {
      if(j->c == '\"') {
        set = '\"';
      }
      else if(j->c == '\\') {
        set = '\\';
      }
      else if(j->c == 'b') {
        set = '\b';
      }
      else if(j->c == 'f') {
        set = '\f';
      }
      else if(j->c == 'n') {
        set = '\n';
      }
      else if(j->c == 'r') {
        set = '\r';
      }
      else if(j->c == 't') {
        set = '\t';
      }
      else {
        set = j->c;
      }
      escaped = 0;
    }
    else if(j->c == '\\') {
      escaped = 1;
    }
    else if(j->c == '\"') {
      return JSONR_READ_STRINGLEN_DONE;
    }
    else {
      escaped = 0;
      set = j->c;
    }

    if(set) {
      buf[*bytes_read] = set;
      *bytes_read+= 1;
    }
  }
}

JSONREAD_DEF void jsonr_skip_remaining_string(JSON_Read_Data *j) {
  int escaped = 0;

  if(j->error) return;

  for(;;) {
    _ensure_char(j);
    _advance(j);

    if(j->c == EOF) {
      jsonr_error(j);
      return;
    }
    else if(_is_unescaped_char(j->c)) {
      jsonr_error(j);
      return;
    }
    else if(escaped) {
      // noop
    }
    else if(j->c == '\\') {
      escaped = 1;
    }
    else if(j->c == '\"') {
      return;
    }

    escaped = 0;
  }
}

JSONREAD_DEF void jsonr_read_string_fixed_size(JSON_Read_Data *j, char **val, unsigned long *len) {
  static char buf[JSONR_STRINGLEN_READ_BUFFER_SIZE];
  int result;
  *len = 0;

  jsonr_begin_read_string(j);
  for(;;) {
    result = jsonr_read_string(j, buf, JSONR_STRINGLEN_READ_BUFFER_SIZE, len);
    if(result == JSONR_READ_STRINGLEN_WANTS_MORE_MEMORY) {
      jsonr_skip_remaining_string(j);
    }

    *val = buf;
    return;
  }
}

JSONREAD_DEF void jsonr_v_string(JSON_Read_Data *j, char **val, unsigned long *len) {
  jsonr_read_string_fixed_size(j, val, len);
  jsonr_maybe_read_comma(j);
}

JSONREAD_DEF void jsonr_k(JSON_Read_Data *j, char **key, unsigned long *len) {
  if(j->error) return;

  jsonr_read_string_fixed_size(j, key, len);

  _skip_whitespace(j);

  if(j->c != ':') {
    jsonr_error(j);
    return;
  }

  _advance(j);
}

JSONREAD_DEF int jsonr_k_is_stringlen(JSON_Read_Data *j, const char *wants, unsigned long wants_len) {
  char * has;
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
  char * key;
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
    jsonr_maybe_read_comma(j);
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
    jsonr_maybe_read_comma(j);
    return 1;
  }
  else if(j->c == 'f') {
    _advance(j); if(j->c != 'a') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 'l') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 's') { jsonr_error(j); return 0; }
    _advance(j); if(j->c != 'e') { jsonr_error(j); return 0; }
    jsonr_maybe_read_comma(j);
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
    jsonr_maybe_read_comma(j);
    return 1;
  }

  jsonr_error(j);
  return 0;
}

JSONREAD_DEF void jsonr_v_skip(JSON_Read_Data *j) {
  int type;
  char * key;
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
      jsonr_v_string(j, &key, &len);
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


