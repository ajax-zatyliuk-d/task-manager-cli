#pragma once

#include <glib.h>

typedef struct _Task Task;
struct _Task
{
    guint id;
    gchar *title;
    gint priority;
    GPtrArray *tags;
    gint64 created_at;
};

Task *task_new(guint id, const gchar *title, gint priority);
void task_free(Task *task);
gboolean task_is_tag_existed(Task *task, const gchar *tag);
void task_tags_add(Task *task, const gchar *tag);
void task_remove_tag(Task *task, const gchar *tag);
Task *task_copy(const Task *task);
