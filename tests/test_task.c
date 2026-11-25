#include "task.h"

#include <glib.h>

#include <stddef.h>

static void test_task_new(void)
{
    gchar title[] = "Test Task";
    guint id = 1;
    gint priority = 5;

    Task *task = task_new(id, title, priority, NULL);
    g_assert_nonnull(task);
    g_assert_nonnull(task_get_title(task));

    g_assert_cmpuint(task_get_id(task), ==, id);
    g_assert_cmpstr(task_get_title(task), ==, title);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);
    g_assert_cmpint(task_get_priority(task), ==, priority);

    title[0] = 'X';
    g_assert_cmpstr(task_get_title(task), ==, "Test Task");

    task_free(task);
}

static void test_task_validation(void)
{
    g_autoptr(GError) error = NULL;
    g_assert_null(task_new(1, "invalid", 0, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);
    g_assert_null(task_new(1, "invalid", 0, NULL));
    g_assert_null(task_new(1, "invalid", 11, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);

    g_autoptr(Task) task = task_new(1, "valid", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_true(task_set_priority(task, 10, &error));
    g_assert_no_error(error);
    g_assert_false(task_set_priority(task, 11, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);
    g_assert_cmpint(task_get_priority(task), ==, 10);

    const gchar *invalid[] = {NULL, "", "Bad", "a-b"};
    for (guint i = 0; i < G_N_ELEMENTS(invalid); ++i) {
        g_assert_false(task_add_tag(task, invalid[i], &error));
        g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
        g_clear_error(&error);
        g_assert_cmpuint(task_get_tag_count(task), ==, 0);
    }
    g_assert_true(task_add_tag(task, "work_1", &error));
    g_assert_no_error(error);
    g_assert_false(task_add_tag(task, "work_1", &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
    g_clear_error(&error);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    gboolean removed = FALSE;
    g_assert_false(task_remove_tag(task, "Bad", &removed, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_true(task_remove_tag(task, "work_1", &removed, &error));
    g_assert_no_error(error);
    g_assert_true(removed);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);
    g_assert_true(task_remove_tag(task, "work_1", &removed, &error));
    g_assert_no_error(error);
    g_assert_false(removed);
}

static void test_task_new_with_tags(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) tags = g_ptr_array_new_with_free_func(g_free);
    g_autoptr(Task) empty = task_new_with_tags(1, "empty", 5, tags, &error);
    g_assert_no_error(error);
    g_assert_nonnull(empty);
    g_assert_cmpuint(task_get_tag_count(empty), ==, 0);

    g_ptr_array_add(tags, g_strdup("work"));
    g_ptr_array_add(tags, g_strdup("home_1"));
    g_autoptr(Task) task = task_new_with_tags(2, "tagged", 5, tags, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 2);
    ((gchar *)g_ptr_array_index(tags, 0))[0] = 'X';
    g_ptr_array_set_size(tags, 0);
    g_assert_cmpstr(task_get_tag(task, 0), ==, "work");
    g_assert_cmpstr(task_get_tag(task, 1), ==, "home_1");
}

static void test_task_new_with_tags_errors(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    g_ptr_array_add(tags, "work");
    g_ptr_array_add(tags, "home");
    g_ptr_array_add(tags, "Bad");

    g_assert_null(task_new_with_tags(1, "invalid", 5, tags, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_null(task_new_with_tags(1, "invalid", 5, tags, NULL));

    g_ptr_array_index(tags, 2) = "work";
    g_assert_null(task_new_with_tags(1, "duplicate", 5, tags, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
    g_clear_error(&error);
    g_assert_null(task_new_with_tags(1, "duplicate", 5, tags, NULL));

    g_assert_null(task_new_with_tags(1, "invalid priority", 0, tags, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);
    g_assert_null(task_new_with_tags(1, "invalid priority", 11, tags, NULL));
    g_assert_cmpstr(g_ptr_array_index(tags, 0), ==, "work");
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/task/new", test_task_new);
    g_test_add_func("/task/new-with-tags", test_task_new_with_tags);
    g_test_add_func("/task/new-with-tags/errors", test_task_new_with_tags_errors);
    g_test_add_func("/task/validation", test_task_validation);
    return g_test_run();
}
