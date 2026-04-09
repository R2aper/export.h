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

#endif // EXPORT_H
