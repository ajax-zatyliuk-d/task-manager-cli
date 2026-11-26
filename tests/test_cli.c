#include "cli.h"
#include "task.h"
#include "task_manager.h"
#include "task_manager_test.h"

#include <glib.h>

#include <stddef.h>

typedef struct
{
    const gchar *command;
    CliStatus expected_status;
    gboolean expect_unknown_command;
    guint expected_history_length;
} BasicCase;

static void test_cli_basic(gconstpointer data)
{
    const BasicCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    const CliStatus status = cli_execute_line(manager, test_case->command, output, &error);

    g_assert_cmpint(status, ==, test_case->expected_status);
    if (test_case->expect_unknown_command) {
        g_assert_error(error, CLI_ERROR, CLI_ERROR_UNKNOWN_COMMAND);
    } else {
        g_assert_no_error(error);
    }
    g_assert_cmpstr(output->str, ==, "");
    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, test_case->expected_history_length);
    if (history->len != 0) {
        g_assert_cmpstr(g_ptr_array_index(history, 0), ==, test_case->command);
    }
}

static const BasicCase basic_cases[] = {
    {"", CLI_CONTINUE, FALSE, 0},
    {"    ", CLI_CONTINUE, FALSE, 0},
    {"quit", CLI_QUIT, FALSE, 0},
    {"unknown", CLI_CONTINUE, TRUE, 1},
};

typedef struct
{
    const gchar *command;
    const gchar *title;
    const gchar *first_tag;
    const gchar *second_tag;
    guint expected_tag_count;
} AddCase;

typedef struct
{
    const gchar *command;
    const gchar *message;
} NumericCase;

typedef struct
{
    const gchar *command;
    GQuark (*domain)(void);
    gint code;
} InvalidCase;

typedef struct
{
    const gchar *command;
} TopInvalidCase;

enum
{
    PARSE_NUMERIC_ADD_SUFFIX,
    PARSE_NUMERIC_ADD_OVERFLOW,
    PARSE_NUMERIC_DEL_SUFFIX,
    PARSE_NUMERIC_DEL_OVERFLOW,
    PARSE_NUMERIC_TOP_SUFFIX,
    PARSE_NUMERIC_TOP_OVERFLOW,
};

static const NumericCase numeric_cases[] = {
    [PARSE_NUMERIC_ADD_SUFFIX] = {"add 5x title", "usage: add <priority> <title> [#tag ...]"},
    [PARSE_NUMERIC_ADD_OVERFLOW] = {"add 999999999999999999999 title", "usage: add <priority> <title> [#tag ...]"},
    [PARSE_NUMERIC_DEL_SUFFIX] = {"del 1x", "invalid id 1x"},
    [PARSE_NUMERIC_DEL_OVERFLOW] = {"del 999999999999999999999", "invalid id 999999999999999999999"},
    [PARSE_NUMERIC_TOP_SUFFIX] = {"top 1x", "expected an unsigned int <n>"},
    [PARSE_NUMERIC_TOP_OVERFLOW] = {"top 999999999999999999999", "expected an unsigned int <n>"},
};

static void test_cli_numeric_tokens(gconstpointer data)
{
    const NumericCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;
    g_test_message("%s", test_case->command);
    g_assert_cmpint(cli_execute_line(manager, test_case->command, output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_assert_cmpstr(error->message, ==, test_case->message);
    g_assert_cmpstr(output->str, ==, "");
    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 0);
}

static void test_cli_invalid_utf8(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;
    const gchar command[] = "add 5 \xFF";

    CliStatus status = cli_execute_line(manager, command, output, &error);

    g_assert_cmpint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_assert_cmpstr(output->str, ==, "");
    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 0);

    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, 0);
}

static const AddCase add_cases[] = {
    {"add 5 buy milk", "buy milk", NULL, NULL, 0},
    {"add 5 title #home #shop", "title", "home", "shop", 2},
    {"add  5   buy   milk  #a", "buy milk", "a", NULL, 1},
};

static void test_cli_add(gconstpointer data)
{
    const AddCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    g_assert_cmpint(cli_execute_line(manager, test_case->command, output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "added task 1\n");

    const Task *task = task_manager_find_task_by_id(manager, 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task_get_title(task), ==, test_case->title);
    g_assert_cmpint(task_get_priority(task), ==, 5);
    g_assert_cmpuint(task_get_tag_count(task), ==, test_case->expected_tag_count);
    if (test_case->first_tag != NULL) {
        g_assert_cmpstr(task_get_tag_at(task, 0), ==, test_case->first_tag);
    }
    if (test_case->second_tag != NULL) {
        g_assert_cmpstr(task_get_tag_at(task, 1), ==, test_case->second_tag);
    }
}

static void test_cli_add_syntax_errors(gconstpointer data)
{
    const gchar *command = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;

    const CliStatus status = cli_execute_line(manager, command, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_assert_cmpstr(output->str, ==, "");
    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 0);
}

typedef struct
{
    const gchar *command;
    gint code;
} AddErrorCase;

static const AddErrorCase add_error_cases[] = {
    {NULL, TASK_ERROR_INVALID_PRIORITY},
    {"add 5 title #ok #Bad", TASK_ERROR_INVALID_TAG},
    {"add 5 b #y #y", TASK_ERROR_DUPLICATE_TAG},
};

static void test_cli_add_domain_errors(gconstpointer data)
{
    const AddErrorCase *test_case = data;
    g_autofree gchar *invalid_priority_command = g_strdup_printf("add %d title", MAX_PRIORITY + 1);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;
    const CliStatus status = cli_execute_line(manager, test_case->command ? test_case->command : invalid_priority_command, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, TASK_ERROR, test_case->code);
    g_assert_cmpstr(output->str, ==, "");
    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(stats.unique_tags, ==, 0);
    g_clear_error(&error);

    g_assert_cmpuint(cli_execute_line(manager, "add 5 b #y", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "added task 1\n");
}

static void test_cli_add_id_exhausted(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;

    g_assert_cmpuint(cli_execute_line(manager, "add 5 b #y", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "added task 1\n");
    g_string_truncate(output, 0);
    task_manager_set_next_id_for_test(manager, G_MAXUINT);

    g_assert_cmpuint(cli_execute_line(manager, "add 5 overflow #new", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, TM_ERROR, TM_ERROR_ID_EXHAUSTED);
    g_assert_cmpstr(output->str, ==, "");
    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.unique_tags, ==, 1);
}

static void test_cli_del(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    CliStatus status = cli_execute_line(manager, "del 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "deleted task 1\n");

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_null(task);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
}

typedef struct
{
    const gchar *command;
    gboolean unknown_id;
} DelInvalidCase;

static const DelInvalidCase del_invalid_cases[] = {
    {"del", FALSE},
    {"del 1 2", FALSE},
    {"del abc", FALSE},
    {"del 0", FALSE},
    {"del 999", TRUE},
};

static void test_cli_del_invalid(gconstpointer data)
{
    const DelInvalidCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    CliStatus status = cli_execute_line(manager, test_case->command, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    if (test_case->unknown_id) {
        g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    } else {
        g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    }
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
}

static void test_cli_tag_lifecycle(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    const struct
    {
        const gchar *command;
        const gchar *expected;
    } cases[] = {
        {"add 5 buy milk #home", "added task 1\n"},
        {"find #home", "id: 1, priority: 5, title: buy milk\n"},
        {"untag 1 #home", "untagged task 1 from home\n"},
        {"find #home", ""},
        {"del 1", "deleted task 1\n"},
        {"history", "add 5 buy milk #home\nfind #home\nuntag 1 #home\nfind #home\ndel 1\n"},
    };
    for (guint i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_autoptr(GError) error = NULL;
        g_string_truncate(output, 0);
        g_assert_cmpint(cli_execute_line(manager, cases[i].command, output, &error), ==, CLI_CONTINUE);
        g_assert_no_error(error);
        g_assert_cmpstr(output->str, ==, cases[i].expected);
    }
    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(stats.unique_tags, ==, 0);
}

static void test_cli_tag(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    CliStatus status = cli_execute_line(manager, "tag 1 #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "tagged task 1 with work\n");

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "work");

    g_autoptr(GPtrArray) tasks = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(tasks, 0) == task);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
}

static void test_cli_tag_duplicate(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    const guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    const CliStatus status = cli_execute_line(manager, "tag 1 #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    g_autoptr(GPtrArray) matches_after_duplicate = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(matches_after_duplicate);
    g_assert_cmpuint(matches_after_duplicate->len, ==, 1);

    const Task *tagged_task = g_ptr_array_index(matches_after_duplicate, 0);
    g_assert_cmpuint(task_get_id(tagged_task), ==, 1);
    g_assert_cmpuint(task_get_tag_count(tagged_task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(tagged_task, 0), ==, "work");
}

enum
{
    TAG_INVALID_MISSING_ARGUMENTS,
    TAG_INVALID_MISSING_TAG,
    TAG_INVALID_EXTRA_ARGUMENT,
    TAG_INVALID_NON_NUMERIC_ID,
    TAG_INVALID_ZERO_ID,
    TAG_INVALID_MISSING_HASH,
    TAG_INVALID_UPPERCASE_TAG,
    TAG_INVALID_EMPTY_TAG,
    TAG_UNKNOWN_ID,
};

static const InvalidCase tag_cases[] = {
    [TAG_INVALID_MISSING_ARGUMENTS] = {"tag", cli_error_quark, CLI_ERROR_USAGE},
    [TAG_INVALID_MISSING_TAG] = {"tag 1", cli_error_quark, CLI_ERROR_USAGE},
    [TAG_INVALID_EXTRA_ARGUMENT] = {"tag 1 #work extra", cli_error_quark, CLI_ERROR_USAGE},
    [TAG_INVALID_NON_NUMERIC_ID] = {"tag abc #work", cli_error_quark, CLI_ERROR_USAGE},
    [TAG_INVALID_ZERO_ID] = {"tag 0 #work", cli_error_quark, CLI_ERROR_USAGE},
    [TAG_INVALID_MISSING_HASH] = {"tag 1 work", cli_error_quark, CLI_ERROR_USAGE},
    [TAG_INVALID_UPPERCASE_TAG] = {"tag 1 #Bad", task_error_quark, TASK_ERROR_INVALID_TAG},
    [TAG_INVALID_EMPTY_TAG] = {"tag 1 #", task_error_quark, TASK_ERROR_INVALID_TAG},
    [TAG_UNKNOWN_ID] = {"tag 999 #work", task_manager_error_quark, TM_ERROR_TASK_NOT_FOUND},
};

static void test_cli_tag_invalid(gconstpointer data)
{
    const InvalidCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    g_test_message("%s", test_case->command);

    CliStatus status = cli_execute_line(manager, test_case->command, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, test_case->domain(), test_case->code);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);

    g_autoptr(GPtrArray) tasks = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 0);
}

static void test_cli_untag_missing_tag(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;

    g_assert_cmpint(cli_execute_line(manager, "add " G_STRINGIFY(MIN_PRIORITY) " one #1", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_string_truncate(output, 0);
    g_assert_cmpint(cli_execute_line(manager, "tag 1 #one", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_string_truncate(output, 0);

    g_assert_cmpint(cli_execute_line(manager, "untag 1 #three", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_TAG_NOT_FOUND);
    g_assert_cmpstr(error->message, ==, "tag not found: three");
    g_assert_cmpstr(output->str, ==, "");
    g_clear_error(&error);

    const Task *task = task_manager_find_task_by_id(manager, 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 2);
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "one");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_true(g_ptr_array_index(matches, 0) == task);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 2);
}

static void test_cli_untag(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id1 = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    guint id2 = task_manager_add_task(manager, "second", 7, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id1, ==, 1);
    g_assert_cmpuint(id2, ==, 2);

    g_assert_true(task_manager_tag_task(manager, id1, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id2, "work", &error));
    g_assert_no_error(error);

    // Remove the tag from the first task.
    CliStatus status = cli_execute_line(manager, "untag 1 #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "untagged task 1 from work\n");

    const Task *task1 = task_manager_find_task_by_id(manager, id1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task1);
    g_assert_cmpuint(task_get_tag_count(task1), ==, 0);

    const Task *task2 = task_manager_find_task_by_id(manager, id2, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task2);
    g_assert_cmpuint(task_get_tag_count(task2), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task2, 0), ==, "work");

    GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(tasks, 0) == task2);
    g_ptr_array_unref(tasks);

    g_string_truncate(output, 0);

    // Remove the last occurrence of the tag.
    status = cli_execute_line(manager, "untag 2 #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "untagged task 2 from work\n");
    g_assert_cmpuint(task_get_tag_count(task2), ==, 0);

    tasks = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 0);
    g_ptr_array_unref(tasks);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 2);
}

enum
{
    UNTAG_INVALID_MISSING_ARGUMENTS,
    UNTAG_INVALID_MISSING_TAG,
    UNTAG_INVALID_EXTRA_ARGUMENT,
    UNTAG_INVALID_NON_NUMERIC_ID,
    UNTAG_INVALID_ZERO_ID,
    UNTAG_INVALID_MISSING_HASH,
    UNTAG_INVALID_UPPERCASE_TAG,
    UNTAG_INVALID_EMPTY_TAG,
    UNTAG_UNKNOWN_ID,
};

static const InvalidCase untag_cases[] = {
    [UNTAG_INVALID_MISSING_ARGUMENTS] = {"untag", cli_error_quark, CLI_ERROR_USAGE},
    [UNTAG_INVALID_MISSING_TAG] = {"untag 1", cli_error_quark, CLI_ERROR_USAGE},
    [UNTAG_INVALID_EXTRA_ARGUMENT] = {"untag 1 #work extra", cli_error_quark, CLI_ERROR_USAGE},
    [UNTAG_INVALID_NON_NUMERIC_ID] = {"untag abc #work", cli_error_quark, CLI_ERROR_USAGE},
    [UNTAG_INVALID_ZERO_ID] = {"untag 0 #work", cli_error_quark, CLI_ERROR_USAGE},
    [UNTAG_INVALID_MISSING_HASH] = {"untag 1 work", cli_error_quark, CLI_ERROR_USAGE},
    [UNTAG_INVALID_UPPERCASE_TAG] = {"untag 1 #Bad", task_error_quark, TASK_ERROR_INVALID_TAG},
    [UNTAG_INVALID_EMPTY_TAG] = {"untag 1 #", task_error_quark, TASK_ERROR_INVALID_TAG},
    [UNTAG_UNKNOWN_ID] = {"untag 999 #work", task_manager_error_quark, TM_ERROR_TASK_NOT_FOUND},
};

static void test_cli_untag_invalid(gconstpointer data)
{
    const InvalidCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_test_message("%s", test_case->command);

    CliStatus status = cli_execute_line(manager, test_case->command, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, test_case->domain(), test_case->code);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "work");

    g_autoptr(GPtrArray) tasks = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(tasks, 0) == task);
}

static void test_cli_enqueue(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.queue_length, ==, 0);

    CliStatus status = cli_execute_line(manager, "enqueue 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "enqueued task 1\n");

    stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 1);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task_get_title(task), ==, "title");
}

enum
{
    ENQUEUE_INVALID_MISSING_ID,
    ENQUEUE_INVALID_EXTRA_ARGUMENT,
    ENQUEUE_INVALID_NON_NUMERIC_ID,
    ENQUEUE_INVALID_ZERO_ID,
    ENQUEUE_UNKNOWN_ID,
};

static const InvalidCase enqueue_cases[] = {
    [ENQUEUE_INVALID_MISSING_ID] = {"enqueue", cli_error_quark, CLI_ERROR_USAGE},
    [ENQUEUE_INVALID_EXTRA_ARGUMENT] = {"enqueue 1 2", cli_error_quark, CLI_ERROR_USAGE},
    [ENQUEUE_INVALID_NON_NUMERIC_ID] = {"enqueue abc", cli_error_quark, CLI_ERROR_USAGE},
    [ENQUEUE_INVALID_ZERO_ID] = {"enqueue 0", cli_error_quark, CLI_ERROR_USAGE},
    [ENQUEUE_UNKNOWN_ID] = {"enqueue 999", task_manager_error_quark, TM_ERROR_TASK_NOT_FOUND},
};

static void test_cli_enqueue_invalid(gconstpointer data)
{
    const InvalidCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    g_test_message("%s", test_case->command);

    CliStatus status = cli_execute_line(manager, test_case->command, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, test_case->domain(), test_case->code);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 0);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task_get_title(task), ==, "title");
}

static void test_cli_run(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id1 = task_manager_add_task(manager, "first", 3, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id1, ==, 1);

    guint id2 = task_manager_add_task(manager, "second", 9, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id2, ==, 2);

    CliStatus status = cli_execute_line(manager, "enqueue 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);

    status = cli_execute_line(manager, "enqueue 2", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_string_truncate(output, 0);

    TaskManagerStats stats;

    // FIFO runs the first enqueued task even though it has lower priority.
    status = cli_execute_line(manager, "run", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "running: first (id 1, priority 3)\n");

    const Task *task = task_manager_find_task_by_id(manager, id1, &error);
    g_assert_null(task);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    task = task_manager_find_task_by_id(manager, id2, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);

    stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 1);

    g_string_truncate(output, 0);

    status = cli_execute_line(manager, "run", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "running: second (id 2, priority 9)\n");

    stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(stats.queue_length, ==, 0);
}

static void test_cli_run_invalid(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    const guint id = task_manager_add_task(manager, "first", 3, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_enqueue_task(manager, id, &error));
    g_assert_no_error(error);
    g_assert_cmpint(cli_execute_line(manager, "run 1", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_assert_cmpstr(output->str, ==, "");
    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 1);
    g_assert_cmpuint(task_manager_get_stats(manager).queue_length, ==, 1);
}

static void test_cli_run_empty(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_assert_cmpint(cli_execute_line(manager, "run", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "queue is empty\n");
}

static void test_cli_find_no_matches(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;

    g_assert_cmpint(cli_execute_line(manager, "find #work", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");
}

static void test_cli_find_one_match(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_assert_cmpint(cli_execute_line(manager, "find #work", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "id: 1, priority: 5, title: first\n");
}

static void test_cli_find_multiple_matches(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GError) error = NULL;
    const guint first = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(first, ==, 1);
    const guint second = task_manager_add_task(manager, "second", 7, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(second, ==, 2);
    g_assert_true(task_manager_tag_task(manager, first, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, second, "work", &error));
    g_assert_no_error(error);

    g_assert_cmpint(cli_execute_line(manager, "find #work", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    // The search does not guarantee an order for matching tasks.
    g_assert_true(g_str_equal(output->str, "id: 1, priority: 5, title: first\nid: 2, priority: 7, title: second\n") ||
                  g_str_equal(output->str, "id: 2, priority: 7, title: second\nid: 1, priority: 5, title: first\n"));
}

enum
{
    FIND_INVALID_MISSING_TAG,
    FIND_INVALID_EXTRA_ARGUMENT,
    FIND_INVALID_MISSING_HASH,
};

static const InvalidCase find_cases[] = {
    [FIND_INVALID_MISSING_TAG] = {"find", cli_error_quark, CLI_ERROR_USAGE},
    [FIND_INVALID_EXTRA_ARGUMENT] = {"find #work extra", cli_error_quark, CLI_ERROR_USAGE},
    [FIND_INVALID_MISSING_HASH] = {"find work", cli_error_quark, CLI_ERROR_USAGE},
};

static void test_cli_find_invalid(gconstpointer data)
{
    const InvalidCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_test_message("%s", test_case->command);

    CliStatus status = cli_execute_line(manager, test_case->command, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, test_case->domain(), test_case->code);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task_get_title(task), ==, "title");
    g_assert_cmpint(task_get_priority(task), ==, 5);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "work");

    g_autoptr(GPtrArray) tasks = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(tasks, 0) == task);
}

static void test_cli_find_unmatched_invalid_format(gconstpointer data)
{
    const gchar *command = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    const guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_assert_cmpint(cli_execute_line(manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");
}

static void test_cli_top(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    CliStatus status = cli_execute_line(manager, "top 2", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");

    guint id1 = task_manager_add_task(manager, "low", 3, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id1, ==, 1);

    guint id2 = task_manager_add_task(manager, "high", 9, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id2, ==, 2);

    guint id3 = task_manager_add_task(manager, "medium", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id3, ==, 3);

    const struct
    {
        const gchar *command;
        const gchar *expected_output;
    } cases[] = {
        {"top 2", "id: 2, priority: 9, title: high\nid: 3, priority: 5, title: medium\n"},
        {"top 10", "id: 2, priority: 9, title: high\nid: 3, priority: 5, title: medium\nid: 1, priority: 3, title: low\n"},
        {"top 0", ""},
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_test_message("%s", cases[i].command);
        g_string_truncate(output, 0);

        status = cli_execute_line(manager, cases[i].command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_no_error(error);
        g_assert_cmpstr(output->str, ==, cases[i].expected_output);

        TaskManagerStats stats = task_manager_get_stats(manager);
        g_assert_cmpuint(stats.tasks, ==, 3);
    }
}

enum
{
    TOP_INVALID_MISSING_LIMIT,
    TOP_INVALID_EXTRA_ARGUMENT,
    TOP_INVALID_NON_NUMERIC,
    TOP_INVALID_NEGATIVE,
};

static const TopInvalidCase top_cases[] = {
    [TOP_INVALID_MISSING_LIMIT] = {"top"},
    [TOP_INVALID_EXTRA_ARGUMENT] = {"top 1 2"},
    [TOP_INVALID_NON_NUMERIC] = {"top abc"},
    [TOP_INVALID_NEGATIVE] = {"top -1"},
};

static void test_cli_top_invalid(gconstpointer data)
{
    const TopInvalidCase *test_case = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    g_test_message("%s", test_case->command);

    CliStatus status = cli_execute_line(manager, test_case->command, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task_get_title(task), ==, "title");
    g_assert_cmpint(task_get_priority(task), ==, 5);
}

static void test_cli_history_formats_commands(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    task_manager_record_command(manager, "add 5 first");
    task_manager_record_command(manager, "add 7 second");

    g_assert_cmpint(cli_execute_line(manager, "history", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "add 5 first\nadd 7 second\n");
}

static void test_cli_history_records_failed_command(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_assert_cmpint(cli_execute_line(manager, "del 999", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    g_assert_cmpint(cli_execute_line(manager, "history", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "del 999\n");
}

static void test_cli_history_does_not_record_history_commands(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    task_manager_record_command(manager, "existing");
    g_assert_cmpint(cli_execute_line(manager, "history extra", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);

    g_assert_cmpint(cli_execute_line(manager, "history", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "existing\n");
    g_string_truncate(output, 0);
    g_assert_cmpint(cli_execute_line(manager, "history", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "existing\n");
}

static void test_cli_history_limit(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    for (guint i = 1; i <= 11; ++i) {
        gchar *command = g_strdup_printf("add 5 task%u", i);

        CliStatus status = cli_execute_line(manager, command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_no_error(error);

        g_free(command);
        g_string_truncate(output, 0);
    }

    CliStatus status = cli_execute_line(manager, "history", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==,
        "add 5 task2\n"
        "add 5 task3\n"
        "add 5 task4\n"
        "add 5 task5\n"
        "add 5 task6\n"
        "add 5 task7\n"
        "add 5 task8\n"
        "add 5 task9\n"
        "add 5 task10\n"
        "add 5 task11\n");
}

static void test_cli_stats(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);

    CliStatus status = cli_execute_line(manager, "stats", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "tasks: 0\ntags: 0\nqueue: 0\n");
    g_string_truncate(output, 0);

    guint id1 = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id1, ==, 1);

    guint id2 = task_manager_add_task(manager, "second", 7, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id2, ==, 2);

    g_assert_true(task_manager_tag_task(manager, id1, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id2, "work", &error));
    g_assert_no_error(error);

    status = cli_execute_line(manager, "enqueue 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_string_truncate(output, 0);

    status = cli_execute_line(manager, "stats", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "tasks: 2\ntags: 1\nqueue: 1\n");
}

static void test_cli_stats_invalid(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    const CliStatus status = cli_execute_line(manager, "stats extra", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_assert_cmpstr(output->str, ==, "");
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_data_func("/cli/basic/empty", &basic_cases[0], test_cli_basic);
    g_test_add_data_func("/cli/basic/spaces", &basic_cases[1], test_cli_basic);
    g_test_add_data_func("/cli/basic/quit", &basic_cases[2], test_cli_basic);
    g_test_add_data_func("/cli/basic/unknown", &basic_cases[3], test_cli_basic);

    g_test_add_data_func("/cli/parse/numeric/add-suffix", &numeric_cases[PARSE_NUMERIC_ADD_SUFFIX], test_cli_numeric_tokens);
    g_test_add_data_func("/cli/parse/numeric/add-overflow", &numeric_cases[PARSE_NUMERIC_ADD_OVERFLOW], test_cli_numeric_tokens);
    g_test_add_data_func("/cli/parse/numeric/del-suffix", &numeric_cases[PARSE_NUMERIC_DEL_SUFFIX], test_cli_numeric_tokens);
    g_test_add_data_func("/cli/parse/numeric/del-overflow", &numeric_cases[PARSE_NUMERIC_DEL_OVERFLOW], test_cli_numeric_tokens);
    g_test_add_data_func("/cli/parse/numeric/top-suffix", &numeric_cases[PARSE_NUMERIC_TOP_SUFFIX], test_cli_numeric_tokens);
    g_test_add_data_func("/cli/parse/numeric/top-overflow", &numeric_cases[PARSE_NUMERIC_TOP_OVERFLOW], test_cli_numeric_tokens);
    g_test_add_func("/cli/parse/invalid-utf8", test_cli_invalid_utf8);

    g_test_add_data_func("/cli/add/multiword-title", &add_cases[0], test_cli_add);
    g_test_add_data_func("/cli/add/multiple-tags", &add_cases[1], test_cli_add);
    g_test_add_data_func("/cli/add/extra-spaces", &add_cases[2], test_cli_add);
    g_test_add_data_func("/cli/add/syntax/missing-arguments", "add", test_cli_add_syntax_errors);
    g_test_add_data_func("/cli/add/syntax/missing-title", "add 5", test_cli_add_syntax_errors);
    g_test_add_data_func("/cli/add/syntax/invalid-priority", "add x title", test_cli_add_syntax_errors);
    g_test_add_data_func("/cli/add/syntax/word-after-tag", "add 5 title #ok word", test_cli_add_syntax_errors);
    g_test_add_data_func("/cli/add/syntax/tag-without-title", "add 5 #tag", test_cli_add_syntax_errors);
    g_test_add_data_func("/cli/add/domain/invalid-priority", &add_error_cases[0], test_cli_add_domain_errors);
    g_test_add_data_func("/cli/add/domain/invalid-tag", &add_error_cases[1], test_cli_add_domain_errors);
    g_test_add_data_func("/cli/add/domain/duplicate-tag", &add_error_cases[2], test_cli_add_domain_errors);
    g_test_add_func("/cli/add/id-exhausted", test_cli_add_id_exhausted);

    g_test_add_func("/cli/del/success", test_cli_del);
    g_test_add_data_func("/cli/del/invalid/missing-id", &del_invalid_cases[0], test_cli_del_invalid);
    g_test_add_data_func("/cli/del/invalid/extra-argument", &del_invalid_cases[1], test_cli_del_invalid);
    g_test_add_data_func("/cli/del/invalid/non-numeric-id", &del_invalid_cases[2], test_cli_del_invalid);
    g_test_add_data_func("/cli/del/invalid/zero-id", &del_invalid_cases[3], test_cli_del_invalid);
    g_test_add_data_func("/cli/del/unknown-id", &del_invalid_cases[4], test_cli_del_invalid);

    g_test_add_func("/cli/tag/lifecycle", test_cli_tag_lifecycle);
    g_test_add_func("/cli/tag/success", test_cli_tag);
    g_test_add_func("/cli/tag/duplicate", test_cli_tag_duplicate);
    g_test_add_data_func("/cli/tag/invalid/missing-arguments", &tag_cases[TAG_INVALID_MISSING_ARGUMENTS], test_cli_tag_invalid);
    g_test_add_data_func("/cli/tag/invalid/missing-tag", &tag_cases[TAG_INVALID_MISSING_TAG], test_cli_tag_invalid);
    g_test_add_data_func("/cli/tag/invalid/extra-argument", &tag_cases[TAG_INVALID_EXTRA_ARGUMENT], test_cli_tag_invalid);
    g_test_add_data_func("/cli/tag/invalid/non-numeric-id", &tag_cases[TAG_INVALID_NON_NUMERIC_ID], test_cli_tag_invalid);
    g_test_add_data_func("/cli/tag/invalid/zero-id", &tag_cases[TAG_INVALID_ZERO_ID], test_cli_tag_invalid);
    g_test_add_data_func("/cli/tag/invalid/missing-hash", &tag_cases[TAG_INVALID_MISSING_HASH], test_cli_tag_invalid);
    g_test_add_data_func("/cli/tag/invalid/uppercase-tag", &tag_cases[TAG_INVALID_UPPERCASE_TAG], test_cli_tag_invalid);
    g_test_add_data_func("/cli/tag/invalid/empty-tag", &tag_cases[TAG_INVALID_EMPTY_TAG], test_cli_tag_invalid);
    g_test_add_data_func("/cli/tag/unknown-id", &tag_cases[TAG_UNKNOWN_ID], test_cli_tag_invalid);

    g_test_add_func("/cli/untag/success", test_cli_untag);
    g_test_add_func("/cli/untag/missing-tag", test_cli_untag_missing_tag);
    g_test_add_data_func("/cli/untag/invalid/missing-arguments", &untag_cases[UNTAG_INVALID_MISSING_ARGUMENTS], test_cli_untag_invalid);
    g_test_add_data_func("/cli/untag/invalid/missing-tag", &untag_cases[UNTAG_INVALID_MISSING_TAG], test_cli_untag_invalid);
    g_test_add_data_func("/cli/untag/invalid/extra-argument", &untag_cases[UNTAG_INVALID_EXTRA_ARGUMENT], test_cli_untag_invalid);
    g_test_add_data_func("/cli/untag/invalid/non-numeric-id", &untag_cases[UNTAG_INVALID_NON_NUMERIC_ID], test_cli_untag_invalid);
    g_test_add_data_func("/cli/untag/invalid/zero-id", &untag_cases[UNTAG_INVALID_ZERO_ID], test_cli_untag_invalid);
    g_test_add_data_func("/cli/untag/invalid/missing-hash", &untag_cases[UNTAG_INVALID_MISSING_HASH], test_cli_untag_invalid);
    g_test_add_data_func("/cli/untag/invalid/uppercase-tag", &untag_cases[UNTAG_INVALID_UPPERCASE_TAG], test_cli_untag_invalid);
    g_test_add_data_func("/cli/untag/invalid/empty-tag", &untag_cases[UNTAG_INVALID_EMPTY_TAG], test_cli_untag_invalid);
    g_test_add_data_func("/cli/untag/unknown-id", &untag_cases[UNTAG_UNKNOWN_ID], test_cli_untag_invalid);

    g_test_add_func("/cli/enqueue/success", test_cli_enqueue);
    g_test_add_data_func("/cli/enqueue/invalid/missing-id", &enqueue_cases[ENQUEUE_INVALID_MISSING_ID], test_cli_enqueue_invalid);
    g_test_add_data_func("/cli/enqueue/invalid/extra-argument", &enqueue_cases[ENQUEUE_INVALID_EXTRA_ARGUMENT], test_cli_enqueue_invalid);
    g_test_add_data_func("/cli/enqueue/invalid/non-numeric-id", &enqueue_cases[ENQUEUE_INVALID_NON_NUMERIC_ID], test_cli_enqueue_invalid);
    g_test_add_data_func("/cli/enqueue/invalid/zero-id", &enqueue_cases[ENQUEUE_INVALID_ZERO_ID], test_cli_enqueue_invalid);
    g_test_add_data_func("/cli/enqueue/unknown-id", &enqueue_cases[ENQUEUE_UNKNOWN_ID], test_cli_enqueue_invalid);

    g_test_add_func("/cli/run/success", test_cli_run);
    g_test_add_func("/cli/run/invalid", test_cli_run_invalid);
    g_test_add_func("/cli/run/empty", test_cli_run_empty);

    g_test_add_func("/cli/find/no-matches", test_cli_find_no_matches);
    g_test_add_func("/cli/find/one-match", test_cli_find_one_match);
    g_test_add_func("/cli/find/multiple-matches", test_cli_find_multiple_matches);
    g_test_add_data_func("/cli/find/invalid/missing-tag", &find_cases[FIND_INVALID_MISSING_TAG], test_cli_find_invalid);
    g_test_add_data_func("/cli/find/invalid/extra-argument", &find_cases[FIND_INVALID_EXTRA_ARGUMENT], test_cli_find_invalid);
    g_test_add_data_func("/cli/find/invalid/missing-hash", &find_cases[FIND_INVALID_MISSING_HASH], test_cli_find_invalid);
    g_test_add_data_func("/cli/find/unmatched/uppercase-tag", "find #Bad", test_cli_find_unmatched_invalid_format);
    g_test_add_data_func("/cli/find/unmatched/empty-tag", "find #", test_cli_find_unmatched_invalid_format);

    g_test_add_func("/cli/top/success", test_cli_top);
    g_test_add_data_func("/cli/top/invalid/missing-limit", &top_cases[TOP_INVALID_MISSING_LIMIT], test_cli_top_invalid);
    g_test_add_data_func("/cli/top/invalid/extra-argument", &top_cases[TOP_INVALID_EXTRA_ARGUMENT], test_cli_top_invalid);
    g_test_add_data_func("/cli/top/invalid/non-numeric", &top_cases[TOP_INVALID_NON_NUMERIC], test_cli_top_invalid);
    g_test_add_data_func("/cli/top/invalid/negative", &top_cases[TOP_INVALID_NEGATIVE], test_cli_top_invalid);

    g_test_add_func("/cli/history/format", test_cli_history_formats_commands);
    g_test_add_func("/cli/history/failed-command", test_cli_history_records_failed_command);
    g_test_add_func("/cli/history/not-recorded", test_cli_history_does_not_record_history_commands);
    g_test_add_func("/cli/history/limit", test_cli_history_limit);

    g_test_add_func("/cli/stats/success", test_cli_stats);
    g_test_add_func("/cli/stats/invalid", test_cli_stats_invalid);

    return g_test_run();
}
