#include "task.h"

Task *task_new(guint id, const gchar *title, gint priority)
{
    Task *task = g_new(Task, 1);

    task->id = id;
    task->title = g_strdup(title);
    task->priority = priority;
    task->tags = g_ptr_array_new_with_free_func(g_free);
    task->created_at = g_get_monotonic_time();

    return task;
}

void task_free(Task *task)
{
    g_free(task->title);
    g_ptr_array_free(task->tags, TRUE);
    g_free(task);
}

gboolean task_is_tag_existed(Task *task, const gchar *tag) { return g_ptr_array_find_with_equal_func(task->tags, tag, g_str_equal, NULL); }

void task_add_tag(Task *task, const gchar *tag) { g_ptr_array_add(task->tags, g_strdup(tag)); }

void task_remove_tag(Task *task, const gchar *tag)
{
    guint index;

    if (!g_ptr_array_find_with_equal_func(task->tags, tag, g_str_equal, &index)) {
        return;
    }

    g_ptr_array_remove_index(task->tags, index);
}
