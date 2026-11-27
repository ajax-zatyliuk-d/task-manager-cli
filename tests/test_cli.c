#include "cli.h"
#include "task.h"
#include "task_manager.h"

#include <glib.h>

#define VALID_PRIORITY (MIN_PRIORITY + MAX_PRIORITY) / 2

static void test_cli_invalid_arguments(gconstpointer data)
{
    const gchar *command = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    g_assert_cmpint(cli_execute_line(cli, manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_assert_cmpstr(output->str, ==, "");
}

static void test_cli_empty_quit_and_unknown(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    g_assert_cmpint(cli_execute_line(cli, manager, "   ", output, &error), ==, CLI_CONTINUE);
    g_assert_cmpint(cli_execute_line(cli, manager, "quit", output, &error), ==, CLI_QUIT);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");

    g_assert_cmpint(cli_execute_line(cli, manager, "unknown", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_UNKNOWN_COMMAND);
    g_assert_cmpstr(output->str, ==, "");
}

static void test_cli_rejects_invalid_utf8(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    const gchar command[] = "add 5 \xFF";

    g_assert_cmpint(cli_execute_line(cli, manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_assert_cmpstr(output->str, ==, "");

    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, 0);
}

static void test_cli_add(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    g_autofree gchar *command = g_strdup_printf("add \t %d  buy\t  milk  #home\t#shop", VALID_PRIORITY);

    g_assert_cmpint(cli_execute_line(cli, manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "added task 1\n");

    const Task *task = task_manager_find_task_by_id(manager, 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task_get_title(task), ==, "buy milk");
    g_assert_cmpint(task_get_priority(task), ==, VALID_PRIORITY);
    g_assert_cmpuint(task_get_tag_count(task), ==, 2);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "home");
    g_assert_cmpstr(task_get_tag_at(task, 1), ==, "shop");
}

static void test_cli_passes_task_error(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    g_autofree gchar *command = g_strdup_printf("add %d title #Bad", VALID_PRIORITY);

    g_assert_cmpint(cli_execute_line(cli, manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_assert_cmpstr(output->str, ==, "");
}

static void test_cli_del(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    const guint id = task_manager_add_task(manager, "task", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_autofree gchar *command = g_strdup_printf("del %u", id);
    g_autofree gchar *expected = g_strdup_printf("deleted task %u\n", id);

    g_assert_cmpint(cli_execute_line(cli, manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, expected);
}

static void test_cli_passes_manager_error(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    g_assert_cmpint(cli_execute_line(cli, manager, "del 999", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_assert_cmpstr(output->str, ==, "");
}

static void test_cli_tag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    const guint id = task_manager_add_task(manager, "task", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_autofree gchar *command = g_strdup_printf("tag %u #work", id);
    g_autofree gchar *expected = g_strdup_printf("tagged task %u with work\n", id);

    g_assert_cmpint(cli_execute_line(cli, manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, expected);
}

static void test_cli_untag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    const guint id = task_manager_add_task(manager, "task", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_autofree gchar *command = g_strdup_printf("untag %u #work", id);
    g_autofree gchar *expected = g_strdup_printf("untagged task %u from work\n", id);

    g_assert_cmpint(cli_execute_line(cli, manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, expected);
}

static void test_cli_enqueue(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    const guint id = task_manager_add_task(manager, "task", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_autofree gchar *command = g_strdup_printf("enqueue %u", id);
    g_autofree gchar *expected = g_strdup_printf("enqueued task %u\n", id);

    g_assert_cmpint(cli_execute_line(cli, manager, command, output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, expected);
}

static void test_cli_run_and_empty_queue(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    const guint id = task_manager_add_task(manager, "task", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_enqueue_task(manager, id, &error));
    g_assert_no_error(error);

    g_autofree gchar *expected = g_strdup_printf("running: task (id %u, priority %d)\n", id, VALID_PRIORITY);

    g_assert_cmpint(cli_execute_line(cli, manager, "run", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, expected);

    g_string_truncate(output, 0);

    g_assert_cmpint(cli_execute_line(cli, manager, "run", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "queue is empty\n");
}

static void test_cli_find_matches(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, first, "work", &error));
    g_assert_true(task_manager_tag_task(manager, second, "work", &error));
    g_assert_no_error(error);

    g_autofree gchar *forward = g_strdup_printf(
        "id: %u, priority: %d, title: first\nid: %u, priority: %d, title: second\n", first, VALID_PRIORITY, second, VALID_PRIORITY);
    g_autofree gchar *reverse = g_strdup_printf(
        "id: %u, priority: %d, title: second\nid: %u, priority: %d, title: first\n", second, VALID_PRIORITY, first, VALID_PRIORITY);

    g_assert_cmpint(cli_execute_line(cli, manager, "find #work", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_true(g_str_equal(output->str, forward) || g_str_equal(output->str, reverse));

    g_string_truncate(output, 0);

    g_assert_cmpint(cli_execute_line(cli, manager, "find #missing", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");
}

static void test_cli_top(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    task_manager_add_task(manager, "low", MIN_PRIORITY, &error);
    const guint high = task_manager_add_task(manager, "high", MAX_PRIORITY, &error);
    g_assert_no_error(error);

    g_autofree gchar *expected = g_strdup_printf("id: %u, priority: %d, title: high\n", high, MAX_PRIORITY);

    g_assert_cmpint(cli_execute_line(cli, manager, "top 1", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, expected);
}

static void test_cli_history_records_commands(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    g_assert_cmpint(cli_execute_line(cli, manager, "stats", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_string_truncate(output, 0);

    g_assert_cmpint(cli_execute_line(cli, manager, "top -1", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);

    g_assert_cmpint(cli_execute_line(cli, manager, "unknown", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_UNKNOWN_COMMAND);
    g_clear_error(&error);

    g_assert_cmpint(cli_execute_line(cli, manager, "history extra", output, &error), ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);

    g_assert_cmpint(cli_execute_line(cli, manager, "quit", output, &error), ==, CLI_QUIT);
    g_assert_cmpint(cli_execute_line(cli, manager, " ", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);

    g_assert_cmpint(cli_execute_line(cli, manager, "history", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "stats\ntop -1\nunknown\n");
}

static void test_cli_stats(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(Cli) cli = cli_new(&error);
    g_assert_no_error(error);
    g_assert_nonnull(cli);

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, first, "work", &error));
    g_assert_true(task_manager_enqueue_task(manager, first, &error));
    g_assert_true(task_manager_enqueue_task(manager, first, &error));
    g_assert_true(task_manager_enqueue_task(manager, second, &error));
    g_assert_no_error(error);

    g_assert_cmpint(cli_execute_line(cli, manager, "stats", output, &error), ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "tasks: 2\ntags: 1\nqueue: 3\n");
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/cli/general/empty-quit-unknown", test_cli_empty_quit_and_unknown);
    g_test_add_func("/cli/general/invalid-utf8", test_cli_rejects_invalid_utf8);

    g_test_add_func("/cli/add/success", test_cli_add);
    g_test_add_func("/cli/add/task-error", test_cli_passes_task_error);
    g_test_add_data_func("/cli/add/invalid/missing-arguments", "add", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/add/invalid/missing-title", "add 1", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/add/invalid/tag-before-title", "add 1 #tag", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/add/invalid/word-after-tag", "add 1 title #tag word", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/add/invalid/non-numeric", "add x title", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/add/invalid/suffix", "add 5x title", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/add/invalid/overflow", "add 999999999999999999999 title", test_cli_invalid_arguments);

    g_test_add_func("/cli/del/success", test_cli_del);
    g_test_add_data_func("/cli/del/invalid/missing-id", "del", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/del/invalid/extra", "del 1 2", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/del/invalid/non-numeric-id", "del abc", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/del/invalid/zero", "del 0", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/del/invalid/suffix", "del 1x", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/del/invalid/overflow", "del 999999999999999999999", test_cli_invalid_arguments);
    g_test_add_func("/cli/del/manager-error", test_cli_passes_manager_error);

    g_test_add_func("/cli/tag/success", test_cli_tag);
    g_test_add_data_func("/cli/tag/invalid/missing-tag", "tag 1", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/tag/invalid/no-arguments", "tag", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/tag/invalid/extra", "tag 1 #work extra", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/tag/invalid/missing-hash", "tag 1 work", test_cli_invalid_arguments);

    g_test_add_func("/cli/untag/success", test_cli_untag);
    g_test_add_data_func("/cli/untag/invalid/missing-tag", "untag 1", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/untag/invalid/no-arguments", "untag", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/untag/invalid/extra", "untag 1 #work extra", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/untag/invalid/missing-hash", "untag 1 work", test_cli_invalid_arguments);

    g_test_add_func("/cli/enqueue/success", test_cli_enqueue);
    g_test_add_data_func("/cli/enqueue/invalid/missing-id", "enqueue", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/enqueue/invalid/extra", "enqueue 1 2", test_cli_invalid_arguments);

    g_test_add_func("/cli/run/success-and-empty", test_cli_run_and_empty_queue);
    g_test_add_data_func("/cli/run/invalid/extra", "run 1", test_cli_invalid_arguments);

    g_test_add_func("/cli/find/success-and-empty", test_cli_find_matches);
    g_test_add_data_func("/cli/find/invalid/missing-tag", "find", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/find/invalid/extra", "find #work extra", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/find/invalid/missing-hash", "find work", test_cli_invalid_arguments);

    g_test_add_func("/cli/top/success", test_cli_top);
    g_test_add_data_func("/cli/top/invalid/missing-limit", "top", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/top/invalid/extra", "top 1 2", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/top/invalid/negative", "top -1", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/top/invalid/suffix", "top 1x", test_cli_invalid_arguments);
    g_test_add_data_func("/cli/top/invalid/overflow", "top 999999999999999999999", test_cli_invalid_arguments);

    g_test_add_func("/cli/history/records-commands", test_cli_history_records_commands);
    g_test_add_data_func("/cli/history/invalid/extra", "history extra", test_cli_invalid_arguments);

    g_test_add_func("/cli/stats/success", test_cli_stats);
    g_test_add_data_func("/cli/stats/invalid/extra", "stats extra", test_cli_invalid_arguments);

    return g_test_run();
}
