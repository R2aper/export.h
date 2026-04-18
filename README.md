# export.h

A lightweight, single-header C library for exporting structured data to **CSV**, **JSON/JSONL**, and **SQLite**.

## Features

- **Unified API**: Single `exporter_t` interface works seamlessly across CSV, JSON, JSONL, and SQLite
- **Thread-Safe Errors**: Uses compiler-provided thread-local storage for `errno`-style error codes and messages
- **CSV Support**: RFC 4180 compliant string quoting, configurable single/multi-header mode, automatic array wrapping
- **JSON/JSONL Support**: Pretty-printing (2-space indent), JSON Lines mode, automatic nesting stack management with dynamic growth
- **SQLite Support**: Automatic `CREATE TABLE IF NOT EXISTS`, prepared `INSERT` statements, transaction batching, auto-commit toggle
- **Zero Dependencies**: Pure C (SQLite3 requires external linking only when `SQLITE_EXPORT` is defined)

## Installation

Simply include `export.h` in your project. For SQLite support, define `SQLITE_EXPORT` before including:

```c
#define EXPORT_IMPLEMENTATION
#define SQLITE_EXPORT  // Optional: for SQLite support
#include "export.h"
```

**Compilation:**
```bash
# CSV and JSON only
gcc -o program program.c -std=c11

# With SQLite support
gcc -o program program.c -lsqlite3 -std=c11
```

## Quick Start

### CSV Export

```c
#define EXPORT_IMPLEMENTATION
#include "export.h"

int main() {
    csv_exporter_t csv = create_csv_exporter(stdout, "id;name;score", true);

    csv.base.begin_object(&csv.base, NULL);
    csv.base.write_int(&csv.base, "id", 1);
    csv.base.write_string(&csv.base, "name", "Alice");
    csv.base.write_double(&csv.base, "score", 95.5);
    csv.base.end_object(&csv.base);

    csv.base.destroy(&csv.base);
    return 0;
}
```

### JSON Export

```c
json_exporter_t json = create_json_exporter(stdout, true, false);

json.base.begin_object(&json.base, "root");
json.base.write_string(&json.base, "greeting", "Hello");
json.base.begin_array(&json.base, "nums");
json.base.write_int(&json.base, NULL, 1);
json.base.write_int(&json.base, NULL, 2);
json.base.end_array(&json.base);
json.base.end_object(&json.base);

json.base.destroy(&json.base);
```

### JSONL Export

```c
json_exporter_t jsonl = create_json_exporter(stdout, false, true);

jsonl.base.begin_object(&jsonl.base, NULL);
jsonl.base.write_string(&jsonl.base, "name", "Alice");
jsonl.base.write_int(&jsonl.base, "age", 30);
jsonl.base.end_object(&jsonl.base);

jsonl.base.destroy(&jsonl.base);
```

### SQLite Export

```c
#define EXPORT_IMPLEMENTATION
#define SQLITE_EXPORT
#include "export.h"
#include <sqlite3.h>

int main() {
    sqlite3 *db;
    sqlite3_open("data.db", &db);

    sqlite_exporter_t sql = create_sqlite_exporter(db, "users", "id:INTEGER;name:TEXT;active:INTEGER");
    sqlite_exporter_set_auto_commit(&sql, true);

    sql.base.begin_object(&sql.base, NULL);
    sql.base.write_int(&sql.base, NULL, 42);
    sql.base.write_string(&sql.base, NULL, "Bob");
    sql.base.write_bool(&sql.base, NULL, true);
    sql.base.end_object(&sql.base);

    sql.base.destroy(&sql.base);
    sqlite3_close(db);
    return 0;
}
```

## API Reference

### Core Types

```c
typedef struct exporter_t {
    int (*begin_object)(exporter_t *self, const char *name);
    int (*end_object)(exporter_t *self);
    int (*write_int)(exporter_t *self, const char *key, int64_t value);
    int (*write_double)(exporter_t *self, const char *key, double value);
    int (*write_string)(exporter_t *self, const char *key, const char *value);
    int (*write_bool)(exporter_t *self, const char *key, bool value);
    int (*write_null)(exporter_t *self, const char *key);
    int (*begin_array)(exporter_t *self, const char *key);
    int (*end_array)(exporter_t *self);
    int (*flush)(exporter_t *self);
    void (*destroy)(exporter_t *self);
} exporter_t;
```

### CSV Functions

```c
csv_exporter_t create_csv_exporter(FILE *file, const char *csv_header, bool write_header_once);
int csv_exporter_set_write_header_once(csv_exporter_t *csv, bool write_header_once);
int csv_exporter_set_csv_header(csv_exporter_t *csv, const char *csv_header);
int csv_exporter_set_output(csv_exporter_t *csv, FILE *file);
```

### JSON Functions

```c
json_exporter_t create_json_exporter(FILE *file, bool pretty, bool jsonl);
int json_exporter_set_pretty(json_exporter_t *json, bool pretty);
int json_exporter_set_jsonl(json_exporter_t *json, bool jsonl);
int json_exporter_set_output(json_exporter_t *json, FILE *file);
```

### SQLite Functions (when `SQLITE_EXPORT` is defined)

```c
sqlite_exporter_t create_sqlite_exporter(sqlite3 *db, const char *table_name, const char *column_names);
int sqlite_exporter_set_auto_commit(sqlite_exporter_t *sqlite, bool auto_commit);
```

### Error Handling

```c
const char *export_strerror(export_error_code_t code);
const char *export_get_last_error(void);
void export_clear_error(void);

extern thread_local export_error_code_t export_last_error;
extern thread_local char export_last_error_msg[EXPORT_ERR_MSG_SIZE];
```

**Error codes:**
- `EXPORT_OK` - No error
- `EXPORT_ERR_INVALID_PARAM` - Invalid parameter
- `EXPORT_ERR_IO` - File I/O error
- `EXPORT_ERR_MEMORY` - Memory allocation failed
- `EXPORT_ERR_SQLITE` - SQLite error
- `EXPORT_ERR_STATE` - Invalid exporter state
- `EXPORT_ERR_SERIALIZE` - Serialization error

## Configuration Notes

### Setup

Define `EXPORT_IMPLEMENTATION` in exactly **one** `.c` file before including the header:

```c
#define EXPORT_IMPLEMENTATION
#include "export.h"
```

### SQLite Support

To enable SQLite export, define `SQLITE_EXPORT` and link against `sqlite3`:

```c
#define EXPORT_IMPLEMENTATION
#define SQLITE_EXPORT
#include "export.h"
```

```bash
gcc -o program program.c -lsqlite3
```

### Column Mapping (SQLite)

Table structure is described via a column specification string in format:
```
"Name1:Type1;Name2:Type2;Name3"
```

Types are optional and default to `TEXT` if omitted. Example:
```c
"id:INTEGER;name:TEXT;salary:REAL;active:INTEGER"
```

### CSV Delimiters

- Field separator: `;` (semicolon) for object members
- Array element separator: `,` (comma)
- All string values are automatically quoted per RFC 4180

## Error Handling Pattern

```c
int rc = exporter.base.write_string(&exporter.base, "key", value);
if (rc == -1) {
    fprintf(stderr, "Export failed [%d]: %s\n", 
            export_last_error, export_get_last_error());
    export_clear_error();
}
```

## Memory Management

Always call `.destroy()` on the base exporter to free internal allocations:

```c
exporter.base.destroy(&exporter.base);
```

## C Standard Requirements

- **C99** minimum (for `<stdbool.h>`)
- **C11** recommended (for native `<threads.h>` thread-local storage)
- Falls back to compiler extensions (`__thread`, `__declspec(thread)`) on older standards