#include "task.h"
#include "task_manager.h"
#include "task_manager_test.h"

#include <glib.h>

#include <stddef.h>

static void test_add_find(void)
{
    GError *error = NULL;

    gchar *title_first = "First";
    gint priority_first = 1;
    gchar *title_second = "Second";
    gint priority_second = 10;

    g_autoptr(TaskManager) manager = task_manager_new();
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
}

static void test_add_id_exhausted(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
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

static void test_add_rejects_invalid_priority_without_consuming_id(void)
{
    GError *error = NULL;

    gchar *title = "Invalid priority";
    gint priority = 1;
    gint big_priority = 11;
    gint small_priority = 0;

    g_autoptr(TaskManager) manager = task_manager_new();
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
    const gint expected_error = duplicate ? TASK_ERROR_DUPLICATE_TAG : TASK_ERROR_INVALID_TAG;
    g_assert_error(error, TASK_ERROR, expected_error);
    g_clear_error(&error);
    g_assert_cmpuint(task_manager_add_task_with_tags(manager, "invalid", 9, tags, NULL), ==, 0);

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.unique_tags, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 1);
    g_autoptr(GPtrArray) missing = task_manager_find_tasks_by_tag(manager, "new");
    g_assert_cmpuint(missing->len, ==, 0);
    g_autoptr(GPtrArray) existing = task_manager_find_tasks_by_tag(manager, "existing");
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
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "new");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, id);
    g_autoptr(GPtrArray) other_matches = task_manager_find_tasks_by_tag(manager, "another");
    g_assert_cmpuint(other_matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(other_matches, 0)), ==, id);
}

static void test_remove_removes_task_from_indexes(void)
{
    GError *error = NULL;
    const gchar *tag = "tag";
    const gchar *second_tag = "tag2";
    const gchar *title1 = "cmd";
    gint priority1 = 7;
    const gchar *title2 = "set";
    gint priority2 = 5;
    gboolean result = FALSE;

    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task1_id = task_manager_add_task(manager, title1, priority1, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(task1_id, !=, 0);
    g_assert_true(task_manager_tag_task(manager, task1_id, tag, &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, task1_id, second_tag, &error));
    g_assert_no_error(error);

    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(task2_id, !=, 0);
    const Task *task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task2);
    g_assert_true(task_manager_tag_task(manager, task2_id, tag, &error));
    g_assert_no_error(error);

    // The shared tag remains indexed for task2; the other tag disappears.
    result = task_manager_remove_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    g_autoptr(GPtrArray) unique_tag = task_manager_find_tasks_by_tag(manager, second_tag);
    g_assert_nonnull(unique_tag);
    g_assert_cmpint(unique_tag->len, ==, 0);

    g_autoptr(GPtrArray) default_tag = task_manager_find_tasks_by_tag(manager, tag);
    g_assert_cmpint(default_tag->len, ==, 1);
    g_assert_true(g_ptr_array_index(default_tag, 0) == task2);

    GPtrArray *tasks_after_delete = task_manager_top_tasks(manager, 1);
    g_assert_nonnull(tasks_after_delete);
    g_assert_cmpuint(tasks_after_delete->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks_after_delete, 0)), ==, task2_id);
    g_ptr_array_unref(tasks_after_delete);
}

static void test_remove_unknown_id(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;

    g_assert_false(task_manager_remove_task(manager, 999, &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_assert_false(task_manager_remove_task(manager, 999, NULL));
}

static void test_remove_twice(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_remove_task(manager, id, &error));
    g_assert_no_error(error);

    g_assert_false(task_manager_remove_task(manager, id, &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
}

static void test_remove_does_not_reuse_id(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_remove_task(manager, id, &error));
    g_assert_no_error(error);

    const guint next = task_manager_add_task(manager, "second", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(next, ==, id + 1);
}

static void test_remove_removes_all_queue_entries(void)
{
    GError *error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    guint first_task_id = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(first_task_id, !=, 0);
    guint second_task_id = task_manager_add_task(manager, "second", 2, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(second_task_id, !=, 0);

    g_assert_true(task_manager_enqueue_task(manager, first_task_id, &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_enqueue_task(manager, second_task_id, &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_enqueue_task(manager, first_task_id, &error));
    g_assert_no_error(error);

    Task *next_task = NULL;
    GPtrArray *top_tasks = NULL;

    // Queue: first, second, first. Removing first leaves second as the next entry.
    g_assert_true(task_manager_remove_task(manager, first_task_id, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(task_manager_get_stats(manager).queue_length, ==, 1);

    next_task = task_manager_extract_next_task(manager);
    g_assert_nonnull(next_task);
    g_assert_cmpuint(task_get_id(next_task), ==, second_task_id);
    g_assert_cmpuint(task_manager_get_stats(manager).queue_length, ==, 0);
    g_assert_null(task_manager_extract_next_task(manager));

    const Task *missing_task2 = task_manager_find_task_by_id(manager, second_task_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task2);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);
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
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "work");
}

static void test_tag_duplicate_leaves_task_unchanged(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    const guint other_id = task_manager_add_task(manager, "other", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, other_id, "work", &error));
    g_assert_no_error(error);

    g_assert_false(task_manager_tag_task(manager, id, "work", &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
    g_clear_error(&error);
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(matches->len, ==, 2);
    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    const Task *other = task_manager_find_task_by_id(manager, other_id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_nonnull(other);
    g_assert_true(g_ptr_array_find(matches, task, NULL));
    g_assert_true(g_ptr_array_find(matches, other, NULL));
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
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
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(matches->len, ==, 2);
    const Task *first_task = task_manager_find_task_by_id(manager, first, &error);
    g_assert_no_error(error);
    const Task *second_task = task_manager_find_task_by_id(manager, second, &error);
    g_assert_no_error(error);
    g_assert_nonnull(first_task);
    g_assert_nonnull(second_task);
    g_assert_true(g_ptr_array_find(matches, first_task, NULL));
    g_assert_true(g_ptr_array_find(matches, second_task, NULL));

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 2);
    g_assert_cmpuint(stats.unique_tags, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 0);
}

static void test_invalid_tag(gconstpointer data)
{
    const gchar *tag = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "existing", &error));
    g_assert_no_error(error);

    g_assert_false(task_manager_tag_task(manager, id, tag, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(task_manager_tag_task(manager, id, tag, NULL));

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "existing");
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "existing");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_true(g_ptr_array_index(matches, 0) == task);
}

static void test_tag_unknown_id(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;

    g_assert_false(task_manager_tag_task(manager, 999, "tag", &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
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
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
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
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
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

static void test_untag_twice(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint first_id = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    const guint second_id = task_manager_add_task(manager, "second", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, first_id, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, second_id, "home", &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_untag_task(manager, first_id, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_untag_task(manager, first_id, "work", &error));
    g_assert_no_error(error);

    const Task *first = task_manager_find_task_by_id(manager, first_id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(first);
    g_assert_cmpuint(task_get_tag_count(first), ==, 0);
    const Task *second = task_manager_find_task_by_id(manager, second_id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(second);
    g_assert_cmpuint(task_get_tag_count(second), ==, 1);
    g_assert_cmpstr(task_get_tag_at(second, 0), ==, "home");
    g_autoptr(GPtrArray) work_matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(work_matches->len, ==, 0);
    g_autoptr(GPtrArray) home_matches = task_manager_find_tasks_by_tag(manager, "home");
    g_assert_cmpuint(home_matches->len, ==, 1);
    g_assert_true(g_ptr_array_index(home_matches, 0) == second);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);
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
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
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
    const gchar *borrowed_tag = task_get_tag_at(first, 0);
    g_assert_true(task_manager_untag_task(manager, first_id, borrowed_tag, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(task_get_tag_count(first), ==, 0);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(matches, 0)), ==, second_id);

    const Task *second = task_manager_find_task_by_id(manager, second_id, &error);
    g_assert_no_error(error);
    borrowed_tag = task_get_tag_at(second, 0);
    g_assert_true(task_manager_untag_task(manager, second_id, borrowed_tag, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 0);

    g_autoptr(GPtrArray) empty = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_cmpuint(empty->len, ==, 0);
}

static void test_invalid_untag(gconstpointer data)
{
    const gchar *tag = data;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "existing", &error));
    g_assert_no_error(error);

    g_assert_false(task_manager_untag_task(manager, id, tag, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(task_manager_untag_task(manager, id, tag, NULL));

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "existing");
    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "existing");
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_true(g_ptr_array_index(matches, 0) == task);
}

static void test_untag_unknown_id(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;

    g_assert_false(task_manager_untag_task(manager, 999, "tag", &error));
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
}

static void test_find_by_id_reports_missing_task(void)
{
    GError *error = NULL;

    guint id = 50;

    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_null(task);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    task = task_manager_find_task_by_id(manager, id, NULL);
    g_assert_null(task);
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

    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task1_id = task_manager_add_task(manager, title1, priority1, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(task1_id, !=, 0);
    const Task *task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task1);
    g_assert_true(task_manager_tag_task(manager, task1_id, tag, &error));
    g_assert_no_error(error);

    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(task2_id, !=, 0);
    const Task *task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task2);
    g_assert_true(task_manager_tag_task(manager, task2_id, tag, &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, task2_id, tag_other, &error));
    g_assert_no_error(error);

    g_autoptr(GPtrArray) tasks = task_manager_find_tasks_by_tag(manager, tag);
    g_assert_nonnull(tasks);
    g_assert_cmpint(tasks->len, ==, 2);
    g_assert_true(g_ptr_array_find(tasks, task1, NULL));
    g_assert_true(g_ptr_array_find(tasks, task2, NULL));

    g_autoptr(GPtrArray) other_tasks = task_manager_find_tasks_by_tag(manager, tag_other);
    g_assert_nonnull(other_tasks);
    g_assert_cmpint(other_tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(other_tasks, 0) == task2);

    g_autoptr(GPtrArray) no_tasks = task_manager_find_tasks_by_tag(manager, unused_tag);
    g_assert_nonnull(no_tasks);
    g_assert_cmpuint(no_tasks->len, ==, 0);
}

static void test_find_by_tag_returns_independent_array(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 1);
    g_ptr_array_set_size(matches, 0);

    g_autoptr(GPtrArray) found_again = task_manager_find_tasks_by_tag(manager, "work");
    g_assert_nonnull(found_again);
    g_assert_cmpuint(found_again->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(found_again, 0)), ==, id);
    g_assert_cmpuint(task_manager_get_stats(manager).tasks, ==, 1);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 1);
}

static void test_find_by_tag_returns_empty_for_invalid_format(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    const gchar *tags[] = {"", "Tag", "t a g"};
    for (guint i = 0; i < G_N_ELEMENTS(tags); ++i) {
        g_autoptr(GPtrArray) matches = task_manager_find_tasks_by_tag(manager, tags[i]);
        g_assert_nonnull(matches);
        g_assert_cmpuint(matches->len, ==, 0);
    }
}

static void test_top_order(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint medium = task_manager_add_task(manager, "medium", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(medium, !=, 0);
    const guint low = task_manager_add_task(manager, "low", 2, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(low, !=, 0);
    const guint high = task_manager_add_task(manager, "high", 7, &error);
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
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint first = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(first, !=, 0);
    const guint second = task_manager_add_task(manager, "second", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(second, !=, 0);
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
    g_assert_no_error(error);
    g_assert_cmpuint(low, !=, 0);
    const guint high = task_manager_add_task(manager, "high", 7, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(high, !=, 0);

    g_autoptr(GPtrArray) tasks = task_manager_top_tasks(manager, limit);
    g_assert_cmpuint(tasks->len, ==, MIN(limit, 2));
    if (tasks->len > 0) {
        g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 0)), ==, high);
    }
    if (tasks->len > 1) {
        g_assert_cmpuint(task_get_id(g_ptr_array_index(tasks, 1)), ==, low);
    }
}

static void test_run_empty(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_null(task_manager_extract_next_task(manager));
}

static void test_run_fifo(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint first = task_manager_add_task(manager, "first", 2, &error);
    g_assert_no_error(error);
    const guint second = task_manager_add_task(manager, "second", 9, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_enqueue_task(manager, first, &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_enqueue_task(manager, second, &error));
    g_assert_no_error(error);

    g_autoptr(Task) first_run = task_manager_extract_next_task(manager);
    g_assert_nonnull(first_run);
    g_assert_cmpuint(task_get_id(first_run), ==, first);
    g_autoptr(Task) second_run = task_manager_extract_next_task(manager);
    g_assert_nonnull(second_run);
    g_assert_cmpuint(task_get_id(second_run), ==, second);
    g_assert_null(task_manager_extract_next_task(manager));
}

static void test_run_removes_indexes(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
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
    g_autoptr(Task) returned = task_manager_extract_next_task(manager);
    g_assert_nonnull(returned);
    g_assert_true(returned == original);
    g_assert_cmpuint(task_manager_get_stats(manager).unique_tags, ==, 0);

    g_clear_pointer(&manager, task_manager_free);
    g_assert_cmpuint(task_get_id(returned), ==, id);
    g_assert_cmpstr(task_get_title(returned), ==, "first");
    g_assert_cmpstr(task_get_tag_at(returned, 0), ==, "work");
    g_assert_true(task_add_tag(returned, "home", &error));
    g_assert_no_error(error);
    g_assert_true(task_set_priority(returned, 10, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(task_get_tag_at(returned, 1), ==, "home");
    g_assert_cmpint(task_get_priority(returned), ==, 10);
}

static void test_run_removes_duplicate_queue_entries(void)
{
    GError *error = NULL;
    g_autoptr(TaskManager) manager = task_manager_new();
    g_assert_nonnull(manager);

    guint first_task_id = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(first_task_id, !=, 0);
    guint second_task_id = task_manager_add_task(manager, "second", 2, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(second_task_id, !=, 0);

    g_assert_true(task_manager_enqueue_task(manager, first_task_id, &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_enqueue_task(manager, second_task_id, &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_enqueue_task(manager, first_task_id, &error));
    g_assert_no_error(error);

    Task *next_task = NULL;
    GPtrArray *top_tasks = NULL;

    // Queue: first, second, first. Running first also removes its second entry.
    next_task = task_manager_extract_next_task(manager);
    g_assert_nonnull(next_task);
    g_assert_cmpuint(task_get_id(next_task), ==, first_task_id);

    const Task *missing_task1 = task_manager_find_task_by_id(manager, first_task_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task1);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 1);
    g_assert_cmpuint(task_get_id(g_ptr_array_index(top_tasks, 0)), !=, first_task_id);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // second is next; the queue becomes empty after it runs.
    next_task = task_manager_extract_next_task(manager);
    g_assert_nonnull(next_task);
    g_assert_cmpuint(task_get_id(next_task), ==, second_task_id);

    const Task *missing_task2 = task_manager_find_task_by_id(manager, second_task_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(missing_task2);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    next_task = task_manager_extract_next_task(manager);
    g_assert_null(next_task);
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

static void test_stats_empty(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(stats.unique_tags, ==, 0);
    g_assert_cmpuint(stats.queue_length, ==, 0);
}

static void test_stats_repeated_queue_entries(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_enqueue_task(manager, id, &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_enqueue_task(manager, id, &error));
    g_assert_no_error(error);

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 2);
}

static void test_stats_after_deletion(void)
{
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GError) error = NULL;
    const guint id = task_manager_add_task(manager, "task", 5, &error);
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_enqueue_task(manager, id, &error));
    g_assert_no_error(error);

    g_assert_true(task_manager_remove_task(manager, id, &error));
    g_assert_no_error(error);
    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(stats.unique_tags, ==, 0);
    g_assert_cmpuint(stats.queue_length, ==, 0);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/task-manager/add/find", test_add_find);
    g_test_add_func("/task-manager/add/id-exhausted", test_add_id_exhausted);
    g_test_add_func("/task-manager/add/exhausted-before-validation", test_add_exhausted_before_validation);
    g_test_add_func("/task-manager/add/invalid-priority-id-not-consumed", test_add_rejects_invalid_priority_without_consuming_id);

    g_test_add_data_func("/task-manager/add-with-tags/invalid-atomic", GINT_TO_POINTER(FALSE), test_add_with_tags_atomic);
    g_test_add_data_func("/task-manager/add-with-tags/duplicate-atomic", GINT_TO_POINTER(TRUE), test_add_with_tags_atomic);
    g_test_add_func("/task-manager/add-with-tags/success", test_add_with_tags_success);

    g_test_add_func("/task-manager/remove/indexes", test_remove_removes_task_from_indexes);
    g_test_add_func("/task-manager/remove/unknown", test_remove_unknown_id);
    g_test_add_func("/task-manager/remove/twice", test_remove_twice);
    g_test_add_func("/task-manager/remove/id-not-reused", test_remove_does_not_reuse_id);
    g_test_add_func("/task-manager/remove/removes-queue-entries", test_remove_removes_all_queue_entries);

    g_test_add_func("/task-manager/tag/add", test_tag_adds_tag);
    g_test_add_func("/task-manager/tag/duplicate", test_tag_duplicate_leaves_task_unchanged);
    g_test_add_func("/task-manager/tag/shared", test_tag_shared_by_two_tasks);
    g_test_add_data_func("/task-manager/tag/invalid/empty", "", test_invalid_tag);
    g_test_add_data_func("/task-manager/tag/invalid/uppercase", "Tag", test_invalid_tag);
    g_test_add_data_func("/task-manager/tag/invalid/space", "t a g", test_invalid_tag);
    g_test_add_func("/task-manager/tag/unknown-id", test_tag_unknown_id);

    g_test_add_func("/task-manager/untag/shared", test_untag_shared_tag);
    g_test_add_func("/task-manager/untag/last", test_untag_last_occurrence);
    g_test_add_func("/task-manager/untag/missing", test_untag_missing_tag);
    g_test_add_func("/task-manager/untag/twice", test_untag_twice);
    g_test_add_func("/task-manager/untag/retag", test_retag_after_untag);
    g_test_add_func("/task-manager/untag/borrowed-tag", test_untag_borrowed_tag);
    g_test_add_data_func("/task-manager/untag/invalid/empty", "", test_invalid_untag);
    g_test_add_data_func("/task-manager/untag/invalid/uppercase", "Tag", test_invalid_untag);
    g_test_add_data_func("/task-manager/untag/invalid/space", "t a g", test_invalid_untag);
    g_test_add_func("/task-manager/untag/unknown-id", test_untag_unknown_id);

    g_test_add_func("/task-manager/find-by-id/not-found", test_find_by_id_reports_missing_task);

    g_test_add_func("/task-manager/find-by-tag/valid", test_find_by_tag);
    g_test_add_func("/task-manager/find-by-tag/independent-array", test_find_by_tag_returns_independent_array);
    g_test_add_func("/task-manager/find-by-tag/invalid-format-empty", test_find_by_tag_returns_empty_for_invalid_format);

    g_test_add_func("/task-manager/top/order", test_top_order);
    g_test_add_func("/task-manager/top/equal-priority", test_top_equal_priority);
    g_test_add_data_func("/task-manager/top/zero", GUINT_TO_POINTER(0), test_top_limit);
    g_test_add_data_func("/task-manager/top/limited", GUINT_TO_POINTER(1), test_top_limit);
    g_test_add_data_func("/task-manager/top/over-size", GUINT_TO_POINTER(1000), test_top_limit);

    g_test_add_func("/task-manager/run/empty", test_run_empty);
    g_test_add_func("/task-manager/run/fifo", test_run_fifo);
    g_test_add_func("/task-manager/run/indexes", test_run_removes_indexes);
    g_test_add_func("/task-manager/run/transfer", test_run_transfer);
    g_test_add_func("/task-manager/run/removes-duplicate-entries", test_run_removes_duplicate_queue_entries);

    g_test_add_func("/task-manager/history/empty", test_history_empty);
    g_test_add_func("/task-manager/history/ignores-empty", test_history_ignores_empty);
    g_test_add_func("/task-manager/history/order", test_history_order);
    g_test_add_func("/task-manager/history/eviction", test_history_eviction);
    g_test_add_func("/task-manager/history/snapshot", test_history_snapshot);

    g_test_add_func("/task-manager/stats/empty", test_stats_empty);
    g_test_add_func("/task-manager/stats/repeated-queue", test_stats_repeated_queue_entries);
    g_test_add_func("/task-manager/stats/after-deletion", test_stats_after_deletion);

    return g_test_run();
}
