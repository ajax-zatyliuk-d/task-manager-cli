#include "task.h"
#include "task_manager.h"
#include "task_manager_test.h"

#include <glib.h>

#include <stddef.h>

#define VALID_PRIORITY (MIN_PRIORITY + (MAX_PRIORITY - MIN_PRIORITY) / 2)

static void test_add_find(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(first, ==, 1);
    g_assert_cmpuint(second, ==, 2);

    const Task *first_task = task_manager_find_task_by_id(manager, first, &error);
    const Task *second_task = task_manager_find_task_by_id(manager, second, &error);
    g_assert_no_error(error);
    g_assert_nonnull(first_task);
    g_assert_nonnull(second_task);

    g_assert_cmpuint(task_get_id(first_task), ==, first);
    g_assert_cmpuint(task_get_id(second_task), ==, second);
}

static void test_add_id_exhausted(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();
    task_manager_set_next_id_for_test(manager, G_MAXUINT - 1);

    const guint last_id = task_manager_add_task(manager, "last", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(last_id, ==, G_MAXUINT - 1);

    g_assert_cmpuint(task_manager_add_task(manager, "overflow", VALID_PRIORITY, &error), ==, 0);
    g_assert_error(error, TM_ERROR, TM_ERROR_ID_EXHAUSTED);
    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 1);
}

static void test_add_with_tags_success(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    g_ptr_array_add(tags, "new");
    g_ptr_array_add(tags, "another");

    const guint id = task_manager_add_task_with_tags(manager, "second", VALID_PRIORITY, tags, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "new");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, id);

    g_autoptr(GPtrArray) other_matches = task_manager_find_tasks_by_tag(manager, "another");
    g_assert_cmpuint(other_matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(other_matches, 0)), ==, id);
}

static void test_add_with_tags_invalid_atomic(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    g_ptr_array_add(tags, "new");
    g_ptr_array_add(tags, "Bad");

    g_assert_cmpuint(task_manager_add_task_with_tags(manager, "invalid", VALID_PRIORITY, tags, &error), ==, 0);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "new");
    g_autoptr(GPtrArray) top = task_manager_top_tasks(manager, 10);
    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(matches->len, ==, 0);
    g_assert_cmpuint(top->len, ==, 0);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(stats.unique_tags, ==, 0);

    const guint id = task_manager_add_task(manager, "valid", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);
}

static void test_remove_task_from_indexes(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, first, "shared", &error));
    g_assert_true(task_manager_tag_task(manager, first, "only_first", &error));
    g_assert_true(task_manager_tag_task(manager, second, "shared", &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_remove_task(manager, first, &error));
    g_assert_no_error(error);

    g_assert_null(task_manager_find_task_by_id(manager, first, &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    g_autoptr(GPtrArray) shared = task_manager_find_tasks_by_tag(manager, "shared");
    g_autoptr(GPtrArray) removed = task_manager_find_tasks_by_tag(manager, "only_first");
    g_autoptr(GPtrArray) top = task_manager_top_tasks(manager, 10);
    g_assert_cmpuint(shared->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(shared, 0)), ==, second);
    g_assert_cmpuint(removed->len, ==, 0);
    g_assert_cmpuint(top->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(top, 0)), ==, second);

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.unique_tags, ==, 1);
}

static void test_remove_twice(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint id = task_manager_add_task(manager, "task", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_remove_task(manager, id, &error));
    g_assert_no_error(error);

    g_assert_false(task_manager_remove_task(manager, id, &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
}

static void test_remove_does_not_reuse_id(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint id = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_remove_task(manager, id, &error));
    g_assert_no_error(error);

    const guint next = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(next, ==, id + 1);
}

static void test_remove_removes_all_queue_entries(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_enqueue_task(manager, first, &error));
    g_assert_true(task_manager_enqueue_task(manager, second, &error));
    g_assert_true(task_manager_enqueue_task(manager, first, &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_remove_task(manager, first, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(task_manager_get_stats(manager).queue_length, ==, 1);

    g_autoptr(Task) next = task_manager_extract_next_task(manager);
    g_assert_nonnull(next);
    g_assert_cmpuint(task_get_id(next), ==, second);

    g_assert_null(task_manager_extract_next_task(manager));
}

static void test_tag_duplicate_keeps_index_unchanged(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint id = task_manager_add_task(manager, "task", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_assert_false(task_manager_tag_task(manager, id, "work", &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, id);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);
}

static void test_tag_unknown_id(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    g_assert_false(task_manager_tag_task(manager, 999, "tag", &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
}

static void test_untag_shared_tag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, first, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, second, "work", &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_untag_task(manager, first, "work", &error));
    g_assert_no_error(error);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, second);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);
}

static void test_untag_missing_tag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, second, "work", &error));
    g_assert_no_error(error);

    g_assert_false(task_manager_untag_task(manager, first, "work", &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_TAG_NOT_FOUND);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, second);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);
}

static void test_untag_unknown_id(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    g_assert_false(task_manager_untag_task(manager, 999, "tag", &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
}

static void test_find_by_id_reports_missing_task(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    g_assert_null(task_manager_find_task_by_id(manager, 50, &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
}

static void test_find_by_tag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, first, "work", &error));
    g_assert_true(task_manager_tag_task(manager, second, "work", &error));
    g_assert_true(task_manager_tag_task(manager, second, "home", &error));
    g_assert_no_error(error);

    const Task *first_task = task_manager_find_task_by_id(manager, first, &error);
    const Task *second_task = task_manager_find_task_by_id(manager, second, &error);
    g_assert_no_error(error);

    g_autoptr(GPtrArray) work = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(work->len, ==, 2);
    g_assert_true(g_ptr_array_find(work, first_task, NULL));
    g_assert_true(g_ptr_array_find(work, second_task, NULL));

    g_autoptr(GPtrArray) home = task_manager_find_tasks_by_tag(manager, "home");
    g_assert_cmpuint(home->len, ==, 1);
    g_assert_true(g_ptr_array_index(home, 0) == second_task);

    g_autoptr(GPtrArray) missing = task_manager_find_tasks_by_tag(manager, "missing");
    g_assert_cmpuint(missing->len, ==, 0);
}

static void test_top_order(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint medium = task_manager_add_task(manager, "medium", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(medium, !=, 0);

    const guint low = task_manager_add_task(manager, "low", MIN_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(low, !=, 0);

    const guint high = task_manager_add_task(manager, "high", MAX_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(high, !=, 0);

    g_autoptr(GPtrArray) tasks = task_manager_top_tasks(manager, 3);
    g_assert_cmpuint(tasks->len, ==, 3);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 0)), ==, high);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 1)), ==, medium);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 2)), ==, low);
}

static void test_top_equal_priority(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(first, !=, 0);

    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(second, !=, 0);

    g_autoptr(GPtrArray) tasks = task_manager_top_tasks(manager, 2);
    g_assert_cmpuint(tasks->len, ==, 2);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 0)), ==, first);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 1)), ==, second);
}

static void test_top_limit(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    task_manager_add_task(manager, "low", MIN_PRIORITY, &error);
    const guint high = task_manager_add_task(manager, "high", MAX_PRIORITY, &error);
    g_assert_no_error(error);

    g_autoptr(GPtrArray) zero = task_manager_top_tasks(manager, 0);
    g_assert_cmpuint(zero->len, ==, 0);

    g_autoptr(GPtrArray) one = task_manager_top_tasks(manager, 1);
    g_assert_cmpuint(one->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(one, 0)), ==, high);

    g_autoptr(GPtrArray) all = task_manager_top_tasks(manager, 10);
    g_assert_cmpuint(all->len, ==, 2);
}

static void test_enqueue_task_unknown_id(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    g_assert_false(task_manager_enqueue_task(manager, 999, &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
}

static void test_extract_next_task_removes_indexes(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint id = task_manager_add_task(manager, "task", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_enqueue_task(manager, id, &error));
    g_assert_no_error(error);

    g_autoptr(Task) task = task_manager_extract_next_task(manager);
    g_assert_nonnull(task);
    g_assert_null(task_manager_find_task_by_id(manager, id, &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(matches->len, ==, 0);
    g_autoptr(GPtrArray) top = task_manager_top_tasks(manager, 10);
    g_assert_cmpuint(top->len, ==, 0);
    g_assert_cmpuint(task_manager_get_stats(manager).queue_length, ==, 0);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 0);
}

static void test_extract_next_task_transfers_ownership(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint id = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    g_assert_no_error(error);
    const Task *original = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(original);
    g_assert_true(task_manager_enqueue_task(manager, id, &error));
    g_assert_no_error(error);

    g_autoptr(Task) returned = task_manager_extract_next_task(manager);
    g_assert_nonnull(returned);
    g_assert_true(returned == original);

    g_clear_pointer(&manager, task_manager_free);
    g_assert_cmpuint(task_get_id(returned), ==, id);
}

static void test_record_command_ignores_empty(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    task_manager_record_command(manager, "");
    task_manager_record_command(manager, NULL);

    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, 0);
}

static void test_record_command_history_limit(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    for (guint i = 1; i <= CMD_HISTORY_LEN + 1; ++i) {
        g_autofree gchar *command = g_strdup_printf("cmd%u", i);
        task_manager_record_command(manager, command);
    }

    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, CMD_HISTORY_LEN);
    for (guint i = 0; i < history->len; ++i) {
        g_autofree gchar *expected = g_strdup_printf("cmd%u", i + 2);
        g_assert_cmpstr(g_ptr_array_index(history, i), ==, expected);
    }
}

static void test_get_stats_empty(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(stats.unique_tags, ==, 0);
    g_assert_cmpuint(stats.queue_length, ==, 0);
}

static void test_get_stats_repeated_queue_entries(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();

    const guint first = task_manager_add_task(manager, "first", VALID_PRIORITY, &error);
    const guint second = task_manager_add_task(manager, "second", VALID_PRIORITY, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, first, "work", &error));
    g_assert_true(task_manager_tag_task(manager, second, "work", &error));
    g_assert_true(task_manager_enqueue_task(manager, first, &error));
    g_assert_true(task_manager_enqueue_task(manager, first, &error));
    g_assert_true(task_manager_enqueue_task(manager, second, &error));
    g_assert_no_error(error);

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 2);
    g_assert_cmpuint(stats.unique_tags, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 3);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/task-manager/add/find", test_add_find);
    g_test_add_func("/task-manager/add/id-exhausted", test_add_id_exhausted);

    g_test_add_func("/task-manager/add-with-tags/success", test_add_with_tags_success);
    g_test_add_func("/task-manager/add-with-tags/invalid-atomic", test_add_with_tags_invalid_atomic);

    g_test_add_func("/task-manager/remove/indexes", test_remove_task_from_indexes);
    g_test_add_func("/task-manager/remove/twice", test_remove_twice);
    g_test_add_func("/task-manager/remove/id-not-reused", test_remove_does_not_reuse_id);
    g_test_add_func("/task-manager/remove/removes-queue-entries", test_remove_removes_all_queue_entries);

    g_test_add_func("/task-manager/tag/duplicate", test_tag_duplicate_keeps_index_unchanged);
    g_test_add_func("/task-manager/tag/unknown-id", test_tag_unknown_id);

    g_test_add_func("/task-manager/untag/shared", test_untag_shared_tag);
    g_test_add_func("/task-manager/untag/missing", test_untag_missing_tag);
    g_test_add_func("/task-manager/untag/unknown-id", test_untag_unknown_id);

    g_test_add_func("/task-manager/find-by-id/not-found", test_find_by_id_reports_missing_task);

    g_test_add_func("/task-manager/find-by-tag/valid", test_find_by_tag);

    g_test_add_func("/task-manager/top/order", test_top_order);
    g_test_add_func("/task-manager/top/equal-priority", test_top_equal_priority);
    g_test_add_func("/task-manager/top/limit", test_top_limit);

    g_test_add_func("/task-manager/enqueue-task/unknown-id", test_enqueue_task_unknown_id);

    g_test_add_func("/task-manager/extract-next-task/indexes", test_extract_next_task_removes_indexes);
    g_test_add_func("/task-manager/extract-next-task/transfer", test_extract_next_task_transfers_ownership);

    g_test_add_func("/task-manager/record-command/ignores-empty", test_record_command_ignores_empty);
    g_test_add_func("/task-manager/record-command/eviction", test_record_command_history_limit);

    g_test_add_func("/task-manager/get-stats/empty", test_get_stats_empty);
    g_test_add_func("/task-manager/get-stats/repeated-queue", test_get_stats_repeated_queue_entries);

    return g_test_run();
}
