#include "task.h"

#include <glib.h>

#include <stddef.h>

static void test_task_new(void)
{
    gchar title[] = "Test Task";
    guint id = 1;
    gint priority = 5;

    Task *task = task_new(id, title, priority);
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

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/task/new", test_task_new);
    return g_test_run();
}
