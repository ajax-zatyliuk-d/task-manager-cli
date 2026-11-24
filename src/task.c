#include "task.h"

#include <stddef.h>

struct _Task
{
    guint id;
    gchar *title;
    gint priority;
    GPtrArray *tags;
    gint64 created_at;
};

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

guint task_get_id(const Task *task) { return task->id; }

const gchar *task_get_title(const Task *task) { return task->title; }

gint task_get_priority(const Task *task) { return task->priority; }

const gchar *task_get_tag(const Task *task, guint index)
{
    if (index >= task->tags->len) {
        return NULL;
    }

    return g_ptr_array_index(task->tags, index);
}

guint task_get_tag_count(const Task *task) { return task->tags->len; }

gboolean task_is_tag_existed(const Task *task, const gchar *tag)
{
    return g_ptr_array_find_with_equal_func(task->tags, tag, g_str_equal, NULL);
}

gint task_compare(const Task *first, const Task *second)
{
    if (first->priority > second->priority) {
        return -1;
    }
    if (first->priority < second->priority) {
        return 1;
    }

    if (first->created_at > second->created_at) {
        return 1;
    }
    if (first->created_at < second->created_at) {
        return -1;
    }

    if (first->id > second->id) {
        return 1;
    }
    if (first->id < second->id) {
        return -1;
    }

    return 0;
}

void task_add_tag(Task *task, const gchar *tag) { g_ptr_array_add(task->tags, g_strdup(tag)); }

void task_remove_tag(Task *task, const gchar *tag)
{
    guint index;

    if (!g_ptr_array_find_with_equal_func(task->tags, tag, g_str_equal, &index)) {
        return;
    }

    g_ptr_array_remove_index(task->tags, index);
}
