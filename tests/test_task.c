#include "task.h"
#include "task_test.h"

#include <glib.h>

#include <stddef.h>

static void test_task_new(void)
{
    gchar title[] = "Test Task";
    guint id = 1;
    gint priority = 5;

    g_autoptr(Task) task = task_new(id, title, priority, NULL);
    g_assert_nonnull(task);
    g_assert_nonnull(task_get_title(task));

    g_assert_cmpuint(task_get_id(task), ==, id);
    g_assert_cmpstr(task_get_title(task), ==, title);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);
    g_assert_cmpint(task_get_priority(task), ==, priority);

    title[0] = 'X';
    g_assert_cmpstr(task_get_title(task), ==, "Test Task");
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
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "work");
    g_assert_cmpstr(task_get_tag_at(task, 1), ==, "home_1");
}

static void test_task_new_with_null_tags(void)
{
    g_autoptr(GError) error = NULL;

    g_autoptr(Task) task = task_new_with_tags(1, "no tags", 5, NULL, &error);

    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);
}

static void test_task_new_with_invalid_tag(void)
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

    g_assert_cmpstr(g_ptr_array_index(tags, 0), ==, "work");
}

static void test_task_new_with_duplicate_tag_in_distinct_strings(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) tags = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(tags, g_strdup("work"));
    g_ptr_array_add(tags, g_strdup("home"));
    g_ptr_array_add(tags, g_strdup("work"));
    g_assert_true(g_ptr_array_index(tags, 0) != g_ptr_array_index(tags, 2));

    g_assert_null(task_new_with_tags(1, "duplicate", 5, tags, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
    g_assert_null(task_new_with_tags(1, "duplicate", 5, tags, NULL));

    g_assert_cmpuint(tags->len, ==, 3);
    g_assert_cmpstr(g_ptr_array_index(tags, 0), ==, "work");
    g_assert_cmpstr(g_ptr_array_index(tags, 1), ==, "home");
    g_assert_cmpstr(g_ptr_array_index(tags, 2), ==, "work");
}

static void test_task_new_with_tags_invalid_priority(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    g_ptr_array_add(tags, "work");
    g_assert_null(task_new_with_tags(1, "invalid priority", 0, tags, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);
    g_assert_null(task_new_with_tags(1, "invalid priority", 11, tags, NULL));
    g_assert_cmpstr(g_ptr_array_index(tags, 0), ==, "work");
}

static void test_task_new_with_tags_reports_first_error(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    g_ptr_array_add(tags, "work");
    g_ptr_array_add(tags, "work");
    g_ptr_array_add(tags, "Bad");

    g_assert_null(task_new_with_tags(1, "invalid", 0, tags, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);

    g_assert_null(task_new_with_tags(1, "invalid", 5, tags, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
}

static void test_task_new_invalid_priority(void)
{
    g_autoptr(GError) error = NULL;
    g_assert_null(task_new(1, "invalid", 0, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);
    g_assert_null(task_new(1, "invalid", 0, NULL));
    g_assert_null(task_new(1, "invalid", 11, &error));
    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);
}

static void test_task_new_maximum_priority(void)
{
    g_autoptr(GError) error = NULL;

    g_autoptr(Task) task = task_new(1, "maximum priority", 10, &error);

    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpint(task_get_priority(task), ==, 10);
}

static void test_task_set_priority(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_true(task_set_priority(task, 10, &error));
    g_assert_no_error(error);
    g_assert_cmpint(task_get_priority(task), ==, 10);
}

static void test_task_set_minimum_priority(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 10, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);

    g_assert_true(task_set_priority(task, 1, &error));

    g_assert_no_error(error);
    g_assert_cmpint(task_get_priority(task), ==, 1);
}

static void test_task_set_priority_below_minimum(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);

    g_assert_false(task_set_priority(task, 0, &error));

    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_assert_cmpint(task_get_priority(task), ==, 1);
}

static void test_task_set_priority_above_maximum(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 10, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);

    g_assert_false(task_set_priority(task, 11, &error));

    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY);
    g_assert_cmpint(task_get_priority(task), ==, 10);
}

static void test_task_add_invalid_tags(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    const gchar *invalid_tags[] = {NULL, "", "Bad", "a-b"};

    for (guint i = 0; i < G_N_ELEMENTS(invalid_tags); ++i) {
        g_test_message("Invalid tag: %s", invalid_tags[i] == NULL ? "(null)" : invalid_tags[i]);
        g_assert_false(task_add_tag(task, invalid_tags[i], &error));
        g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
        g_assert_cmpuint(task_get_tag_count(task), ==, 0);
        g_clear_error(&error);
    }
}

static void test_task_add_duplicate_tag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_true(task_add_tag(task, "work_1", &error));
    g_assert_no_error(error);

    g_assert_false(task_add_tag(task, "work_1", &error));

    g_assert_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "work_1");
}

static void test_task_remove_tag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_true(task_add_tag(task, "work_1", &error));
    g_assert_no_error(error);

    g_assert_true(task_remove_tag(task, "work_1", &error));

    g_assert_no_error(error);
    g_assert_cmpuint(task_get_tag_count(task), ==, 0);
}

static void test_task_remove_invalid_tag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_true(task_add_tag(task, "work_1", &error));
    g_assert_no_error(error);

    g_assert_false(task_remove_tag(task, "Bad", &error));

    g_assert_error(error, TASK_ERROR, TASK_ERROR_INVALID_TAG);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "work_1");
}

static void test_task_remove_missing_tag(void)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(Task) task = task_new(1, "valid", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_true(task_add_tag(task, "home", &error));
    g_assert_no_error(error);

    g_assert_true(task_remove_tag(task, "work_1", &error));

    g_assert_no_error(error);
    g_assert_cmpuint(task_get_tag_count(task), ==, 1);
    g_assert_cmpstr(task_get_tag_at(task, 0), ==, "home");
}

static void test_task_compare_created_at_before_id(void)
{
    g_autoptr(Task) older = task_new(2, "older", 5, NULL);
    g_autoptr(Task) newer = task_new(1, "newer", 5, NULL);
    g_assert_nonnull(older);
    g_assert_nonnull(newer);
    task_set_created_at_for_test(older, 100);
    task_set_created_at_for_test(newer, 200);

    g_assert_cmpint(task_compare(older, newer), <, 0);
    g_assert_cmpint(task_compare(newer, older), >, 0);
}

static void test_task_compare_id_when_created_at_equal(void)
{
    g_autoptr(Task) lower_id = task_new(1, "lower", 5, NULL);
    g_autoptr(Task) higher_id = task_new(2, "higher", 5, NULL);
    g_assert_nonnull(lower_id);
    g_assert_nonnull(higher_id);
    task_set_created_at_for_test(lower_id, 100);
    task_set_created_at_for_test(higher_id, 100);

    g_assert_cmpint(task_compare(lower_id, higher_id), <, 0);
    g_assert_cmpint(task_compare(higher_id, lower_id), >, 0);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/task/new/success", test_task_new);

    g_test_add_func("/task/new-with-tags/success", test_task_new_with_tags);
    g_test_add_func("/task/new-with-tags/null", test_task_new_with_null_tags);
    g_test_add_func("/task/new-with-tags/invalid-tag", test_task_new_with_invalid_tag);
    g_test_add_func("/task/new-with-tags/duplicate-distinct-strings", test_task_new_with_duplicate_tag_in_distinct_strings);
    g_test_add_func("/task/new-with-tags/invalid-priority", test_task_new_with_tags_invalid_priority);
    g_test_add_func("/task/new-with-tags/first-error", test_task_new_with_tags_reports_first_error);

    g_test_add_func("/task/priority/new-invalid", test_task_new_invalid_priority);
    g_test_add_func("/task/priority/new-maximum", test_task_new_maximum_priority);
    g_test_add_func("/task/priority/set", test_task_set_priority);
    g_test_add_func("/task/priority/set-minimum", test_task_set_minimum_priority);
    g_test_add_func("/task/priority/set-below-minimum", test_task_set_priority_below_minimum);
    g_test_add_func("/task/priority/set-above-maximum", test_task_set_priority_above_maximum);

    g_test_add_func("/task/tags/add-invalid", test_task_add_invalid_tags);
    g_test_add_func("/task/tags/add-duplicate", test_task_add_duplicate_tag);
    g_test_add_func("/task/tags/remove", test_task_remove_tag);
    g_test_add_func("/task/tags/remove-invalid", test_task_remove_invalid_tag);
    g_test_add_func("/task/tags/remove-missing", test_task_remove_missing_tag);

    g_test_add_func("/task/compare/created-at-before-id", test_task_compare_created_at_before_id);
    g_test_add_func("/task/compare/id-when-created-at-equal", test_task_compare_id_when_created_at_equal);
    return g_test_run();
}
