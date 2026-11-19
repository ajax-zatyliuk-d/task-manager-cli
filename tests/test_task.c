#include "task.h"

static void test_task_new(void)
{
    gchar title[] = "Test Task";
    guint id = 1;
    gint priority = 5;

    Task *task = task_new(id, title, priority);
    g_assert_nonnull(task);
    g_assert_nonnull(task->title);
    g_assert_nonnull(task->tags);

    g_assert_cmpuint(task->id, ==, id);
    g_assert_cmpstr(task->title, ==, title);
    g_assert_cmpuint(task->tags->len, ==, 0);
    g_assert_cmpint(task->priority, ==, priority);

    title[0] = 'X';
    g_assert_cmpstr(task->title, ==, "Test Task");

    task_free(task);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/task/new", test_task_new);
    return g_test_run();
}