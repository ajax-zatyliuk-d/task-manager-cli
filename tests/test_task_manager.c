#include "task.h"
#include "task_manager.h"
#include "task_manager_test.h"

#include <glib.h>

#include <stddef.h>

static void test_run_transfer(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    const Task *original = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(original);
    g_assert_true(task_manager_enqueue_task(manager, id, &error));
    g_assert_no_error(error);
    g_autoptr(Task) returned = task_manager_run_next(manager);
    g_assert_nonnull(returned);
    g_assert_true(returned == original);
    g_assert_true(task_add_tag(returned, "home", &error));
    g_assert_no_error(error);
    g_assert_true(task_set_priority(returned, 10, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 0);
}

static void test_add_with_tags_atomic(gconstpointer data)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    g_ptr_array_add(tags, "existing");
    const guint first = task_manager_add_task_with_tags(manager, "first", 5, tags, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(first, ==, 1);
    g_assert_true(task_manager_enqueue_task(manager, first, &error));
    g_assert_no_error(error);

    g_ptr_array_set_size(tags, 0);
    g_ptr_array_add(tags, "new");
    g_ptr_array_add(tags, "another");
    const gboolean duplicate = GPOINTER_TO_INT(data);
    g_ptr_array_add(tags, duplicate ? "new" : "Bad");
    g_assert_cmpuint(task_manager_add_task_with_tags(manager, "invalid", 9, tags, &error), ==, 0);
    const TaskError expected_error = duplicate ? TASK_ERROR_DUPLICATE_TAG : TASK_ERROR_INVALID_TAG;
    g_assert_error(error, TASK_ERROR, expected_error);
    g_clear_error(&error);
    g_assert_cmpuint(task_manager_add_task_with_tags(manager, "invalid", 9, tags, NULL), ==, 0);
    g_clear_error(&error);

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.unique_tags, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 1);
    g_autoptr(GPtrArray) missing = task_manager_find_tasks_by_tag(manager, "new", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(missing->len, ==, 0);
    g_autoptr(GPtrArray) existing = task_manager_find_tasks_by_tag(manager, "existing", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(existing->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(existing, 0)), ==, first);
    g_autoptr(GPtrArray) top = task_manager_top_tasks(manager, 10);
    g_assert_cmpuint(top->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(top, 0)), ==, first);

    const guint second = task_manager_add_task(manager, "second", 9, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(second, ==, 2);
}

static void test_add_with_tags_success(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    g_ptr_array_add(tags, "new");
    g_ptr_array_add(tags, "another");
    const guint id = task_manager_add_task_with_tags(manager, "second", 9, tags, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "new", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, id);
    g_autoptr(GPtrArray) other_matches = task_manager_find_tasks_by_tag(manager, "another", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(other_matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(other_matches, 0)), ==, id);
}

static void test_add_find(void)
{
    GError *error = NULL;

    gchar *title_first = "First";
    gint priority_first = 1;
    gchar *title_second = "Second";
    gint priority_second = 10;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint id_first = task_manager_add_task(manager, title_first, priority_first, &error);
    g_assert_no_error(error);

    guint id_second = task_manager_add_task(manager, title_second, priority_second, &error);
    g_assert_no_error(error);

    const Task *task_first = task_manager_find_task_by_id(manager, id_first, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task_first);
    g_assert_cmpuint(task_get_tag_count(task_first), ==, 0);
    g_assert_nonnull(task_get_title(task_first));
    g_assert_cmpuint(task_get_id(task_first), ==, 1);
    g_assert_cmpint(task_get_priority(task_first), ==, priority_first);
    g_assert_cmpstr(task_get_title(task_first), ==, title_first);

    const Task *task_second = task_manager_find_task_by_id(manager, id_second, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task_second);
    g_assert_cmpuint(task_get_tag_count(task_second), ==, 0);
    g_assert_nonnull(task_get_title(task_second));
    g_assert_cmpuint(task_get_id(task_second), ==, 2);
    g_assert_cmpint(task_get_priority(task_second), ==, priority_second);
    g_assert_cmpstr(task_get_title(task_second), ==, title_second);

    task_manager_free(manager);
}

static void test_add_invalid(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    task_manager_set_next_id_for_test(manager, G_MAXUINT - 1);

    const guint last_id = task_manager_add_task(manager, "Last task", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(last_id, ==, G_MAXUINT - 1);

    const guint overflow_id = task_manager_add_task(manager, "Overflow", 5, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_ID_EXHAUSTED);
    g_assert_cmpuint(overflow_id, ==, 0);
    g_clear_error(&error);
    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 1);

    task_manager_set_next_id_for_test(manager, 0);

    const guint zero_id = task_manager_add_task(manager, "Zero ID", 5, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_ID_EXHAUSTED);
    g_assert_cmpuint(zero_id, ==, 0);
    g_clear_error(&error);
    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 1);

    const Task *task = task_manager_find_task_by_id(manager, last_id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task_get_title(task), ==, "Last task");

    task_manager_free(manager);
}

static void test_add_exhausted_before_validation(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint existing = task_manager_add_task(manager, "existing", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(existing, ==, 1);
    task_manager_set_next_id_for_test(manager, G_MAXUINT);

    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    g_ptr_array_add(tags, "Bad");
    g_assert_cmpuint(task_manager_add_task_with_tags(manager, "invalid", 0, tags, &error), ==, 0);
    g_assert_error(error, TM_ERROR, TM_ERROR_ID_EXHAUSTED);
    g_clear_error(&error);
    g_assert_cmpuint(task_manager_add_task_with_tags(manager, "invalid", 5, tags, &error), ==, 0);
    g_assert_error(error, TM_ERROR, TM_ERROR_ID_EXHAUSTED);
    g_clear_error(&error);
    g_assert_cmpuint(task_manager_add_task(manager, "invalid", 0, NULL), ==, 0);

    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 1);
    const Task *task = task_manager_find_task_by_id(manager, existing, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task_get_title(task), ==, "existing");
}

static void test_invalid_priority(void)
{
    GError *error = NULL;

    gchar *title = "Invalid priority";
    gint priority = 1;
    gint big_priority = 11;
    gint small_priority = 0;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint id = task_manager_add_task(manager, title, big_priority, &error);
    g_assert_cmpuint(id, ==, 0);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);

    id = task_manager_add_task(manager, title, small_priority, &error);
    g_assert_cmpuint(id, ==, 0);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);

    id = task_manager_add_task(manager, title, small_priority, NULL);
    g_assert_cmpuint(id, ==, 0);

    id = task_manager_add_task(manager, title, priority, &error);
    g_assert_no_error(error);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);

    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);
    g_assert_nonnull(task_get_title(task));
    g_assert_cmpuint(task_get_id(task), ==, 1);
    g_assert_cmpint(task_get_priority(task), ==, priority);
    g_assert_cmpstr(task_get_title(task), ==, title);

    task_manager_free(manager);
}

static void test_not_found(void)
{
    GError *error = NULL;

    guint id = 50;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_null(task);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    task = task_manager_find_task_by_id(manager, id, NULL);
    g_assert_null(task);

    task_manager_free(manager);
}

static void test_tag_adds_tag(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag(task, 0), ==, "work");
}

static void test_tag_duplicate_leaves_task_unchanged(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_assert_false(task_manager_tag_task(manager, id, "work", &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
    g_clear_error(&error);
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_tag_count(g_ptr_array_index(matches, 0)), ==, 1);
}

static void test_tag_shared_by_two_tasks(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint first = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    const guint second = task_manager_add_task(manager, "second", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, first, "work", &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, second, "work", &error));
    g_assert_no_error(error);
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);
}

static void test_invalid_tag(void)
{
    GError *error = NULL;

    const gchar *title = "cmd";
    gint priority = 1;

    const gchar *tag = "tag";
    const gchar *empty = "";
    const gchar *camel_case = "Tag";
    const gchar *space = "t a g";

    gboolean result = TRUE;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task_id = task_manager_add_task(manager, title, priority, &error);
    g_assert_no_error(error);

    const Task *task = task_manager_find_task_by_id(manager, task_id, &error);
    g_assert_no_error(error);

    result = task_manager_tag_task(manager, task_id, empty, &error);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);

    result = task_manager_tag_task(manager, task_id, camel_case, &error);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);

    result = task_manager_tag_task(manager, task_id, space, &error);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);

    result = task_manager_tag_task(manager, 999, tag, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(result);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);

    result = task_manager_tag_task(manager, task_id, space, NULL);
    g_assert_false(result);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);

    task_manager_free(manager);
}

static void test_find_by_tag(void)
{
    GError *error = NULL;
    const gchar *tag = "tag";
    const gchar *tag_other = "other";
    const gchar *unused_tag = "unused_tag";

    const gchar *title1 = "cmd";
    gint priority1 = 7;

    const gchar *title2 = "set";
    gint priority2 = 5;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    // first task
    guint task1_id = task_manager_add_task(manager, title1, priority1, &error);
    g_assert_no_error(error);
    const Task *task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_no_error(error);
    task_manager_tag_task(manager, task1_id, tag, &error);
    g_assert_no_error(error);

    // second task
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    g_assert_no_error(error);
    const Task *task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_no_error(error);
    task_manager_tag_task(manager, task2_id, tag, &error);
    task_manager_tag_task(manager, task2_id, tag_other, &error);
    g_assert_no_error(error);

    GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_nonnull(tasks);
    g_assert_cmpint(tasks->len, ==, 2);
    g_assert_true(g_ptr_array_index(tasks, 0) == task1);
    g_assert_true(g_ptr_array_index(tasks, 1) == task2);
    g_ptr_array_unref(tasks);

    GPtrArray *other_tasks = task_manager_find_tasks_by_tag(manager, tag_other, &error);
    g_assert_no_error(error);
    g_assert_nonnull(other_tasks);
    g_assert_cmpint(other_tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(other_tasks, 0) == task2);
    g_ptr_array_unref(other_tasks);

    GPtrArray *no_tasks = task_manager_find_tasks_by_tag(manager, unused_tag, &error);
    g_assert_no_error(error);
    g_assert_nonnull(no_tasks);
    g_assert_cmpuint(no_tasks->len, ==, 0);
    g_ptr_array_unref(no_tasks);

    GPtrArray *no_tasks_again = task_manager_find_tasks_by_tag(manager, unused_tag, NULL);
    g_assert_nonnull(no_tasks_again);
    g_assert_cmpuint(no_tasks_again->len, ==, 0);
    g_ptr_array_unref(no_tasks_again);

    gboolean result = task_manager_tag_task(manager, task2_id, tag, &error);
    g_assert_false(result);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
    g_clear_error(&error);

    GPtrArray *same_tasks = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_nonnull(same_tasks);
    g_assert_cmpint(same_tasks->len, ==, 2);
    g_assert_true((g_ptr_array_index(same_tasks, 0) == task1) && (g_ptr_array_index(same_tasks, 1) == task2));
    g_ptr_array_unref(same_tasks);

    task_manager_free(manager);
}

static void test_find_by_invalid_tag(void)
{
    GError *error = NULL;
    const gchar *empty = "";
    const gchar *camel_case = "Tag";
    const gchar *space = "t a g";

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GPtrArray *empty_tag_tasks = task_manager_find_tasks_by_tag(manager, empty, &error);
    g_assert_null(empty_tag_tasks);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);

    GPtrArray *camel_case_tag_tasks = task_manager_find_tasks_by_tag(manager, camel_case, &error);
    g_assert_null(camel_case_tag_tasks);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);

    GPtrArray *space_tag_tasks = task_manager_find_tasks_by_tag(manager, space, &error);
    g_assert_null(space_tag_tasks);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);

    GPtrArray *space_tag_tasks_again = task_manager_find_tasks_by_tag(manager, space, NULL);
    g_assert_null(space_tag_tasks_again);

    task_manager_free(manager);
}

static void test_untag_shared_tag(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint first = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    const guint second = task_manager_add_task(manager, "second", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, first, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, second, "work", &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_untag_task(manager, first, "work", &error));
    g_assert_no_error(error);
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, second);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);
}

static void test_untag_last_occurrence(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_untag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(matches->len, ==, 0);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 0);
}

static void test_untag_missing_tag(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_untag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 0);
}

static void test_retag_after_untag(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_untag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, id);
}

static void test_untag_borrowed_tag(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint first_id = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    const guint second_id = task_manager_add_task(manager, "second", 5, &error);
    g_assert_no_error(error);

    g_assert_true(task_manager_tag_task(manager, first_id, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, second_id, "work", &error));
    g_assert_no_error(error);

    const Task *first = task_manager_find_task_by_id(manager, first_id, &error);
    g_assert_no_error(error);
    const gchar *borrowed_tag = task_get_tag(first, 0);
    g_assert_true(task_manager_untag_task(manager, first_id, borrowed_tag, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(task_get_tag_count(first), ==, 0);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, second_id);

    const Task *second = task_manager_find_task_by_id(manager, second_id, &error);
    g_assert_no_error(error);
    borrowed_tag = task_get_tag(second, 0);
    g_assert_true(task_manager_untag_task(manager, second_id, borrowed_tag, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 0);

    g_autoptr(GPtrArray) empty = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(empty->len, ==, 0);
}

static void test_invalid_untag(void)
{
    GError *error = NULL;

    const gchar *title = "cmd";
    gint priority = 1;

    const gchar *tag = "tag";
    const gchar *empty = "";
    const gchar *camel_case = "Tag";
    const gchar *space = "t a g";

    gboolean result = TRUE;

    TaskManager *manager = task_manager_new();
    guint task_id = task_manager_add_task(manager, title, priority, &error);
    g_assert_no_error(error);
    g_assert_true(task_id != 0);

    result = task_manager_untag_task(manager, task_id, empty, &error);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);

    result = task_manager_untag_task(manager, task_id, camel_case, &error);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);

    result = task_manager_untag_task(manager, task_id, space, &error);
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);

    result = task_manager_untag_task(manager, 999, tag, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(result);

    result = task_manager_untag_task(manager, task_id, space, NULL);
    g_assert_false(result);

    task_manager_free(manager);
}

static void test_delete_by_valid_id(void)
{
    GError *error = NULL;
    const gchar *tag = "tag";
    const gchar *second_tag = "tag2";
    const gchar *title1 = "cmd";
    gint priority1 = 7;
    const gchar *title2 = "set";
    gint priority2 = 5;
    gboolean result = FALSE;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    // first task
    guint task1_id = task_manager_add_task(manager, title1, priority1, &error);
    task_manager_tag_task(manager, task1_id, tag, &error);
    task_manager_tag_task(manager, task1_id, second_tag, &error);

    // second task
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    const Task *task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    task_manager_tag_task(manager, task2_id, tag, &error);
    g_assert_no_error(error);

    // delete task with 2 tags
    result = task_manager_remove_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    GPtrArray *unique_tag = task_manager_find_tasks_by_tag(manager, second_tag, &error);
    g_assert_no_error(error);
    g_assert_nonnull(unique_tag);
    g_assert_cmpint(unique_tag->len, ==, 0);
    g_ptr_array_unref(unique_tag);

    GPtrArray *default_tag = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_cmpint(default_tag->len, ==, 1);
    g_assert_true(g_ptr_array_index(default_tag, 0) == task2);
    g_ptr_array_unref(default_tag);

    GPtrArray *tasks_after_delete = task_manager_top_tasks(manager, 1);
    g_assert_nonnull(tasks_after_delete);
    g_assert_cmpuint(tasks_after_delete->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks_after_delete, 0)), ==, task2_id);
    g_ptr_array_unref(tasks_after_delete);

    // delete deleted task id
    result = task_manager_remove_task(manager, task1_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(result);

    // delete unknown task id
    result = task_manager_remove_task(manager, 999, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(result);

    // delete unknown task id without error
    result = task_manager_remove_task(manager, 999, NULL);
    g_assert_false(result);

    // new_task_id != delete_task_id
    guint new_task1_id = task_manager_add_task(manager, title1, priority1, &error);
    g_assert_no_error(error);
    g_assert_false(task1_id == new_task1_id);
    g_assert_cmpint(new_task1_id, ==, 3);

    GPtrArray *tasks_after_adding = task_manager_top_tasks(manager, 1);
    g_assert_nonnull(tasks_after_adding);
    g_assert_cmpuint(tasks_after_adding->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks_after_adding, 0)), ==, new_task1_id);
    g_ptr_array_unref(tasks_after_adding);

    task_manager_free(manager);
}

static void test_top_order(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint medium = task_manager_add_task(manager, "medium", 5, &error);
    const guint low = task_manager_add_task(manager, "low", 2, &error);
    const guint high = task_manager_add_task(manager, "high", 7, &error);
    g_assert_no_error(error);
    g_autoptr(GPtrArray) tasks = task_manager_top_tasks(manager, 3);
    g_assert_cmpuint(tasks->len, ==, 3);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 0)), ==, high);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 1)), ==, medium);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 2)), ==, low);
}

static void test_top_equal_priority(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint first = task_manager_add_task(manager, "first", 5, &error);
    const guint second = task_manager_add_task(manager, "second", 5, &error);
    g_assert_no_error(error);
    g_autoptr(GPtrArray) tasks = task_manager_top_tasks(manager, 2);
    g_assert_cmpuint(tasks->len, ==, 2);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 0)), ==, first);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 1)), ==, second);
}

static void test_top_limit(gconstpointer data)
{
    const guint limit = GPOINTER_TO_UINT(data);
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint low = task_manager_add_task(manager, "low", 2, &error);
    const guint high = task_manager_add_task(manager, "high", 7, &error);
    g_assert_no_error(error);
    g_autoptr(GPtrArray) tasks = task_manager_top_tasks(manager, limit);
    g_assert_cmpuint(tasks->len, ==, MIN(limit, 2));
    if (tasks->len > 0) {
        g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 0)), ==, high);
    }
    if (tasks->len > 1) {
        g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 1)), ==, low);
    }
}

static void test_run(void)
{
    GError *error = NULL;
    const gchar *title1 = "cmd1";
    const gchar *title2 = "cmd2";
    const gchar *title3 = "cmd3";

    gint priority5 = 5;
    gint priority2 = 2;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    Task *empty_queue = task_manager_run_next(manager);
    g_assert_null(empty_queue);
    g_assert_no_error(error);

    guint task1_id = task_manager_add_task(manager, title1, priority5, &error);
    g_assert_no_error(error);
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    g_assert_no_error(error);
    guint task3_id = task_manager_add_task(manager, title3, priority2, &error);
    g_assert_no_error(error);

    gboolean result = FALSE;
    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task2_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task3_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    Task *next_task = NULL;
    GPtrArray *top_tasks = NULL;

    // run task1
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(task_get_id(next_task), ==, task1_id);

    const Task *missing_task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task1);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 2);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(top_tasks, 0)), !=, task1_id);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(top_tasks, 1)), !=, task1_id);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // run task2
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(task_get_id(next_task), ==, task2_id);

    const Task *missing_task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task2);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(top_tasks, 0)), !=, task2_id);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // run task3
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(task_get_id(next_task), ==, task3_id);

    const Task *missing_task3 = task_manager_find_task_by_id(manager, task3_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task3);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    task_manager_free(manager);
}

static void test_run_duplicated(void)
{
    GError *error = NULL;
    const gchar *title1 = "cmd1";
    const gchar *title2 = "cmd2";

    gint priority5 = 5;
    gint priority2 = 2;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task1_id = task_manager_add_task(manager, title1, priority5, &error);
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);

    g_assert_no_error(error);

    gboolean result = FALSE;
    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task2_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    Task *next_task = NULL;
    GPtrArray *top_tasks = NULL;

    // run task1
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(task_get_id(next_task), ==, task1_id);

    const Task *missing_task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task1);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(top_tasks, 0)), !=, task1_id);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // run task2
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(task_get_id(next_task), ==, task2_id);

    const Task *missing_task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task2);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // run epmty
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_null(next_task);

    const Task *missing_task1_again = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task1_again);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_manager_free(manager);
}

static void test_run_with_delete(void)
{
    GError *error = NULL;
    const gchar *title1 = "cmd1";
    const gchar *title2 = "cmd2";

    gint priority5 = 5;
    gint priority2 = 2;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task1_id = task_manager_add_task(manager, title1, priority5, &error);
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);

    g_assert_no_error(error);

    gboolean result = FALSE;
    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task2_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    Task *next_task = NULL;
    GPtrArray *top_tasks = NULL;

    // delete task1 and run next - get task2
    result = task_manager_remove_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(task_get_id(next_task), ==, task2_id);

    const Task *missing_task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task2);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    task_manager_free(manager);
}

static void test_history_empty(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, 0);
}

static void test_history_ignores_empty(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    task_manager_record_command(manager, "");
    task_manager_record_command(manager, NULL);
    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, 0);
}

static void test_history_order(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    task_manager_record_command(manager, "first");
    task_manager_record_command(manager, "second");
    task_manager_record_command(manager, "third");
    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, 3);
    g_assert_cmpstr(g_ptr_array_index(history, 0), ==, "first");
    g_assert_cmpstr(g_ptr_array_index(history, 1), ==, "second");
    g_assert_cmpstr(g_ptr_array_index(history, 2), ==, "third");
}

static void test_history_eviction(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    for (guint i = 1; i <= 11; ++i) {
        g_autofree gchar *command = g_strdup_printf("cmd%u", i);
        task_manager_record_command(manager, command);
    }
    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    g_assert_cmpuint(history->len, ==, 10);
    for (guint i = 0; i < history->len; ++i) {
        g_autofree gchar *expected = g_strdup_printf("cmd%u", i + 2);
        g_assert_cmpstr(g_ptr_array_index(history, i), ==, expected);
    }
}

static void test_history_snapshot(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    gchar command[] = "first";
    task_manager_record_command(manager, command);
    command[0] = 'X';
    g_autoptr(GPtrArray) snapshot = task_manager_get_history(manager);
    task_manager_record_command(manager, "second");
    g_assert_cmpuint(snapshot->len, ==, 1);
    g_assert_cmpstr(g_ptr_array_index(snapshot, 0), ==, "first");
    ((gchar *)g_ptr_array_index(snapshot, 0))[0] = 'X';
    g_autoptr(GPtrArray) current = task_manager_get_history(manager);
    g_assert_cmpuint(current->len, ==, 2);
    g_assert_cmpstr(g_ptr_array_index(current, 0), ==, "first");
    g_assert_cmpstr(g_ptr_array_index(current, 1), ==, "second");
}

static void test_stats(void)
{
    GError *error = NULL;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    TaskManagerStats statistics = {0};

    // no tasks
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 0);
    g_assert_cmpuint(statistics.tasks, ==, 0);
    g_assert_cmpuint(statistics.unique_tags, ==, 0);

    // 2 tasks, 2 tag
    guint task1_id = task_manager_add_task(manager, "cmd1", 7, &error);
    guint task2_id = task_manager_add_task(manager, "cmd2", 3, &error);

    task_manager_tag_task(manager, task1_id, "tag1", &error);
    task_manager_tag_task(manager, task1_id, "tag2", &error);
    task_manager_tag_task(manager, task2_id, "tag2", &error);

    g_assert_no_error(error);

    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 0);
    g_assert_cmpuint(statistics.tasks, ==, 2);
    g_assert_cmpuint(statistics.unique_tags, ==, 2);

    // enqueue one task 2 times -> queue size 2
    task_manager_enqueue_task(manager, task1_id, &error);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 1);
    g_assert_cmpuint(statistics.tasks, ==, 2);
    g_assert_cmpuint(statistics.unique_tags, ==, 2);

    task_manager_enqueue_task(manager, task1_id, &error);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 2);
    g_assert_cmpuint(statistics.tasks, ==, 2);
    g_assert_cmpuint(statistics.unique_tags, ==, 2);

    // untag task and unique tags = 1
    task_manager_untag_task(manager, task1_id, "tag1", &error);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 2);
    g_assert_cmpuint(statistics.tasks, ==, 2);
    g_assert_cmpuint(statistics.unique_tags, ==, 1);

    // del and run task
    Task *task = task_manager_run_next(manager);
    task_free(task);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 0);
    g_assert_cmpuint(statistics.tasks, ==, 1);
    g_assert_cmpuint(statistics.unique_tags, ==, 1);

    task_manager_remove_task(manager, task2_id, &error);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 0);
    g_assert_cmpuint(statistics.tasks, ==, 0);
    g_assert_cmpuint(statistics.unique_tags, ==, 0);

    task_manager_free(manager);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/task-manager/add-find", test_add_find);
    g_test_add_data_func("/task-manager/add-with-tags/invalid-atomic", GINT_TO_POINTER(FALSE), test_add_with_tags_atomic);
    g_test_add_data_func("/task-manager/add-with-tags/duplicate-atomic", GINT_TO_POINTER(TRUE), test_add_with_tags_atomic);
    g_test_add_func("/task-manager/add-with-tags/success", test_add_with_tags_success);
    g_test_add_func("/task-manager/add-invalid", test_add_invalid);
    g_test_add_func("/task-manager/add/exhausted-before-validation", test_add_exhausted_before_validation);
    g_test_add_func("/task-manager/invalid-priority", test_invalid_priority);
    g_test_add_func("/task-manager/not-found", test_not_found);

    g_test_add_func("/task-manager/tag/add", test_tag_adds_tag);
    g_test_add_func("/task-manager/tag/duplicate", test_tag_duplicate_leaves_task_unchanged);
    g_test_add_func("/task-manager/tag/shared", test_tag_shared_by_two_tasks);
    g_test_add_func("/task-manager/tag/invalid", test_invalid_tag);
    g_test_add_func("/task-manager/tag/find-valid", test_find_by_tag);
    g_test_add_func("/task-manager/tag/find-invalid", test_find_by_invalid_tag);

    g_test_add_func("/task-manager/untag/shared", test_untag_shared_tag);
    g_test_add_func("/task-manager/untag/last", test_untag_last_occurrence);
    g_test_add_func("/task-manager/untag/missing", test_untag_missing_tag);
    g_test_add_func("/task-manager/untag/retag", test_retag_after_untag);
    g_test_add_func("/task-manager/untag/borrowed-tag", test_untag_borrowed_tag);
    g_test_add_func("/task-manager/untag/invalid", test_invalid_untag);

    g_test_add_func("/task-manager/tag/delete", test_delete_by_valid_id);

    g_test_add_func("/task-manager/top/order", test_top_order);
    g_test_add_func("/task-manager/top/equal-priority", test_top_equal_priority);
    g_test_add_data_func("/task-manager/top/zero", GUINT_TO_POINTER(0), test_top_limit);
    g_test_add_data_func("/task-manager/top/limited", GUINT_TO_POINTER(1), test_top_limit);
    g_test_add_data_func("/task-manager/top/over-size", GUINT_TO_POINTER(1000), test_top_limit);

    g_test_add_func("/task-manager/run", test_run);
    g_test_add_func("/task-manager/run/transfer", test_run_transfer);
    g_test_add_func("/task-manager/run/duplicated", test_run_duplicated);
    g_test_add_func("/task-manager/run/with-delete", test_run_with_delete);

    g_test_add_func("/task-manager/history/empty", test_history_empty);
    g_test_add_func("/task-manager/history/ignores-empty", test_history_ignores_empty);
    g_test_add_func("/task-manager/history/order", test_history_order);
    g_test_add_func("/task-manager/history/eviction", test_history_eviction);
    g_test_add_func("/task-manager/history/snapshot", test_history_snapshot);

    g_test_add_func("/task-manager/stats", test_stats);

    return g_test_run();
}
