#ifndef EXPORT_H
#define EXPORT_H

// TODO:
// - SQLITE
// - Raw format
// - Errors handling

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
   *
   * @param name Name of the structure/object (may be NULL)
   *
   * @return 0 on success, otherwise -1 on error
   */
  int (*begin_object)(exporter_t *self, const char *name);

  /**
   * @brief Ends writing a structure/object
   *
   * @return 0 on success, otherwise -1 on error
   */
  int (*end_object)(exporter_t *self);

  /**
   * @brief Write functions
   *
   * @param key Field name (used as a key within the object). Can be NULL
   *
   * @return 0 on success, otherwise -1 on error
   */
  int (*write_int)(exporter_t *self, const char *key, int64_t value);
  int (*write_double)(exporter_t *self, const char *key, double value);
  int (*write_string)(exporter_t *self, const char *key, const char *value);
  int (*write_bool)(exporter_t *self, const char *key, bool value);
  int (*write_null)(exporter_t *self, const char *key);

  /**
   * @brief Begins writing an array for the specified key
   *
   * @param key Field name (used as a key within the object). Can be NULL
   *
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

/*------------------------CSV EXPORTER-----------------------*/

typedef struct csv_exporter_t {
  exporter_t base;
  FILE *output;
  const char *csv_header;
  bool in_array;
  bool is_first;
  bool header_written;
  bool write_header_once;

} csv_exporter_t;

/**
 * @brief Creates a new exporter in CSV format
 *
 * @param file output file (stdout, fopen(...) etc)
 * @param write_header_once if true, header is written only once (recommended
 * for multi-row CSV)
 */
csv_exporter_t create_csv_exporter(FILE *file, const char *csv_header,
                                   bool write_header_once);

/**
 * @brief Sets whether the CSV header should be written only once
 *
 * @param csv Pointer to csv_exporter_t
 * @param write_header_once if true — write header only on first begin_object
 * call; if false — write header on every begin_object call
 *
 * @return 0 on success, -1 on error (e.g. csv is NULL)
 */
int csv_exporter_set_write_header_once(csv_exporter_t *csv,
                                       bool write_header_once);

/**
 * @brief Sets the CSV header string
 *
 * @param csv Pointer to csv_exporter_t
 * @param csv_header The header line (comma-separated column names). The string
 * is not copied — caller must ensure it remains valid
 *
 * @return 0 on success, -1 on error (e.g. csv is NULL)
 */
int csv_exporter_set_csv_header(csv_exporter_t *csv, const char *csv_header);

/**
 * @brief Sets the output FILE* for the CSV exporter
 *
 * @note This function resets internal states
 * @param csv Pointer to csv_exporter_t
 * @param file Output stream (e.g., stdout, fopen(...))
 *
 * @return 0 on success, -1 on error (e.g. csv or file is NULL)
 */
int csv_exporter_set_output(csv_exporter_t *csv, FILE *file);

/*------------------------CSV EXPORTER-----------------------*/

/*-----------------------JSON EXPORTER-----------------------*/

typedef struct json_exporter_t {
  exporter_t base;
  FILE *output;
  int depth;
  int capacity;
  bool *context_is_object;
  bool *level_first;
  bool pretty; ///<- if true — pretty-print JSON (2-space indent + newlines)
  bool jsonl; ///<- if true — JSONL mode (each object on separate line, no outer
              ///< array)

} json_exporter_t;

/**
 * @brief Creates a new exporter in JSON format
 *
 * @param file output file (stdout, fopen(...) etc)
 * @param pretty if true, outputs pretty-printed JSON (newlines + 2-space
 * indentation)
 * @param jsonl if true, enables JSONL mode (each object on a separate line)
 */
json_exporter_t create_json_exporter(FILE *file, bool pretty, bool jsonl);

/**
 * @brief Sets pretty-print mode for JSON output
 *
 * @param json Pointer to json_exporter_t
 * @param pretty if true — enable pretty-printed JSON (newlines + 2-space
 * indent); if false — compact JSON on a single line
 *
 * @return 0 on success, -1 on error (e.g. json is NULL)
 */
int json_exporter_set_pretty(json_exporter_t *json, bool pretty);

/**
 * @brief Sets JSONL mode for JSON output
 *
 * @param json Pointer to json_exporter_t
 * @param jsonl if true — enable JSONL mode (each object on separate line,
 * no outer wrapping); if false — standard JSON mode
 *
 * @return 0 on success, -1 on error (e.g. json is NULL)
 */
int json_exporter_set_jsonl(json_exporter_t *json, bool jsonl);

/**
 * @brief Sets the output FILE* for the JSON exporter and resets internal state
 * @note This function frees internal buffers and resets depth to 0,
 * effectively starting a fresh export
 *
 * @param json Pointer to json_exporter_t
 * @param file Output stream (e.g., stdout, fopen(...))
 *
 * @return 0 on success, -1 on error (e.g. json or file is NULL)
 */
int json_exporter_set_output(json_exporter_t *json, FILE *file);

/*-----------------------JSON EXPORTER-----------------------*/

/*----------------------SQLITE EXPORTER----------------------*/

#ifdef SQLITE_EXPORT

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef struct sqlite_exporter_t {
  exporter_t base;          ///
  sqlite3 *db;              ///< SQLite database connection
  sqlite3_stmt *stmt;       ///< Prepared INSERT statement
  const char *table_name;   ///
  const char *column_names; ///< Semicolon-separated column names
  int column_count;         ///
  int current_column;       ///< Current column index being filled
  bool in_object;           ///
  bool is_first_column;     ///
  bool in_transaction;      ///
  bool table_created;       ///

} sqlite_exporter_t;

sqlite_exporter_t create_sqlite_exporter(sqlite3 *db, const char *table_name,
                                         const char *column_names);

#endif

/*----------------------SQLITE EXPORTER----------------------*/

#ifdef EXPORT_IMPLEMENTATION

#include <stdlib.h>

/*------------------------CSV EXPORTER-----------------------*/

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

int csv_exporter_set_write_header_once(csv_exporter_t *csv,
                                       bool write_header_once) {
  if (!csv)
    return -1;

  csv->write_header_once = write_header_once;

  return 0;
}

int csv_exporter_set_csv_header(csv_exporter_t *csv, const char *csv_header) {
  if (!csv)
    return -1;

  csv->csv_header = csv_header;

  return 0;
}

int csv_exporter_set_output(csv_exporter_t *csv, FILE *file) {
  if (!csv || !file)
    return -1;

  csv->output = file;
  csv->in_array = false;
  csv->is_first = true;
  csv->header_written = false;

  return 0;
}

/*------------------------CSV EXPORTER-----------------------*/

/*-----------------------JSON EXPORTER-----------------------*/

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

  // JSONL mode: at depth 0, just start the object
  if (json->jsonl && json->depth == 0) {
    if (fputc('{', json->output) == EOF)
      return -1;

    if (json_grow_stack(json) != 0)
      return -1;

    json->context_is_object[json->depth] = true;
    json->level_first[json->depth] = true;
    json->depth++;

    return 0;
  }

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

  json->depth--;

  // JSONL mode: at depth 0, end object and add newline
  if (json->jsonl && json->depth == 0) {
    if (fputc('}', json->output) == EOF)
      return -1;
    if (fputc('\n', json->output) == EOF)
      return -1;
    return 0;
  }

  if (json_pretty_indent(json, json->depth) != 0)
    return -1;

  if (fputc('}', json->output) == EOF)
    return -1;

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

  json->context_is_object[json->depth] = false;
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

json_exporter_t create_json_exporter(FILE *file, bool pretty, bool jsonl) {
  json_exporter_t json = {0};

  json.output = file;
  json.depth = 0;
  json.capacity = 0;
  json.context_is_object = NULL;
  json.level_first = NULL;
  json.pretty = pretty;
  json.jsonl = jsonl;

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

int json_exporter_set_pretty(json_exporter_t *json, bool pretty) {
  if (!json)
    return -1;

  json->pretty = pretty;

  return 0;
}

int json_exporter_set_jsonl(json_exporter_t *json, bool jsonl) {
  if (!json)
    return -1;

  json->jsonl = jsonl;

  return 0;
}

int json_exporter_set_output(json_exporter_t *json, FILE *file) {
  if (!json || !file)
    return -1;

  free(json->context_is_object);
  free(json->level_first);

  json->output = file;
  json->depth = 0;
  json->capacity = 0;
  json->context_is_object = NULL;
  json->level_first = NULL;
  json->jsonl = false;

  return 0;
}

/*-----------------------JSON EXPORTER-----------------------*/

/*----------------------SQLITE EXPORTER----------------------*/

#ifdef SQLITE_EXPORT

#include <sqlite3.h>
#include <string.h>

static inline int sqlite_ensure_table_created(sqlite_exporter_t *sqlite) {
  if (!sqlite || sqlite->table_created)
    return 0;

  if (!sqlite->column_names || !sqlite->table_name)
    return -1;

  // Build CREATE TABLE statement
  // column_names is semicolon-separated: "id;name;value"
  // We need: "id INTEGER, name TEXT, value REAL" (all as TEXT for simplicity)
  char *sql = NULL;
  size_t sql_len = 0;

  // Count columns and build column definitions
  const char *cols = sqlite->column_names;
  size_t cols_len = strlen(cols);

  // TODO:
  //  Estimate buffer size: "CREATE TABLE IF NOT EXISTS " + table + " (" +
  //  cols*10 + ");"
  sql_len = 64 + strlen(sqlite->table_name) + cols_len * 16;
  sql = (char *)malloc(sql_len);
  if (!sql)
    return -1;

  // Start CREATE TABLE statement
  int offset = snprintf(sql, sql_len, "CREATE TABLE IF NOT EXISTS %s (",
                        sqlite->table_name);
  if (offset < 0) {
    free(sql);
    return -1;
  }

  // Parse column names and create TEXT columns
  const char *start = cols;
  const char *end = cols;
  bool first_col = true;

  while (1) {
    if (*end == ';' || *end == '\0') {
      size_t col_len = (size_t)(end - start);

      if (!first_col) {
        offset = snprintf(sql + offset, sql_len - offset, ", ");
        if (offset < 0) {
          free(sql);
          return -1;
        }
      }

      // Write column name as TEXT type
      offset = snprintf(sql + offset, sql_len - offset, "%.*s TEXT",
                        (int)col_len, start);
      if (offset < 0) {
        free(sql);
        return -1;
      }

      first_col = false;

      if (*end == '\0')
        break;
      start = end + 1;
    }
    end++;
  }

  // Close statement
  offset = snprintf(sql + offset, sql_len - offset, ")");
  if (offset < 0) {
    free(sql);
    return -1;
  }

  // Execute CREATE TABLE
  char *err_msg = NULL;
  int rc = sqlite3_exec(sqlite->db, sql, NULL, NULL, &err_msg);
  free(sql);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQLite error: %s\n", err_msg); // TODO:
    sqlite3_free(err_msg);
    return -1;
  }

  sqlite->table_created = true;

  return 0;
}

static inline int sqlite_prepare_insert_stmt(sqlite_exporter_t *sqlite) {
  if (!sqlite || !sqlite->column_names || !sqlite->table_name)
    return -1;

  // If statement already exists, skip
  if (sqlite->stmt)
    return 0;

  // Build INSERT statement: INSERT INTO table (col1, col2, ...) VALUES (?, ?,
  // ...) Count columns first
  int col_count = 1;
  for (const char *p = sqlite->column_names; *p; ++p) {
    if (*p == ';')
      col_count++;
  }

  sqlite->column_count = col_count;

  // TODO:
  // Estimate buffer size
  size_t cols_len = strlen(sqlite->column_names);
  size_t sql_len = 64 + strlen(sqlite->table_name) + cols_len + col_count * 4;
  char *sql = (char *)malloc(sql_len);
  if (!sql)
    return -1;

  //  Start: INSERT INTO table (
  int offset = snprintf(sql, sql_len, "INSERT INTO %s (", sqlite->table_name);
  if (offset < 0) {
    free(sql);
    return -1;
  }

  // Column names
  const char *start = sqlite->column_names;
  const char *end = sqlite->column_names;
  bool first = true;

  while (1) {
    if (*end == ';' || *end == '\0') {
      size_t col_len = (size_t)(end - start);

      if (!first) {
        offset = snprintf(sql + offset, sql_len - offset, ", ");
        if (offset < 0) {
          free(sql);
          return -1;
        }
      }

      offset =
          snprintf(sql + offset, sql_len - offset, "%.*s", (int)col_len, start);
      if (offset < 0) {
        free(sql);
        return -1;
      }

      first = false;

      if (*end == '\0')
        break;
      start = end + 1;
    }
    end++;
  }

  // VALUES part
  offset = snprintf(sql + offset, sql_len - offset, ") VALUES (");
  if (offset < 0) {
    free(sql);
    return -1;
  }

  for (int i = 0; i < col_count; ++i) {
    if (i > 0) {
      offset = snprintf(sql + offset, sql_len - offset, ", ");
      if (offset < 0) {
        free(sql);
        return -1;
      }
    }
    offset = snprintf(sql + offset, sql_len - offset, "?");
    if (offset < 0) {
      free(sql);
      return -1;
    }
  }

  offset = snprintf(sql + offset, sql_len - offset, ")");
  if (offset < 0) {
    free(sql);
    return -1;
  }

  // Prepare statement
  int rc = sqlite3_prepare_v2(sqlite->db, sql, -1, &sqlite->stmt, NULL);
  free(sql);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQLite prepare error: %s\n",
            sqlite3_errmsg(sqlite->db)); // TODO:
    sqlite->stmt = NULL;
    return -1;
  }

  return 0;
}

static inline int sqlite_reset_stmt(sqlite_exporter_t *sqlite) {
  if (!sqlite || !sqlite->stmt)
    return -1;

  int rc = sqlite3_reset(sqlite->stmt);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQLite reset error: %s\n",
            sqlite3_errmsg(sqlite->db)); // TODO:
    return -1;
  }

  rc = sqlite3_clear_bindings(sqlite->stmt);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQLite clear bindings error: %s\n",
            sqlite3_errmsg(sqlite->db)); // TODO:
    return -1;
  }

  return 0;
}

static int sqlite_begin_object_impl(exporter_t *self, const char *name) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db)
    return -1;

  // Ensure table exists
  if (sqlite_ensure_table_created(sqlite) != 0)
    return -1;

  // Prepare INSERT statement if not already
  if (sqlite_prepare_insert_stmt(sqlite) != 0)
    return -1;

  // Begin transaction if not active
  if (!sqlite->in_transaction) {
    char *err_msg = NULL;
    int rc =
        sqlite3_exec(sqlite->db, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQLite BEGIN error: %s\n", err_msg); // TODO:
      sqlite3_free(err_msg);
      return -1;
    }
    sqlite->in_transaction = true;
  }

  // Reset statement for new row
  if (sqlite_reset_stmt(sqlite) != 0)
    return -1;

  // Reset column tracking
  sqlite->current_column = 0;
  sqlite->is_first_column = true;
  sqlite->in_object = true;

  (void)name; // We don't care about name

  return 0;
}

static int sqlite_end_object_impl(exporter_t *self) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db || !sqlite->stmt)
    return -1;

  // Execute the prepared statement
  int rc = sqlite3_step(sqlite->stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "SQLite step error: %s\n",
            sqlite3_errmsg(sqlite->db)); // TODO:
    return -1;
  }

  sqlite->in_object = false;

  // Note: Don't reset statement here - it will be reset on next begin_object
  // This allows the statement to be reused efficiently

  return 0;
}

sqlite_exporter_t create_sqlite_exporter(sqlite3 *db, const char *table_name,
                                         const char *column_names) {

  sqlite_exporter_t sqlite = {0};

  sqlite.db = db;
  sqlite.table_name = table_name;
  sqlite.column_names = column_names;
  sqlite.in_object = false;
  sqlite.is_first_column = true;
  sqlite.stmt = NULL;
  sqlite.column_count = 0;
  sqlite.current_column = 0;
  sqlite.table_created = false;
  sqlite.in_transaction = false;

  // TODO:
  sqlite.base.begin_object = NULL;
  sqlite.base.end_object = NULL;
  sqlite.base.write_int = NULL;
  sqlite.base.write_double = NULL;
  sqlite.base.write_string = NULL;
  sqlite.base.write_bool = NULL;
  sqlite.base.write_null = NULL;
  sqlite.base.begin_array = NULL;
  sqlite.base.end_array = NULL;
  sqlite.base.flush = NULL;
  sqlite.base.destroy = NULL;

  // Parse column_names to count columns
  if (column_names) {
    int col_count = 1;
    for (const char *p = column_names; *p; ++p) {
      if (*p == ';')
        col_count++;
    }
    sqlite.column_count = col_count;
  }

  return sqlite;
}

#endif // SQLITE_EXPORT

/*----------------------SQLITE EXPORTER----------------------*/

#endif // EXPORT_IMPLEMENTATION

#endif // EXPORT_H
