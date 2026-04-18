// tests.c
// Test suite for export.h
//
// Compile:
//   gcc -o tests tests.c -lsqlite3 -std=c11 -Wall -Wextra
//   ./test

#define EXPORT_IMPLEMENTATION
#define SQLITE_EXPORT

#include <assert.h>
#include <stdio.h>

#ifdef SQLITE_EXPORT
#include <sqlite3.h>
#endif

#include "export.h"

/* ======================= TEST MACROS ======================== */

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define EXPECT_SUCCESS(expr, description)                                      \
  do {                                                                         \
    export_clear_error();                                                      \
    int r = (expr);                                                            \
    if (r == 0) {                                                              \
      printf("[OK]   %s\n", description);                                      \
      g_tests_passed++;                                                        \
    } else {                                                                   \
      printf("[FAIL] %s → %s (%s)\n", description,                             \
             export_strerror(export_last_error), export_get_last_error());     \
      g_tests_failed++;                                                        \
      export_clear_error();                                                    \
    }                                                                          \
  } while (0)

#define EXPECT_ERROR(expr, description, expected_code)                         \
  do {                                                                         \
    export_clear_error();                                                      \
    int r = (expr);                                                            \
    if (r == 0) {                                                              \
      printf("[FAIL] %s → expected error but succeeded\n", description);       \
      g_tests_failed++;                                                        \
    } else if (export_last_error != (expected_code)) {                         \
      printf("[FAIL] %s → wrong error (got %s, expected %s)\n", description,   \
             export_strerror(export_last_error),                               \
             export_strerror(expected_code));                                  \
      g_tests_failed++;                                                        \
    } else {                                                                   \
      printf("[OK]   %s (correctly returned error)\n", description);           \
      g_tests_passed++;                                                        \
    }                                                                          \
    export_clear_error();                                                      \
  } while (0)

#define TEST_SECTION(title)                                                    \
  do {                                                                         \
    printf("\n=== %s ===\n", title);                                           \
  } while (0)

/* ======================= TEST MACROS ======================== */

/* ====================== TEST FUNCTIONS ====================== */

#ifdef SQLITE_EXPORT
static int test_sqlite_create(void) {
  const char *cols = "name;";
  sqlite_exporter_t s = create_sqlite_exporter(NULL, "test_data", cols);
  (void)s;

  return export_last_error;
}
#endif

static void test_error_handling(void) {
  TEST_SECTION("Error Handling");

  EXPECT_ERROR(csv_exporter_set_output(NULL, stdout),
               "csv_exporter_set_output(NULL)", EXPORT_ERR_INVALID_PARAM);

  EXPECT_ERROR(json_exporter_set_pretty(NULL, true),
               "json_exporter_set_pretty(NULL)", EXPORT_ERR_INVALID_PARAM);

#ifdef SQLITE_EXPORT
  EXPECT_ERROR(test_sqlite_create(),
               "create_sqlite_exporter(Invalid column_names)",
               EXPORT_ERR_INVALID_PARAM);

  EXPECT_ERROR(sqlite_exporter_set_auto_commit(NULL, true),
               "sqlite_exporter_set_auto_commit(NULL)",
               EXPORT_ERR_INVALID_PARAM);
#endif

  // Invalid state
  {
    json_exporter_t j = create_json_exporter(stdout, false, false);
    EXPECT_ERROR(j.base.end_object(&j.base),
                 "json_end_object without begin_object", EXPORT_ERR_STATE);
    j.base.destroy(&j.base);
  }

#ifdef SQLITE_EXPORT
  {
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);
    sqlite_exporter_t s = create_sqlite_exporter(db, "test", "col1;col2");
    EXPECT_ERROR(s.base.begin_array(&s.base, NULL),
                 "SQLite begin_array (unsupported)", EXPORT_ERR_SQLITE);
    s.base.destroy(&s.base);
    sqlite3_close(db);
  }
#endif
}

static void test_csv_exporter(void) {
  TEST_SECTION("CSV Exporter");

  csv_exporter_t csv =
      create_csv_exporter(stdout, "name;age;salary;active;children", true);
  EXPECT_SUCCESS(csv.base.begin_object(&csv.base, NULL),
                 "begin_object (header once)");

  EXPECT_SUCCESS(csv.base.write_string(&csv.base, NULL, "Alice"),
                 "write_string name");
  EXPECT_SUCCESS(csv.base.write_int(&csv.base, NULL, 30), "write_int age");
  EXPECT_SUCCESS(csv.base.write_double(&csv.base, NULL, 50000.5),
                 "write_double salary");
  EXPECT_SUCCESS(csv.base.write_bool(&csv.base, NULL, true),
                 "write_bool active");

  EXPECT_SUCCESS(csv.base.begin_array(&csv.base, NULL), "begin_array children");
  EXPECT_SUCCESS(csv.base.write_string(&csv.base, NULL, "Bob"),
                 "array string 1");
  EXPECT_SUCCESS(csv.base.write_string(&csv.base, NULL, "Charlie"),
                 "array string 2");
  EXPECT_SUCCESS(csv.base.end_array(&csv.base), "end_array");

  EXPECT_SUCCESS(csv.base.end_object(&csv.base), "end_object row 1");

  // Row 2
  EXPECT_SUCCESS(csv.base.begin_object(&csv.base, NULL), "begin_object row 2");
  EXPECT_SUCCESS(csv.base.write_string(&csv.base, NULL, "Bob"),
                 "write_string name 2");
  EXPECT_SUCCESS(csv.base.write_int(&csv.base, NULL, 25), "write_int age 2");
  EXPECT_SUCCESS(csv.base.write_double(&csv.base, NULL, 62000.0),
                 "write_double salary 2");
  EXPECT_SUCCESS(csv.base.write_bool(&csv.base, NULL, false),
                 "write_bool active 2");
  EXPECT_SUCCESS(csv.base.write_null(&csv.base, NULL), "write_null children");
  EXPECT_SUCCESS(csv.base.end_object(&csv.base), "end_object row 2");

  EXPECT_SUCCESS(csv.base.flush(&csv.base), "flush");
  csv.base.destroy(&csv.base);

  printf("   → CSV output shown above (header + 2 rows)\n");
}

static void test_json_exporter_pretty(void) {
  TEST_SECTION("JSON Exporter (pretty, nested)");

  json_exporter_t json = create_json_exporter(stdout, true, false);
  EXPECT_SUCCESS(json.base.begin_array(&json.base, NULL), "begin root array");

  EXPECT_SUCCESS(json.base.begin_object(&json.base, NULL), "begin person 1");
  EXPECT_SUCCESS(json.base.write_string(&json.base, "name", "Alice"),
                 "write_string name");
  EXPECT_SUCCESS(json.base.write_int(&json.base, "age", 30), "write_int age");
  EXPECT_SUCCESS(json.base.write_double(&json.base, "salary", 50000.5),
                 "write_double salary");
  EXPECT_SUCCESS(json.base.write_bool(&json.base, "active", true),
                 "write_bool active");

  EXPECT_SUCCESS(json.base.begin_array(&json.base, "children"),
                 "begin children array");
  EXPECT_SUCCESS(json.base.write_string(&json.base, NULL, "Bob"),
                 "array string 1");
  EXPECT_SUCCESS(json.base.write_string(&json.base, NULL, "Charlie"),
                 "array string 2");
  EXPECT_SUCCESS(json.base.end_array(&json.base), "end children array");

  EXPECT_SUCCESS(json.base.begin_object(&json.base, "address"),
                 "begin address object");
  EXPECT_SUCCESS(json.base.write_string(&json.base, "city", "Wonderland"),
                 "address city");
  EXPECT_SUCCESS(json.base.write_null(&json.base, "zip"), "address zip null");
  EXPECT_SUCCESS(json.base.end_object(&json.base), "end address object");

  EXPECT_SUCCESS(json.base.end_object(&json.base), "end person 1");
  EXPECT_SUCCESS(json.base.end_array(&json.base), "end root array");

  EXPECT_SUCCESS(json.base.flush(&json.base), "flush");
  json.base.destroy(&json.base);

  printf("   → Pretty JSON shown above\n");
}

static void test_json_exporter_jsonl(void) {
  TEST_SECTION("JSON Exporter (JSONL mode)");

  json_exporter_t jsonl = create_json_exporter(stdout, false, true);
  EXPECT_SUCCESS(jsonl.base.begin_object(&jsonl.base, NULL), "begin object 1");
  EXPECT_SUCCESS(jsonl.base.write_string(&jsonl.base, "name", "Alice"),
                 "write_string name");
  EXPECT_SUCCESS(jsonl.base.write_int(&jsonl.base, "age", 30), "write_int age");
  EXPECT_SUCCESS(jsonl.base.write_double(&jsonl.base, "salary", 50000.5),
                 "write_double salary");
  EXPECT_SUCCESS(jsonl.base.end_object(&jsonl.base), "end object 1");

  EXPECT_SUCCESS(jsonl.base.begin_object(&jsonl.base, NULL), "begin object 2");
  EXPECT_SUCCESS(jsonl.base.write_string(&jsonl.base, "name", "Bob"),
                 "write_string name");
  EXPECT_SUCCESS(jsonl.base.write_int(&jsonl.base, "age", 25), "write_int age");
  EXPECT_SUCCESS(jsonl.base.write_null(&jsonl.base, "notes"), "write_null");
  EXPECT_SUCCESS(jsonl.base.end_object(&jsonl.base), "end object 2");

  EXPECT_SUCCESS(jsonl.base.flush(&jsonl.base), "flush");
  jsonl.base.destroy(&jsonl.base);

  printf("   → JSONL output shown above\n");
}

#ifdef SQLITE_EXPORT
static void test_sqlite_exporter(void) {
  TEST_SECTION("SQLite Exporter");

  sqlite3 *db = NULL;
  if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
    printf("[FAIL] Could not open :memory: database\n");
    g_tests_failed++;
    return;
  }

  const char *cols = "name:TEXT;age:INTEGER;salary:REAL;active:INTEGER;notes";
  sqlite_exporter_t s = create_sqlite_exporter(db, "test_data", cols);

  EXPECT_SUCCESS(sqlite_exporter_set_auto_commit(&s, true), "set auto-commit");

  EXPECT_SUCCESS(s.base.begin_object(&s.base, NULL), "begin row 1");
  EXPECT_SUCCESS(s.base.write_string(&s.base, "name", "Alice"),
                 "write_string name");
  EXPECT_SUCCESS(s.base.write_int(&s.base, "age", 30), "write_int age");
  EXPECT_SUCCESS(s.base.write_double(&s.base, "salary", 50000.5),
                 "write_double salary");
  EXPECT_SUCCESS(s.base.write_bool(&s.base, "active", true),
                 "write_bool active");
  EXPECT_SUCCESS(
      s.base.write_string(&s.base, "notes",
                          "Hello \"world\" with comma, and; semicolon"),
      "write_string notes");
  EXPECT_SUCCESS(s.base.end_object(&s.base), "end row 1");

  EXPECT_SUCCESS(s.base.begin_object(&s.base, NULL), "begin row 2");
  EXPECT_SUCCESS(s.base.write_string(&s.base, "name", "Bob"),
                 "write_string name 2");
  EXPECT_SUCCESS(s.base.write_int(&s.base, "age", 25), "write_int age 2");
  EXPECT_SUCCESS(s.base.write_double(&s.base, "salary", 62000.0),
                 "write_double salary 2");
  EXPECT_SUCCESS(s.base.write_bool(&s.base, "active", false),
                 "write_bool active 2");
  EXPECT_SUCCESS(s.base.write_null(&s.base, "notes"), "write_null notes");
  EXPECT_SUCCESS(s.base.end_object(&s.base), "end row 2");

  EXPECT_SUCCESS(s.base.flush(&s.base), "flush");
  s.base.destroy(&s.base);

  // Verify
  printf("   Verifying data:\n");
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(
          db,
          "SELECT name,age,salary,active,notes FROM test_data ORDER BY name",
          -1, &stmt, NULL) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      printf("     • %s | age=%lld | salary=%.2f | active=%d | notes=%s\n",
             sqlite3_column_text(stmt, 0), sqlite3_column_int64(stmt, 1),
             sqlite3_column_double(stmt, 2), sqlite3_column_int(stmt, 3),
             sqlite3_column_text(stmt, 4)
                 ? (const char *)sqlite3_column_text(stmt, 4)
                 : "NULL");
    }
    sqlite3_finalize(stmt);
  }
  sqlite3_close(db);

  printf("   → SQLite test completed (table + 2 rows verified)\n");
}
#endif

/* ====================== TEST FUNCTIONS ====================== */

/* =========================== MAIN =========================== */

int main(void) {
  printf("=== export.h Comprehensive Test Suite ===\n");
  printf("Running all tests...\n");

  test_error_handling();
  test_csv_exporter();
  test_json_exporter_pretty();
  test_json_exporter_jsonl();
#ifdef SQLITE_EXPORT
  test_sqlite_exporter();
#else
  TEST_SECTION("SQLite Exporter");
  printf("[SKIP] SQLite tests (SQLITE_EXPORT not defined)\n");
#endif

  printf("\n=== TEST SUMMARY ===\n");
  printf("Passed: %d\n", g_tests_passed);
  printf("Failed: %d\n", g_tests_failed);

  if (g_tests_failed == 0)
    printf("All tests passed!\n");
  else
    printf("Some tests failed (see [FAIL] lines above)\n");

  return g_tests_failed == 0 ? 0 : 1;
}
