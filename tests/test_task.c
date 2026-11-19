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

static void test_task_copy(void)
{
    gchar title[] = "Test Task";
    gchar *tag1 = "tag1";
    gchar *tag2 = "tag2";
    guint id = 1;
    gint priority = 5;

    Task *task = task_new(id, title, priority);
    task_add_tag(task, tag1);
    task_add_tag(task, tag2);

    Task *copy = task_copy(task);

    g_assert_nonnull(copy);
    g_assert_nonnull(copy->title);
    g_assert_nonnull(copy->tags);

    g_assert_cmpuint(copy->id, ==, id);
    g_assert_cmpstr(copy->title, ==, title);
    g_assert_cmpuint(copy->tags->len, ==, 2);
    g_assert_cmpint(copy->priority, ==, priority);
    g_assert_cmpint(copy->created_at, ==, task->created_at);

    g_assert_false(copy->title == task->title);
    g_assert_false(copy->tags->pdata[0] == task->tags->pdata[0]);

    task_free(task);
    task_free(copy);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/task/new", test_task_new);
    g_test_add_func("/task/copy", test_task_copy);
    return g_test_run();
}