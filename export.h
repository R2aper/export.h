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

#endif // EXPORT_IMPLEMENTATION

#endif // EXPORT_H
