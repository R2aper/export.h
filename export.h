#ifndef EXPORT_H
#define EXPORT_H

// TODO:
// - Raw format

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <threads.h>

/*----------------------ERROR HANDLING-----------------------*/

/// @brief Maximum size of error message buffer
#ifndef EXPORT_ERR_MSG_SIZE
#define EXPORT_ERR_MSG_SIZE 512
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define EXPORT_THREAD_LOCAL thread_local
#elif defined(_MSC_VER)
#define EXPORT_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define EXPORT_THREAD_LOCAL __thread
#else
#define EXPORT_THREAD_LOCAL
#pragma message(                                                               \
    "Thread-local storage not supported on this platform - error handling is not thread-safe")
#endif

/// @brief Error codes for export operations
typedef enum {
  EXPORT_OK = 0,         ///< No error
  EXPORT_ERR_NULL_PARAM, ///< NULL parameter passed
  EXPORT_ERR_IO,         ///< File I/O error (EOF, fwrite/fflush failed)
  EXPORT_ERR_MEMORY,     ///< Memory allocation failed (malloc/realloc)
  EXPORT_ERR_SQLITE,     ///< SQLite database error
  EXPORT_ERR_STATE,      ///< Invalid exporter state (e.g., end_object without
                         ///< begin_object)
  EXPORT_ERR_SERIALIZE,  ///< Serialization error

} export_error_code_t;

/**
 * @brief Last error code (thread-local)
 *
 * Similar to errno, this variable holds the last error code set by any export
 * function. Thread-local when supported by the compiler
 */
extern EXPORT_THREAD_LOCAL export_error_code_t export_last_error;

/**
 * @brief Last error message (thread-local)
 *
 * Contains a human-readable description of the last error. Thread-local when
 * supported by the compiler
 */
extern EXPORT_THREAD_LOCAL char export_last_error_msg[EXPORT_ERR_MSG_SIZE];

/**
 * @brief Returns a human-readable string for an error code
 *
 * @param code Error code from export_error_code_t
 * @return Static string describing the error
 */
const char *export_strerror(export_error_code_t code);

/**
 * @brief Returns the last error message from any export function
 *
 * @return Static string with the last error message (thread-local)
 */
const char *export_get_last_error(void);

/// @brief Clear error
static inline void export_clear_error(void) {
  export_last_error = EXPORT_OK;
  export_last_error_msg[0] = '\0';
}

/*----------------------ERROR HANDLING-----------------------*/

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
  bool pretty; ///<- if true - pretty-print JSON (2-space indent + newlines)
  bool
      jsonl; ///<- if true  - JSONL mode (each object on separate line, no outer
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
 * @param pretty if true - enable pretty-printed JSON (newlines + 2-space
 * indent); if false - compact JSON on a single line
 *
 * @return 0 on success, -1 on error (e.g. json is NULL)
 */
int json_exporter_set_pretty(json_exporter_t *json, bool pretty);

/**
 * @brief Sets JSONL mode for JSON output
 *
 * @param json Pointer to json_exporter_t
 * @param jsonl if true - enable JSONL mode (each object on separate line,
 * no outer wrapping); if false - standard JSON mode
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
  const char *column_names; ///< Semicolon-separated column names and types
  int column_count;         ///
  int current_column;       ///< Current column index being filled
  bool auto_commit;         ///< If true, commit after each object (default)
  bool in_object;           ///
  bool is_first_column;     ///
  bool in_transaction;      ///
  bool table_created;       ///

} sqlite_exporter_t;

/**
 * @brief Creates a new exporter in SQLite format
 * @note  column_names should be in format: "Name1:Type1;Name2:Type2".
 * Type is optional, defaults to TEXT if omitted or empty.
 *
 * @param db Open sqlite3 database
 * @param table_name Name of table
 * @param column_names List of column names
 */
sqlite_exporter_t create_sqlite_exporter(sqlite3 *db, const char *table_name,
                                         const char *column_names);

/**
 * @brief Sets auto-commit mode
 *
 * @param sqlite Pointer to sqlite_exporter_t
 * @param auto_commit if true (default) - commit after each end_object();
 *                    if false - caller must commit via flush() manually
 *
 * @return 0 on success, -1 on error
 */
int sqlite_exporter_set_auto_commit(sqlite_exporter_t *sqlite,
                                    bool auto_commit);

#endif

/*----------------------SQLITE EXPORTER----------------------*/

#ifdef EXPORT_IMPLEMENTATION

#include <stdarg.h>
#include <stdlib.h>

/*----------------------ERROR HANDLING-----------------------*/

// Thread-local error storage
EXPORT_THREAD_LOCAL export_error_code_t export_last_error = EXPORT_OK;
EXPORT_THREAD_LOCAL char export_last_error_msg[EXPORT_ERR_MSG_SIZE] = {0};

// Internal helper: sets error code and message
static inline void export_set_error(export_error_code_t code, const char *fmt,
                                    ...) {
  export_last_error = code;
  va_list args;
  va_start(args, fmt);
  vsnprintf(export_last_error_msg, EXPORT_ERR_MSG_SIZE, fmt, args);
  va_end(args);
}

const char *export_strerror(export_error_code_t code) {
  switch (code) {
  case EXPORT_OK:
    return "No error";
  case EXPORT_ERR_NULL_PARAM:
    return "NULL parameter passed";
  case EXPORT_ERR_IO:
    return "File I/O error";
  case EXPORT_ERR_MEMORY:
    return "Memory allocation failed";
  case EXPORT_ERR_SQLITE:
    return "SQLite database error";
  case EXPORT_ERR_STATE:
    return "Invalid exporter state";
  case EXPORT_ERR_SERIALIZE:
    return "Serialization error";
  default:
    return "Unknown error";
  }
}

const char *export_get_last_error(void) { return export_last_error_msg; }

/*----------------------ERROR HANDLING-----------------------*/

/*------------------------CSV EXPORTER-----------------------*/

// Helper: writes a properly quoted CSV string field according to RFC 4180
// (always quoted with ", internal " doubled)
static inline int csv_write_quoted_string(FILE *out, const char *value) {
  if (fputc('"', out) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write CSV quote character");
    return -1;
  }

  if (value) {
    for (const char *p = value; *p; ++p) {
      if (*p == '"') {
        if (fputc('"', out) == EOF || fputc('"', out) == EOF) {
          export_set_error(EXPORT_ERR_IO, "Failed to write escaped CSV quote");
          return -1;
        }
      } else {
        if (fputc(*p, out) == EOF) {
          export_set_error(EXPORT_ERR_IO, "Failed to write CSV string char");
          return -1;
        }
      }
    }
  }

  if (fputc('"', out) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write closing CSV quote");
    return -1;
  }

  return 0;
}

// Write csv header (only once if write_header_once == true)
static int csv_begin_object_impl(exporter_t *self, const char *name) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  csv->is_first = true;

  if (!csv->header_written || !csv->write_header_once) {
    if (fprintf(csv->output, "%s\n", csv->csv_header) < 0) {
      export_set_error(EXPORT_ERR_IO, "Failed to write CSV header");
      return -1;
    }
    csv->header_written = true;
  }

  return 0;
}

// Add new line
static int csv_end_object_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  csv->is_first = false;

  if (putc('\n', csv->output) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write CSV newline");
    return -1;
  }
  return 0;
}

static int csv_write_int_impl(exporter_t *self, const char *key,
                              int64_t value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write CSV separator");
      return -1;
    }
  }

  csv->is_first = false;

  if (fprintf(csv->output, "%lld", value) < 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write CSV integer");
    return -1;
  }
  return 0;
}

static int csv_write_double_impl(exporter_t *self, const char *key,
                                 double value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write CSV separator");
      return -1;
    }
  }

  csv->is_first = false;

  if (fprintf(csv->output, "%f", value) < 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write CSV double");
    return -1;
  }

  return 0;
}

static int csv_write_string_impl(exporter_t *self, const char *key,
                                 const char *value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write CSV separator");
      return -1;
    }
  }

  csv->is_first = false;

  return csv_write_quoted_string(csv->output, value);
}

static int csv_write_bool_impl(exporter_t *self, const char *key, bool value) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write CSV separator");
      return -1;
    }
  }

  csv->is_first = false;

  if (fprintf(csv->output, "%s", value ? "true" : "false") < 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write CSV boolean");
    return -1;
  }

  return 0;
}

static int csv_write_null_impl(exporter_t *self, const char *key) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write CSV separator");
      return -1;
    }
  }

  csv->is_first = false;

  if (fprintf(csv->output, "null") < 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write CSV null");
    return -1;
  }

  return 0;
}

static int csv_begin_array_impl(exporter_t *self, const char *key) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  if (!csv->is_first) {
    if (putc((csv->in_array) ? ',' : ';', csv->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write CSV separator");
      return -1;
    }
  }

  if (fputc('"', csv->output) == EOF || fputc('[', csv->output) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write CSV array start");
    return -1;
  }

  csv->in_array = true;
  csv->is_first = true;

  return 0;
}

static int csv_end_array_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  if (fputc(']', csv->output) == EOF || fputc('"', csv->output) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write CSV array end");
    return -1;
  }

  csv->in_array = false;
  csv->is_first = false;

  return 0;
}

static int csv_flush_impl(exporter_t *self) {
  csv_exporter_t *csv = (csv_exporter_t *)self;
  if (!csv || !csv->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or output is NULL");
    return -1;
  }

  if (fflush(csv->output) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to flush CSV output");
    return -1;
  }
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
  if (!csv) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter is NULL");
    return -1;
  }

  csv->write_header_once = write_header_once;

  return 0;
}

int csv_exporter_set_csv_header(csv_exporter_t *csv, const char *csv_header) {
  if (!csv) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter is NULL");
    return -1;
  }

  csv->csv_header = csv_header;

  return 0;
}

int csv_exporter_set_output(csv_exporter_t *csv, FILE *file) {
  if (!csv || !file) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "CSV exporter or file is NULL");
    return -1;
  }

  csv->output = file;
  csv->in_array = false;
  csv->is_first = true;
  csv->header_written = false;

  return 0;
}

/*------------------------CSV EXPORTER-----------------------*/

/*-----------------------JSON EXPORTER-----------------------*/

// Helper: Fully escaping strings according to the JSON standard
static inline int json_escape_string(FILE *out, const char *str) {
  if (fputc('"', out) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
    return -1;
  }
  if (str) {
    for (const char *p = str; *p; ++p) {
      switch (*p) {
      case '"':
        if (fprintf(out, "\\\"") < 0) {
          export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
          return -1;
        }
        break;
      case '\\':
        if (fprintf(out, "\\\\") < 0) {
          export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
          return -1;
        }
        break;
      case '\b':
        if (fprintf(out, "\\b") < 0) {
          export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
          return -1;
        }
        break;
      case '\f':
        if (fprintf(out, "\\f") < 0) {
          export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
          return -1;
        }
        break;
      case '\n':
        if (fprintf(out, "\\n") < 0) {
          export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
          return -1;
        }
        break;
      case '\r':
        if (fprintf(out, "\\r") < 0) {
          export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
          return -1;
        }
        break;
      case '\t':
        if (fprintf(out, "\\t") < 0) {
          export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
          return -1;
        }
        break;
      default:
        if ((unsigned char)*p < 0x20) {
          if (fprintf(out, "\\u%04x", (unsigned char)*p) < 0) {
            export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
            return -1;
          }
        } else {
          if (fputc(*p, out) == EOF) {
            export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
            return -1;
          }
        }
      }
    }
  }

  if (fputc('"', out) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON string");
    return -1;
  }

  return 0;
}

/// Prints \n + 2-space indent  * level
static inline int json_pretty_indent(json_exporter_t *json, int level) {
  if (!json->pretty)
    return 0;

  if (fputc('\n', json->output) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON newline");
    return -1;
  }

  int spaces = level * 2;
  for (int i = 0; i < spaces; ++i) {
    if (fputc(' ', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
      return -1;
    }
  }

  return 0;
}

static inline int json_grow_stack(json_exporter_t *json) {
  if (json->depth < json->capacity)
    return 0;

  int new_cap = (json->capacity == 0) ? 8 : json->capacity * 2;

  bool *new_ctx =
      (bool *)realloc(json->context_is_object, (size_t)new_cap * sizeof(bool));
  bool *new_fst =
      (bool *)realloc(json->level_first, (size_t)new_cap * sizeof(bool));

  if (!new_ctx || !new_fst) {
    free(new_ctx);
    free(new_fst);
    export_set_error(EXPORT_ERR_MEMORY,
                     "Failed to grow JSON stack (capacity=%d)", new_cap);
    return -1;
  }

  json->context_is_object = new_ctx;
  json->level_first = new_fst;
  json->capacity = new_cap;

  return 0;
}

static int json_begin_object_impl(exporter_t *self, const char *name) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "JSON exporter or output is NULL");
    return -1;
  }

  // JSONL mode: at depth 0, just start the object
  if (json->jsonl && json->depth == 0) {
    if (fputc('{', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON object start");
      return -1;
    }

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
      if (fputc(',', json->output) == EOF) {
        export_set_error(EXPORT_ERR_IO, "Failed to write JSON comma");
        return -1;
      }
    }

    if (json_pretty_indent(json, json->depth) != 0) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
      return -1;
    }
    if (json->context_is_object[curr_level] && name != NULL) {
      if (fprintf(json->output, "\"%s\":%s", name, json->pretty ? " " : "") <
          0) {
        export_set_error(EXPORT_ERR_IO, "Failed to write JSON key");
        return -1;
      }
    }
    json->level_first[curr_level] = false;
  }

  if (json_grow_stack(json) != 0)
    return -1;

  if (fputc('{', json->output) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON object start");
    return -1;
  }

  json->context_is_object[json->depth] = true;
  json->level_first[json->depth] = true;
  json->depth++;

  return 0;
}

static int json_end_object_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0) {
    export_set_error(EXPORT_ERR_STATE,
                     "JSON exporter: end_object without begin_object");
    return -1;
  }

  json->depth--;

  // JSONL mode: at depth 0, end object and add newline
  if (json->jsonl && json->depth == 0) {
    if (fputc('}', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON object end");
      return -1;
    }
    if (fputc('\n', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSONL newline");
      return -1;
    }
    return 0;
  }

  if (json_pretty_indent(json, json->depth) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
    return -1;
  }

  if (fputc('}', json->output) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON object end");
    return -1;
  }

  return 0;
}

static int json_write_int_impl(exporter_t *self, const char *key,
                               int64_t value) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0) {
    export_set_error(EXPORT_ERR_STATE,
                     "JSON exporter: write_int outside of object/array");
    return -1;
  }

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON comma");
      return -1;
    }
  }

  if (json_pretty_indent(json, json->depth) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
    return -1;
  }
  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON key");
      return -1;
    }
  }
  if (fprintf(json->output, "%lld", value) < 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON integer");
    return -1;
  }

  json->level_first[curr_level] = false;

  return 0;
}

static int json_write_double_impl(exporter_t *self, const char *key,
                                  double value) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0) {
    export_set_error(EXPORT_ERR_STATE,
                     "JSON exporter: write_double outside of object/array");
    return -1;
  }

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON comma");
      return -1;
    }
  }
  if (json_pretty_indent(json, json->depth) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
    return -1;
  }
  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON key");
      return -1;
    }
  }
  if (fprintf(json->output, "%f", value) < 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON double");
    return -1;
  }

  json->level_first[curr_level] = false;

  return 0;
}

static int json_write_string_impl(exporter_t *self, const char *key,
                                  const char *value) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0) {
    export_set_error(EXPORT_ERR_STATE,
                     "JSON exporter: write_string outside of object/array");
    return -1;
  }

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON comma");
      return -1;
    }
  }
  if (json_pretty_indent(json, json->depth) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
    return -1;
  }
  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON key");
      return -1;
    }
  }

  if (json_escape_string(json->output, value) != 0)
    return -1;

  json->level_first[curr_level] = false;

  return 0;
}

static int json_write_bool_impl(exporter_t *self, const char *key, bool value) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0) {
    export_set_error(EXPORT_ERR_STATE,
                     "JSON exporter: write_bool outside of object/array");
    return -1;
  }

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON comma");
      return -1;
    }
  }
  if (json_pretty_indent(json, json->depth) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
    return -1;
  }
  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON key");
      return -1;
    }
  }
  if (fprintf(json->output, "%s", value ? "true" : "false") < 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON boolean");
    return -1;
  }

  json->level_first[curr_level] = false;

  return 0;
}

static int json_write_null_impl(exporter_t *self, const char *key) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0) {
    export_set_error(EXPORT_ERR_STATE,
                     "JSON exporter: write_null outside of object/array");
    return -1;
  }

  int curr_level = json->depth - 1;

  if (!json->level_first[curr_level]) {
    if (fputc(',', json->output) == EOF) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON comma");
      return -1;
    }
  }

  if (json_pretty_indent(json, json->depth) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
    return -1;
  }

  if (json->context_is_object[curr_level] && key != NULL) {
    if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") < 0) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON key");
      return -1;
    }
  }

  if (fprintf(json->output, "null") < 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON null");
    return -1;
  }

  json->level_first[curr_level] = false;

  return 0;
}

static int json_begin_array_impl(exporter_t *self, const char *key) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "JSON exporter or output is NULL");
    return -1;
  }

  if (json->depth > 0) {
    int curr_level = json->depth - 1;
    if (!json->level_first[curr_level]) {
      if (fputc(',', json->output) == EOF) {
        export_set_error(EXPORT_ERR_IO, "Failed to write JSON comma");
        return -1;
      }
    }

    if (json_pretty_indent(json, json->depth) != 0) {
      export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
      return -1;
    }

    if (json->context_is_object[curr_level] && key != NULL) {
      if (fprintf(json->output, "\"%s\":%s", key, json->pretty ? " " : "") <
          0) {
        export_set_error(EXPORT_ERR_IO, "Failed to write JSON key");
        return -1;
      }
    }
    json->level_first[curr_level] = false;
  }

  if (json_grow_stack(json) != 0)
    return -1;

  if (fputc('[', json->output) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON array start");
    return -1;
  }

  json->context_is_object[json->depth] = false;
  json->level_first[json->depth] = true;
  json->depth++;

  return 0;
}

static int json_end_array_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output || json->depth == 0) {
    export_set_error(EXPORT_ERR_STATE,
                     "JSON exporter: end_array without begin_array");
    return -1;
  }

  if (json_pretty_indent(json, json->depth - 1) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON indent");
    return -1;
  }

  if (fputc(']', json->output) == EOF) {
    export_set_error(EXPORT_ERR_IO, "Failed to write JSON array end");
    return -1;
  }

  json->depth--;

  return 0;
}

static int json_flush_impl(exporter_t *self) {
  json_exporter_t *json = (json_exporter_t *)self;
  if (!json || !json->output) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "JSON exporter or output is NULL");
    return -1;
  }

  if (fflush(json->output) != 0) {
    export_set_error(EXPORT_ERR_IO, "Failed to flush JSON output");
    return -1;
  }

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
  if (!json) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "JSON exporter is NULL");
    return -1;
  }

  json->pretty = pretty;

  return 0;
}

int json_exporter_set_jsonl(json_exporter_t *json, bool jsonl) {
  if (!json) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "JSON exporter is NULL");
    return -1;
  }

  json->jsonl = jsonl;

  return 0;
}

int json_exporter_set_output(json_exporter_t *json, FILE *file) {
  if (!json || !file) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "JSON exporter or file is NULL");
    return -1;
  }

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

static inline void parse_column_spec(const char *spec, size_t spec_len,
                                     size_t *out_name_len,
                                     const char **out_type_start,
                                     size_t *out_type_len) {
  const char *colon = NULL;
  for (size_t i = 0; i < spec_len; i++) {
    if (spec[i] == ':') {
      colon = spec + i;
      break;
    }
  }

  if (colon) {
    *out_name_len = (size_t)(colon - spec);
    *out_type_start = colon + 1;
    *out_type_len = spec_len - (size_t)(colon - spec) - 1;
    // Handle empty type (e.g., "Name:")
    if (*out_type_len == 0)
      *out_type_start = NULL;
  } else {
    *out_name_len = spec_len;
    *out_type_start = NULL;
    *out_type_len = 0;
  }
}

static inline int sqlite_ensure_table_created(sqlite_exporter_t *sqlite) {
  if (!sqlite || sqlite->table_created)
    return 0;

  if (!sqlite->column_names || !sqlite->table_name) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLite's column names or table name is NULL");
    return -1;
  }

  // Build CREATE TABLE statement
  // column_names is semicolon-separated: "id;name;value"
  // We need: "id INTEGER, name TEXT, value REAL" (all as TEXT for simplicity)
  char *sql = NULL;
  size_t sql_len = 0;

  // Count columns and build column definitions
  const char *cols = sqlite->column_names;
  size_t cols_len = strlen(cols);

  //  Estimate buffer size: "CREATE TABLE IF NOT EXISTS " + table + " (" +
  //  cols*10 + ");"
  sql_len = strlen("CREATE TABLE IF NOT EXISTS ") + strlen(sqlite->table_name) +
            cols_len * 24;
  sql = (char *)malloc(sql_len);
  if (!sql) {
    export_set_error(
        EXPORT_ERR_MEMORY,
        "Failed to allocate memory for SQLite CREATE TABLE statement");
    return -1;
  }

  // Start CREATE TABLE statement
  int offset = snprintf(sql, sql_len, "CREATE TABLE IF NOT EXISTS %s (",
                        sqlite->table_name);
  if (offset < 0 || (size_t)offset >= sql_len) {
    free(sql);
    export_set_error(
        EXPORT_ERR_IO,
        "Failed to build CREATE TABLE statement for SQLite exporter");
    return -1;
  }

  // Parse column names and types
  const char *start = cols;
  const char *end = cols;
  bool first_col = true;

  while (*end) {
    if (*end == ';') {
      size_t spec_len = (size_t)(end - start);

      if (spec_len > 0) {
        if (!first_col) {
          int written = snprintf(sql + offset, sql_len - offset, ", ");
          if (written < 0 || (size_t)(offset + written) >= sql_len) {
            free(sql);
            export_set_error(
                EXPORT_ERR_IO,
                "Failed to build CREATE TABLE statement for SQLite exporter");
            return -1;
          }
          offset += written;
        }

        // Parse "Name:Type" or "Name"
        size_t name_len = 0;
        const char *type_start = NULL;
        size_t type_len = 0;
        parse_column_spec(start, spec_len, &name_len, &type_start, &type_len);

        // Write column definition with type (default TEXT)
        int written;
        if (type_start && type_len > 0) {
          written = snprintf(sql + offset, sql_len - offset, "%.*s %.*s",
                             (int)name_len, start, (int)type_len, type_start);
        } else {
          written = snprintf(sql + offset, sql_len - offset, "%.*s TEXT",
                             (int)name_len, start);
        }

        if (written < 0 || (size_t)(offset + written) >= sql_len) {
          free(sql);
          export_set_error(
              EXPORT_ERR_IO,
              "Failed to build CREATE TABLE statement for SQLite exporter");
          return -1;
        }
        offset += written;

        first_col = false;
      }

      start = end + 1;
    }
    end++;
  }

  // Handle last column
  {
    size_t spec_len = (size_t)(end - start);
    if (spec_len > 0) {
      if (!first_col) {
        int written = snprintf(sql + offset, sql_len - offset, ", ");
        if (written < 0 || (size_t)(offset + written) >= sql_len) {
          free(sql);
          export_set_error(
              EXPORT_ERR_IO,
              "Failed to build CREATE TABLE statement for SQLite exporter");
          return -1;
        }
        offset += written;
      }

      size_t name_len = 0;
      const char *type_start = NULL;
      size_t type_len = 0;
      parse_column_spec(start, spec_len, &name_len, &type_start, &type_len);

      int written;
      if (type_start && type_len > 0) {
        written = snprintf(sql + offset, sql_len - offset, "%.*s %.*s",
                           (int)name_len, start, (int)type_len, type_start);
      } else {
        written = snprintf(sql + offset, sql_len - offset, "%.*s TEXT",
                           (int)name_len, start);
      }

      if (written < 0 || (size_t)(offset + written) >= sql_len) {
        free(sql);
        export_set_error(
            EXPORT_ERR_IO,
            "Failed to build CREATE TABLE statement for SQLite exporter");
        return -1;
      }
      offset += written;
    }
  }

  // Close statement
  int written = snprintf(sql + offset, sql_len - offset, ")");
  if (written < 0 || (size_t)(offset + written) >= sql_len) {
    free(sql);
    export_set_error(
        EXPORT_ERR_IO,
        "Failed to build CREATE TABLE statement for SQLite exporter");
    return -1;
  }
  offset += written;

  // Execute CREATE TABLE
  char *err_msg = NULL;
  int rc = sqlite3_exec(sqlite->db, sql, NULL, NULL, &err_msg);
  free(sql);

  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite error: %s", err_msg);
    sqlite3_free(err_msg);
    return -1;
  }

  sqlite->table_created = true;

  return 0;
}

static inline int sqlite_prepare_insert_stmt(sqlite_exporter_t *sqlite) {
  if (!sqlite || !sqlite->column_names || !sqlite->table_name) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLITE exporter or column names or table name is NULL");
    return -1;
  }

  // If statement already exists, skip
  if (sqlite->stmt)
    return 0;

  // Build INSERT statement: INSERT INTO table (col1, col2, ...) VALUES (?, ?,
  // ...) Count columns first (ignore empty trailing segments after semicolons)
  int col_count = 0;
  const char *p = sqlite->column_names;
  if (*p) { // Non-empty string
    col_count = 1;
    while (*p) {
      if (*p == ';') {
        // Only count if there's content after the semicolon
        if (*(p + 1) != '\0')
          col_count++;
      }
      p++;
    }
    // Check for trailing semicolon - don't count empty segment
    if (*(p - 1) == ';')
      col_count--;
  }

  sqlite->column_count = col_count;

  // Estimate buffer size: need extra space for ", " between columns and "?", "
  // in VALUES
  size_t cols_len = strlen(sqlite->column_names);
  size_t sql_len = strlen("INSERT INTO ") + strlen(sqlite->table_name) +
                   cols_len * 2 + (size_t)col_count * 6 + 32;
  char *sql = (char *)malloc(sql_len);
  if (!sql) {
    export_set_error(
        EXPORT_ERR_MEMORY,
        "Failed to allocate memory for SQLite INSERT INTO statement");
    return -1;
  }

  //  Start: INSERT INTO table (
  int offset = snprintf(sql, sql_len, "INSERT INTO %s (", sqlite->table_name);
  if (offset < 0) {
    free(sql);
    export_set_error(EXPORT_ERR_SQLITE, "SQLite snprintf error(offset=%d)",
                     offset);
    return -1;
  }
  if ((size_t)offset >= sql_len) {
    free(sql);
    export_set_error(
        EXPORT_ERR_IO,
        "Failed to build INSERT INTO statement for SQLite exporter");
    return -1;
  }

  // Column names
  const char *start = sqlite->column_names;
  const char *end = sqlite->column_names;
  bool first = true;

  while (*end) {
    if (*end == ';') {
      size_t spec_len = (size_t)(end - start);

      if (spec_len > 0) {
        if (!first) {
          int written = snprintf(sql + offset, sql_len - offset, ", ");
          if (written < 0 || (size_t)(offset + written) >= sql_len) {
            free(sql);
            export_set_error(
                EXPORT_ERR_IO,
                "Failed to build INSERT INTO statement for SQLite exporter");
            return -1;
          }
          offset += written;
        }

        // Extract only the column name (ignore type for INSERT statement)
        size_t name_len = 0;
        const char *type_start = NULL;
        size_t type_len = 0;
        parse_column_spec(start, spec_len, &name_len, &type_start, &type_len);

        int written = snprintf(sql + offset, sql_len - offset, "%.*s",
                               (int)name_len, start);
        if (written < 0 || (size_t)(offset + written) >= sql_len) {
          free(sql);
          export_set_error(
              EXPORT_ERR_IO,
              "Failed to build INSERT INTO statement for SQLite exporter");
          return -1;
        }
        offset += written;

        first = false;
      }

      start = end + 1;
    }
    end++;
  }

  {
    size_t spec_len = (size_t)(end - start);
    if (spec_len > 0) {
      if (!first) {
        int written = snprintf(sql + offset, sql_len - offset, ", ");
        if (written < 0 || (size_t)(offset + written) >= sql_len) {
          free(sql);
          export_set_error(
              EXPORT_ERR_IO,
              "Failed to build INSERT INTO statement for SQLite exporter");
          return -1;
        }
        offset += written;
      }

      size_t name_len = 0;
      const char *type_start = NULL;
      size_t type_len = 0;
      parse_column_spec(start, spec_len, &name_len, &type_start, &type_len);

      int written = snprintf(sql + offset, sql_len - offset, "%.*s",
                             (int)name_len, start);
      if (written < 0 || (size_t)(offset + written) >= sql_len) {
        free(sql);
        export_set_error(
            EXPORT_ERR_IO,
            "Failed to build INSERT INTO statement for SQLite exporter");
        return -1;
      }
      offset += written;
    }
  }

  // VALUES part
  int written = snprintf(sql + offset, sql_len - offset, ") VALUES (");
  if (written < 0 || (size_t)(offset + written) >= sql_len) {
    free(sql);
    export_set_error(
        EXPORT_ERR_IO,
        "Failed to build INSERT INTO statement for SQLite exporter");
    return -1;
  }
  offset += written;

  for (int i = 0; i < col_count; ++i) {
    if (i > 0) {
      written = snprintf(sql + offset, sql_len - offset, ", ");
      if (written < 0 || (size_t)(offset + written) >= sql_len) {
        free(sql);
        export_set_error(
            EXPORT_ERR_IO,
            "Failed to build INSERT INTO statement for SQLite exporter");
        return -1;
      }
      offset += written;
    }
    written = snprintf(sql + offset, sql_len - offset, "?");
    if (written < 0 || (size_t)(offset + written) >= sql_len) {
      free(sql);
      export_set_error(
          EXPORT_ERR_IO,
          "Failed to build INSERT INTO statement for SQLite exporter");
      return -1;
    }
    offset += written;
  }

  written = snprintf(sql + offset, sql_len - offset, ")");
  if (written < 0 || (size_t)(offset + written) >= sql_len) {
    free(sql);
    export_set_error(
        EXPORT_ERR_IO,
        "Failed to build INSERT INTO statement for SQLite exporter");
    return -1;
  }
  offset += written;

  // Prepare statement
  int rc = sqlite3_prepare_v2(sqlite->db, sql, -1, &sqlite->stmt, NULL);
  free(sql);

  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite prepare error: %s",
                     sqlite3_errmsg(sqlite->db));
    sqlite->stmt = NULL;
    return -1;
  }

  return 0;
}

static inline int sqlite_reset_stmt(sqlite_exporter_t *sqlite) {
  if (!sqlite || !sqlite->stmt) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLITE exporter or SQLITE stmt is NULL");
    return -1;
  }

  int rc = sqlite3_reset(sqlite->stmt);
  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite reset error: %s",
                     sqlite3_errmsg(sqlite->db));
    return -1;
  }

  rc = sqlite3_clear_bindings(sqlite->stmt);
  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite clear bindings error: %s",
                     sqlite3_errmsg(sqlite->db));
    return -1;
  }

  return 0;
}

static int sqlite_begin_object_impl(exporter_t *self, const char *name) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLITE exporter or SQLITE db is NULL");
    return -1;
  }

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
      export_set_error(EXPORT_ERR_SQLITE, "SQLite BEGIN error: %s", err_msg);
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
  if (!sqlite || !sqlite->db || !sqlite->stmt) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLITE exporter or SQLITE db or SQLITE stmt is NULL");
    return -1;
  }

  // Execute the prepared statement
  int rc = sqlite3_step(sqlite->stmt);
  if (rc != SQLITE_DONE) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite step error: %s (rc=%d)",
                     sqlite3_errmsg(sqlite->db), rc);
    return -1;
  }

  // Commit immediately if auto_commit mode is enabled
  if (sqlite->auto_commit && sqlite->in_transaction) {
    char *err_msg = NULL;
    rc = sqlite3_exec(sqlite->db, "COMMIT", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
      export_set_error(EXPORT_ERR_SQLITE, "SQLite COMMIT error: %s", err_msg);
      sqlite3_free(err_msg);
      return -1;
    }
    sqlite->in_transaction = false;
  }

  sqlite->in_object = false;

  return 0;
}

static int sqlite_write_int_impl(exporter_t *self, const char *key,
                                 int64_t value) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db || !sqlite->stmt || !sqlite->in_object) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLite exporter or database/statement is NULL");
    return -1;
  }

  if (sqlite->current_column >= sqlite->column_count) {
    export_set_error(
        EXPORT_ERR_SQLITE,
        "SQLite exporter: current column index exceeds column count");
    return -1;
  }

  int rc = sqlite3_bind_int64(sqlite->stmt, sqlite->current_column + 1, value);
  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite bind_int64 error: %s",
                     sqlite3_errmsg(sqlite->db));
    return -1;
  }

  sqlite->current_column++;
  sqlite->is_first_column = false;

  (void)key;

  return 0;
}

static int sqlite_write_double_impl(exporter_t *self, const char *key,
                                    double value) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db || !sqlite->stmt || !sqlite->in_object) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLite exporter or database/statement is NULL");
    return -1;
  }

  if (sqlite->current_column >= sqlite->column_count) {
    export_set_error(
        EXPORT_ERR_SQLITE,
        "SQLite exporter: current column index exceeds column count");
    return -1;
  }

  int rc = sqlite3_bind_double(sqlite->stmt, sqlite->current_column + 1, value);
  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite bind_double error: %s",
                     sqlite3_errmsg(sqlite->db));
    return -1;
  }

  sqlite->current_column++;
  sqlite->is_first_column = false;

  (void)key;

  return 0;
}

static int sqlite_write_string_impl(exporter_t *self, const char *key,
                                    const char *value) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db || !sqlite->stmt || !sqlite->in_object) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLite exporter or database/statement is NULL");
    return -1;
  }

  if (sqlite->current_column >= sqlite->column_count) {
    export_set_error(
        EXPORT_ERR_SQLITE,
        "SQLite exporter: current column index exceeds column count");
    return -1;
  }

  int rc = sqlite3_bind_text(sqlite->stmt, sqlite->current_column + 1,
                             value ? value : "", -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite bind_text error: %s",
                     sqlite3_errmsg(sqlite->db));
    return -1;
  }

  sqlite->current_column++;
  sqlite->is_first_column = false;

  (void)key;

  return 0;
}

static int sqlite_write_bool_impl(exporter_t *self, const char *key,
                                  bool value) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db || !sqlite->stmt || !sqlite->in_object) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLite exporter or database/statement is NULL");
    return -1;
  }

  if (sqlite->current_column >= sqlite->column_count) {
    export_set_error(
        EXPORT_ERR_SQLITE,
        "SQLite exporter: current column index exceeds column count");
    return -1;
  }

  // SQLite stores booleans as integers (0 or 1)
  int rc =
      sqlite3_bind_int(sqlite->stmt, sqlite->current_column + 1, value ? 1 : 0);
  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite bind_int error: %s",
                     sqlite3_errmsg(sqlite->db));
    return -1;
  }

  sqlite->current_column++;
  sqlite->is_first_column = false;

  (void)key;

  return 0;
}

static int sqlite_write_null_impl(exporter_t *self, const char *key) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db || !sqlite->stmt || !sqlite->in_object) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLite exporter or database/statement is NULL");
    return -1;
  }

  if (sqlite->current_column >= sqlite->column_count) {
    export_set_error(
        EXPORT_ERR_SQLITE,
        "SQLite exporter: current column index exceeds column count");
    return -1;
  }

  int rc = sqlite3_bind_null(sqlite->stmt, sqlite->current_column + 1);
  if (rc != SQLITE_OK) {
    export_set_error(EXPORT_ERR_SQLITE, "SQLite bind_null error: %s",
                     sqlite3_errmsg(sqlite->db));
    return -1;
  }

  sqlite->current_column++;
  sqlite->is_first_column = false;

  (void)key;

  return 0;
}

static int sqlite_begin_array_impl(exporter_t *self, const char *key) {
  (void)self;
  (void)key;
  export_set_error(EXPORT_ERR_SQLITE,
                   "Array serialization not supported in SQLite");
  return -1;
}

static int sqlite_end_array_impl(exporter_t *self) {
  (void)self;
  export_set_error(EXPORT_ERR_SQLITE,
                   "Array serialization not supported in SQLite");
  return -1;
}

static int sqlite_flush_impl(exporter_t *self) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite || !sqlite->db) {
    export_set_error(EXPORT_ERR_NULL_PARAM,
                     "SQLITE exporter or SQLITE db is NULL");
    return -1;
  }

  // Commit any pending transaction
  if (sqlite->in_transaction) {
    char *err_msg = NULL;
    int rc = sqlite3_exec(sqlite->db, "COMMIT", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
      export_set_error(EXPORT_ERR_SQLITE, "SQLite COMMIT error: %s", err_msg);
      sqlite3_free(err_msg);
      return -1;
    }
    sqlite->in_transaction = false;
  }

  return 0;
}

static void sqlite_destroy_impl(exporter_t *self) {
  sqlite_exporter_t *sqlite = (sqlite_exporter_t *)self;
  if (!sqlite)
    return;

  // Commit any pending transaction before destroying
  if (sqlite->in_transaction) {
    sqlite_flush_impl(&sqlite->base);
  }

  if (sqlite->stmt) {
    sqlite3_finalize(sqlite->stmt);
    sqlite->stmt = NULL;
  }
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

  sqlite.base.begin_object = sqlite_begin_object_impl;
  sqlite.base.end_object = sqlite_end_object_impl;
  sqlite.base.begin_array = sqlite_begin_array_impl;
  sqlite.base.end_array = sqlite_end_array_impl;
  sqlite.base.write_int = sqlite_write_int_impl;
  sqlite.base.write_double = sqlite_write_double_impl;
  sqlite.base.write_string = sqlite_write_string_impl;
  sqlite.base.write_bool = sqlite_write_bool_impl;
  sqlite.base.write_null = sqlite_write_null_impl;
  sqlite.base.flush = sqlite_flush_impl;
  sqlite.base.destroy = sqlite_destroy_impl;

  // Parse column_names to count columns (ignore empty trailing segments)
  if (column_names) {
    int col_count = 0;
    const char *p = column_names;
    if (*p) { // Non-empty string
      col_count = 1;
      while (*p) {
        if (*p == ';') {
          // Only count if there's content after the semicolon
          if (*(p + 1) != '\0')
            col_count++;
        }
        p++;
      }
      // Check for trailing semicolon - don't count empty segment
      if (*(p - 1) == ';')
        col_count--;
    }
    sqlite.column_count = col_count;
  }

  return sqlite;
}

int sqlite_exporter_set_auto_commit(sqlite_exporter_t *sqlite,
                                    bool auto_commit) {
  if (!sqlite) {
    export_set_error(EXPORT_ERR_NULL_PARAM, "SQLITE exporter is NULL");
    return -1;
  }

  sqlite->auto_commit = auto_commit;

  return 0;
}

#endif // SQLITE_EXPORT

/*----------------------SQLITE EXPORTER----------------------*/

#endif // EXPORT_IMPLEMENTATION

#endif // EXPORT_H
