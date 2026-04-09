#ifndef EXPORT_H
#define EXPORT_H

#include <stdbool.h>
#include <stddef.h>
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
   * @param name Name of the structure/object (may be NULL)
   * @return 0 as success, otherwise - error
   */
  int (*begin_object)(exporter_t *self, const char *name);

  /**
   * @brief Ends writing a structure/object
   * @return 0 as success, otherwise - error
   */
  int (*end_object)(exporter_t *self);

  /**
   * @brief Write functions
   * @param key Field name (used as a key within the object).Can be NULL
   * @return 0 as success, otherwise - error
   */
  int (*write_int)(exporter_t *self, const char *key, int64_t value);
  int (*write_double)(exporter_t *self, const char *key, double value);
  int (*write_string)(exporter_t *self, const char *key, const char *value);
  int (*write_bool)(exporter_t *self, const char *key, bool value);
  // int (*write_null)(exporter_t *self, const char *key); TODO

  /**
   * @brief Begins writing an array for the specified key
   * @param key Field name (used as a key within the object).Can be NULL
   * @return 0 as success, otherwise - error
   */
  int (*begin_array)(exporter_t *self, const char *key);

  /// @brief Ends writing an array
  /// @return 0 as success, otherwise - erro
  int (*end_array)(exporter_t *self);

  /// @brief Flush data into output
  /// @return 0 as success, otherwise - error
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

} csv_exporter_t;

/// @brief Creates a new exporter in CSV format
/// @param file output file (stdout, fopen(...) etc)
csv_exporter_t create_csv_exporter(FILE *file, const char *csv_header);

#define MAX_JSON_DEPTH 32

typedef struct json_exporter_t {
  exporter_t base;
  FILE *output;
  int depth;
  bool context_is_object[MAX_JSON_DEPTH];
  bool level_first[MAX_JSON_DEPTH];

} json_exporter_t;

/// @brief Creates a new exporter in JSON format
/// @param file output file (stdout, fopen(...) etc)
json_exporter_t create_json_exporter(FILE *file);

#ifdef EXPORT_IMPLEMENTATION

/*----------------------CSV EXPORTER----------------------*/

static int csv_flush_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  return fflush(csv->output);
}

// Write csv header
int csv_begin_object_impl(exporter_t *self, const char *name) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  csv->is_first = true;
  return fprintf(csv->output, "%s\n", csv->csv_header);
}

// Add new line
int csv_end_object_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  csv->is_first = false;
  return putc('\n', csv->output);
}

static int csv_write_int_impl(exporter_t *self, const char *key,
                              int64_t value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first)
    return putc((csv->in_array) ? ',' : ';', csv->output) |
           fprintf(csv->output, "%lld", value);

  csv->is_first = false;
  return fprintf(csv->output, "%lld", value);
}

static int csv_write_double_impl(exporter_t *self, const char *key,
                                 double value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first)
    return putc((csv->in_array) ? ',' : ';', csv->output) |
           fprintf(csv->output, "%f", value);

  csv->is_first = false;
  return fprintf(csv->output, "%f", value);
}

static int csv_write_string_impl(exporter_t *self, const char *key,
                                 const char *value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first)
    return putc((csv->in_array) ? ',' : ';', csv->output) |
           fprintf(csv->output, "%s", value);

  csv->is_first = false;
  return fprintf(csv->output, "%s", value);
}

static int csv_write_bool_impl(exporter_t *self, const char *key, bool value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  if (!csv->is_first)
    return putc((csv->in_array) ? ',' : ';', csv->output) |
           fprintf(csv->output, "%s", (value) ? "true" : "false");

  csv->is_first = false;
  return fprintf(csv->output, "%s", (value) ? "true" : "false");
}

static int csv_begin_array_impl(exporter_t *self, const char *key) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  int result = 0;
  if (!csv->is_first)
    result |= putc(';', csv->output);

  csv->in_array = true;
  csv->is_first = true;

  return (result | putc('[', csv->output));
}

static int csv_end_array_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output)
    return -1;

  csv->in_array = false;
  csv->is_first = false;

  return putc(']', csv->output);
}

// dummy function
static void csv_destroy_impl(exporter_t *self) { (void)self; }

csv_exporter_t create_csv_exporter(FILE *file, const char *csv_header) {
  csv_exporter_t csv = {0};

  csv.output = file;
  csv.csv_header = csv_header;
  csv.in_array = false;

  csv.base.begin_object = csv_begin_object_impl;
  csv.base.end_object = csv_end_object_impl;
  csv.base.write_int = csv_write_int_impl;
  csv.base.write_double = csv_write_double_impl;
  csv.base.write_string = csv_write_string_impl;
  csv.base.write_bool = csv_write_bool_impl;
  csv.base.begin_array = csv_begin_array_impl;
  csv.base.end_array = csv_end_array_impl;
  csv.base.flush = csv_flush_impl;
  csv.base.destroy = csv_destroy_impl;

  return csv;
}

/*----------------------CSV EXPORTER----------------------*/

/*----------------------JSON EXPORTER---------------------*/

static int json_flush_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output)
    return -1;

  return fflush(json->output);
}

static int json_begin_object_impl(exporter_t *self, const char *name) {
  json_exporter_t *json = (json_exporter_t *)self;
  int result = 0;
  if (!json || !json->output)
    return -1;

  if (json->depth > 0) {
    int curr_level = json->depth - 1;
    if (!json->level_first[curr_level])
      result |= fputc(',', json->output);

    if (json->context_is_object[curr_level] && name != NULL)
      result |= fprintf(json->output, "\"%s\":", name);

    json->level_first[curr_level] = false;
  }

  result |= fputc('{', json->output);

  if (json->depth >= MAX_JSON_DEPTH)
    return -1;

  json->context_is_object[json->depth] = true;
  json->level_first[json->depth] = true;
  json->depth++;

  return result;
}

static int json_end_object_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int result = fputc('}', json->output);
  json->depth--;
  return result;
}

static int json_write_int_impl(exporter_t *self, const char *key,
                               int64_t value) {
  json_exporter_t *json = (json_exporter_t *)self;
  int result = 0;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level])
    result |= fputc(',', json->output);

  if (json->context_is_object[curr_level] && key != NULL) {
    result |= fprintf(json->output, "\"%s\":", key);
  }

  result |= fprintf(json->output, "%lld", value);

  json->level_first[curr_level] = false;
  return result;
}

static int json_write_double_impl(exporter_t *self, const char *key,
                                  double value) {
  json_exporter_t *json = (json_exporter_t *)self;
  int result = 0;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level])
    result |= fputc(',', json->output);

  if (json->context_is_object[curr_level] && key != NULL)
    result |= fprintf(json->output, "\"%s\":", key);

  result |= fprintf(json->output, "%f", value);

  json->level_first[curr_level] = false;
  return result;
}

static int json_write_string_impl(exporter_t *self, const char *key,
                                  const char *value) {
  json_exporter_t *json = (json_exporter_t *)self;
  int result = 0;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level])
    result |= fputc(',', json->output);

  if (json->context_is_object[curr_level] && key != NULL)
    result |= fprintf(json->output, "\"%s\":", key);

  result |= fprintf(json->output, "\"%s\"", value ? value : "");

  json->level_first[curr_level] = false;
  return result;
}

static int json_write_bool_impl(exporter_t *self, const char *key, bool value) {
  json_exporter_t *json = (json_exporter_t *)self;
  int result = 0;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level])
    result |= fputc(',', json->output);

  if (json->context_is_object[curr_level] && key != NULL)
    result |= fprintf(json->output, "\"%s\":", key);

  result |= fprintf(json->output, "%s", value ? "true" : "false");

  json->level_first[curr_level] = false;
  return result;
}

static int json_begin_array_impl(exporter_t *self, const char *key) {
  json_exporter_t *json = (json_exporter_t *)self;
  int result = 0;
  if (!json || !json->output)
    return -1;

  if (json->depth > 0) {
    int curr_level = json->depth - 1;
    if (!json->level_first[curr_level])
      result |= fputc(',', json->output);

    if (json->context_is_object[curr_level] && key != NULL)
      result |= fprintf(json->output, "\"%s\":", key);

    json->level_first[curr_level] = false;
  }

  result |= fputc('[', json->output);

  if (json->depth >= MAX_JSON_DEPTH)
    return -1;

  json->context_is_object[json->depth] = false; // array
  json->level_first[json->depth] = true;
  json->depth++;

  return result;
}

static int json_end_array_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0)
    return -1;

  int result = fputc(']', json->output);
  json->depth--;
  return result;
}

// dummy function
static void json_destroy(exporter_t *self) { (void)self; }

json_exporter_t create_json_exporter(FILE *file) {
  json_exporter_t json = {0};

  json.output = file;
  json.depth = 0;

  json.base.begin_object = json_begin_object_impl;
  json.base.end_object = json_end_object_impl;
  json.base.write_int = json_write_int_impl;
  json.base.write_double = json_write_double_impl;
  json.base.write_string = json_write_string_impl;
  json.base.write_bool = json_write_bool_impl;
  json.base.begin_array = json_begin_array_impl;
  json.base.end_array = json_end_array_impl;
  json.base.flush = json_flush_impl;
  json.base.destroy = json_destroy;

  return json;
}

/*----------------------JSON EXPORTER---------------------*/

#endif // EXPORT_IMPLEMENTATION

#endif // EXPORT_H
