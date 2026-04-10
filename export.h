#ifndef EXPORT_H
#define EXPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// TODO:
// - Обрабтка ощибок
// - SQLITE

typedef struct exporter_t exporter_t;

/**
 * @struct exporter_t
 * @brief The basic structure of the exporter interface
 *
 * Defines a set of operations for data export. Each specific
 * implementation (e.g., for CSV, JSON, etc) must provide
 * its own versions of these functions
 */
typedef struct exporter_t {
  /**
   * @brief Begins writing a new structure/object
   * @param name Name of the structure/object (may be NULL)
   * @return 0 on success, otherwise -1 on error
   */
  int (*begin_object)(exporter_t *self, const char *name);

  /**
   * @brief Ends writing a structure/object
   * @return 0 on success, otherwise -1 on error
   */
  int (*end_object)(exporter_t *self);

  /**
   * @brief Write functions
   * @param key Field name (used as a key within the object). Can be NULL
   * @return 0 on success, otherwise -1 on error
   */
  int (*write_int)(exporter_t *self, const char *key, int64_t value);
  int (*write_double)(exporter_t *self, const char *key, double value);
  int (*write_string)(exporter_t *self, const char *key, const char *value);
  int (*write_bool)(exporter_t *self, const char *key, bool value);
  int (*write_null)(exporter_t *self, const char *key);

  /**
   * @brief Begins writing an array for the specified key
   * @param key Field name (used as a key within the object). Can be NULL
   * @return 0 on success, otherwise -1 on error
   */
  int (*begin_array)(exporter_t *self, const char *key);

  /// @brief Ends writing an array
  int (*end_array)(exporter_t *self);

  /// @brief Flush data into output
  int (*flush)(exporter_t *self);

  /// @brief Frees up resources allocated to the exporter
  void (*destroy)(exporter_t *self);

} exporter_t;

typedef struct csv_exporter_t {
  exporter_t base;
  FILE *output;
  const char *csv_header;
  bool in_array;
  bool is_first;
  bool header_written;
  bool write_header_once;

} csv_exporter_t;

/// @brief Creates a new exporter in CSV format
/// @param file output file (stdout, fopen(...) etc)
/// @param write_header_once if true, header is written only once (recommended
/// for multi-row CSV)
csv_exporter_t create_csv_exporter(FILE *file, const char *csv_header,
                                   bool write_header_once);

typedef struct json_exporter_t {
  exporter_t base;
  FILE *output;
  int depth;
  int capacity;
  bool *context_is_object;
  bool *level_first;
  bool pretty; ///<- if true — pretty-print JSON (2-space indent + newlines)

} json_exporter_t;

/// @brief Creates a new exporter in JSON format
/// @param file output file (stdout, fopen(...) etc)
/// @param pretty if true, outputs pretty-printed JSON (newlines + 2-space
/// indentation)
json_exporter_t create_json_exporter(FILE *file, bool pretty);

#ifdef EXPORT_IMPLEMENTATION

/*----------------------CSV EXPORTER----------------------*/

// Helper: writes a properly quoted CSV string field according to RFC 4180
// (always quoted with ", internal " doubled)
static inline int csv_write_quoted_string(FILE *out, const char *value) {
  if (fputc('"', out) == EOF)
    return -1;

  if (value) {
    for (const char *p = value; *p; ++p) {
      if (*p == '"') {
        if (fputc('"', out) == EOF || fputc('"', out) == EOF)
          return -1;
      } else {
        if (fputc(*p, out) == EOF)
          return -1;
      }
    }
  }

  if (fputc('"', out) == EOF)
    return -1;

  return 0;
}

static int csv_flush_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  return fflush(csv->output);
}

// Write csv header (only once if write_header_once == true)
int csv_begin_object_impl(exporter_t *self, const char *name) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  csv->is_first = true;

  if (!csv->header_written || !csv->write_header_once) {
    if (fprintf(csv->output, "%s\n", csv->csv_header) < 0)
      return -1;
    csv->header_written = true;
  }

  return 0;
}

// Add new line
int csv_end_object_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  csv->is_first = false;

  return (putc('\n', csv->output) == EOF) ? -1 : 0;
}

static int csv_write_int_impl(exporter_t *self, const char *key,
                              int64_t value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF)
      return -1;
  }

  csv->is_first = false;

  return (fprintf(csv->output, "%lld", value) < 0) ? -1 : 0;
}

static int csv_write_double_impl(exporter_t *self, const char *key,
                                 double value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF)
      return -1;
  }

  csv->is_first = false;

  return (fprintf(csv->output, "%f", value) < 0) ? -1 : 0;
}

static int csv_write_string_impl(exporter_t *self, const char *key,
                                 const char *value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF)
      return -1;
  }

  csv->is_first = false;

  return csv_write_quoted_string(csv->output, value);
}

static int csv_write_bool_impl(exporter_t *self, const char *key, bool value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF)
      return -1;
  }

  csv->is_first = false;

  return (fprintf(csv->output, "%s", value ? "true" : "false") < 0) ? -1 : 0;
}

static int csv_write_null_impl(exporter_t *self, const char *key) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF)
      return -1;
  }

  csv->is_first = false;

  return (fprintf(csv->output, "null") < 0) ? -1 : 0;
}

static int csv_begin_array_impl(exporter_t *self, const char *key) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF)
      return -1;
  }

  if (fputc('"', csv->output) == EOF || fputc('[', csv->output) == EOF)
    return -1;

  csv->in_array = true;
  csv->is_first = true;

  return 0;
}

static int csv_end_array_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (fputc(']', csv->output) == EOF || fputc('"', csv->output) == EOF)
    return -1;

  csv->in_array = false;
  csv->is_first = false;

  return 0;
}

// dummy function
static void csv_destroy_impl(exporter_t *self) { (void)self; }

csv_exporter_t create_csv_exporter(FILE *file, const char *csv_header,
                                   bool write_header_once) {
  csv_exporter_t csv = {0};

  csv.output = file;
  csv.csv_header = csv_header;
  csv.in_array = false;
  csv.header_written = false;
  csv.write_header_once = write_header_once;

  csv.base.begin_object = csv_begin_object_impl;
  csv.base.end_object = csv_end_object_impl;
  csv.base.write_int = csv_write_int_impl;
  csv.base.write_double = csv_write_double_impl;
  csv.base.write_string = csv_write_string_impl;
  csv.base.write_bool = csv_write_bool_impl;
  csv.base.write_null = csv_write_null_impl;
  csv.base.begin_array = csv_begin_array_impl;
  csv.base.end_array = csv_end_array_impl;
  csv.base.flush = csv_flush_impl;
  csv.base.destroy = csv_destroy_impl;

  return csv;
}

/*----------------------JSON EXPORTER----------------------*/

// Helper: Fully escaping strings according to the JSON standard
static inline void json_escape_string(FILE *out, const char *str) {
  fputc('"', out);
  if (str) {
    for (const char *p = str; *p; ++p) {
      switch (*p) {
      case '"':
        fprintf(out, "\\\"");
        break;
      case '\\':
        fprintf(out, "\\\\");
        break;
      case '\b':
        fprintf(out, "\\b");
        break;
      case '\f':
        fprintf(out, "\\f");
        break;
      case '\n':
        fprintf(out, "\\n");
        break;
      case '\r':
        fprintf(out, "\\r");
        break;
      case '\t':
        fprintf(out, "\\t");
        break;
      default:
        if ((unsigned char)*p < 0x20) {
          fprintf(out, "\\u%04x", (unsigned char)*p);
        } else {
          fputc(*p, out);
        }
      }
    }
  }

  fputc('"', out);
}

/// Prints \n + 2-space indent  * level
static int json_pretty_indent(json_exporter_t *json, int level) {
  if (!json->pretty)
    return 0;

  if (fputc('\n', json->output) == EOF)
    return -1;

  int spaces = level * 2;
  for (int i = 0; i < spaces; ++i) {
    if (fputc(' ', json->output) == EOF)
      return -1;
  }

  return 0;
}

static int json_grow_stack(json_exporter_t *json) {
  if (json->depth < json->capacity)
    return 0;

  int new_cap = (json->capacity == 0) ? 8 : json->capacity * 2;

  bool *new_ctx =
      (bool *)realloc(json->context_is_object, (size_t)new_cap * sizeof(bool));
  bool *new_fst =
      (bool *)realloc(json->level_first, (size_t)new_cap * sizeof(bool));

  if (new_ctx == NULL || new_fst == NULL) {
    free(new_ctx);
    free(new_fst);
    return -1;
  }

  json->context_is_object = new_ctx;
  json->level_first = new_fst;
  json->capacity = new_cap;

  return 0;
}

static int json_flush_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output)
    return -1;

  return fflush(json->output);
}

static int json_begin_object_impl(exporter_t *self, const char *name) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output)
    return -1;

  if (json->depth > 0) {
    int curr_level = json->depth - 1;
    if (!json->level_first[curr_level]) {
      if (fputc(',', json->output) == EOF)
        return -1;
    }

    if (json_pretty_indent(json, json->depth) != 0)
      return -1;
    if (json->context_is_object[curr_level] && name != NULL) {
      if (fprintf(json->output, "\"%s\":%s", name, json->pretty ? " " : "") < 0)
        return -1;
    }
    json->level_first[curr_level] = false;
  }

  if (json_grow_stack(json) != 0)
    return -1;

  if (fputc('{', json->output) == EOF)
    return -1;

  json->context_is_object[json->depth] = true;
  json->level_first[json->depth] = true;
  json->depth++;

  return 0;
}

static int json_end_object_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  if (json_pretty_indent(json, json->depth - 1) != 0)
    return -1;

  if (fputc('}', json->output) == EOF)
    return -1;

  json->depth--;

  return 0;
}

static int json_write_int_impl(exporter_t *self, const char *key,
                               int64_t value) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF)
      return -1;
  }

  if (json_pretty_indent(json, json->depth) != 0)
    return -1;
  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0)
      return -1;
  }
  if (fprintf(json->output, "%lld", value) < 0)
    return -1;

  json->level_first[curr_level] = false;

  return 0;
}

static int json_write_double_impl(exporter_t *self, const char *key,
                                  double value) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF)
      return -1;
  }
  if (json_pretty_indent(json, json->depth) != 0)
    return -1;
  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0)
      return -1;
  }
  if (fprintf(json->output, "%f", value) < 0)
    return -1;

  json->level_first[curr_level] = false;

  return 0;
}

static int json_write_string_impl(exporter_t *self, const char *key,
                                  const char *value) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF)
      return -1;
  }
  if (json_pretty_indent(json, json->depth) != 0)
    return -1;
  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0)
      return -1;
  }

  json_escape_string(json->output, value);
  json->level_first[curr_level] = false;

  return 0;
}

static int json_write_bool_impl(exporter_t *self, const char *key, bool value) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF)
      return -1;
  }
  if (json_pretty_indent(json, json->depth) != 0)
    return -1;
  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0)
      return -1;
  }
  if (fprintf(json->output, "%s", value ? "true" : "false") < 0)
    return -1;

  json->level_first[curr_level] = false;

  return 0;
}

static int json_write_null_impl(exporter_t *self, const char *key) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF)
      return -1;
  }

  if (json_pretty_indent(json, json->depth) != 0)
    return -1;

  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0)
      return -1;
  }

  if (fprintf(json->output, "null") < 0)
    return -1;

  json->level_first[curr_level] = false;

  return 0;
}

static int json_begin_array_impl(exporter_t *self, const char *key) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output)
    return -1;

  if (json->depth > 0) {
    int curr_level = json->depth - 1;
    if (!json->level_first[curr_level]) {
      if (fputc(',', json->output) == EOF)
        return -1;
    }

    if (json_pretty_indent(json, json->depth) != 0)
      return -1;

    if (json->context_is_object[curr_level] && key != NULL) {
      if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0)
        return -1;
    }
    json->level_first[curr_level] = false;
  }

  if (json_grow_stack(json) != 0)
    return -1;

  if (fputc('[', json->output) == EOF)
    return -1;

  json->context_is_object[json->depth] = false; /* массив */
  json->level_first[json->depth] = true;
  json->depth++;

  return 0;
}

static int json_end_array_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  if (json_pretty_indent(json, json->depth - 1) != 0)
    return -1;

  if (fputc(']', json->output) == EOF)
    return -1;

  json->depth--;

  return 0;
}

static void json_destroy_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json)
    return;
  free(json->context_is_object);
  free(json->level_first);
}

json_exporter_t create_json_exporter(FILE *file, bool pretty) {
  json_exporter_t json = {0};

  json.output = file;
  json.depth = 0;
  json.capacity = 0;
  json.context_is_object = NULL;
  json.level_first = NULL;
  json.pretty = pretty;

  json.base.begin_object = json_begin_object_impl;
  json.base.end_object = json_end_object_impl;
  json.base.write_int = json_write_int_impl;
  json.base.write_double = json_write_double_impl;
  json.base.write_string = json_write_string_impl;
  json.base.write_bool = json_write_bool_impl;
  json.base.write_null = json_write_null_impl;
  json.base.begin_array = json_begin_array_impl;
  json.base.end_array = json_end_array_impl;
  json.base.flush = json_flush_impl;
  json.base.destroy = json_destroy_impl;

  return json;
}

/*----------------------JSON EXPORTER----------------------*/

#endif // EXPORT_IMPLEMENTATION

#endif // EXPORT_H
