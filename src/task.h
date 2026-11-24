#pragma once

#include <glib.h>

typedef struct _Task Task;

Task *task_new(guint id, const gchar *title, gint priority);
void task_free(Task *task);

guint task_get_id(const Task *task);
const gchar *task_get_title(const Task *task);
gint task_get_priority(const Task *task);

const gchar *task_get_tag(const Task *task, guint index);
guint task_get_tag_count(const Task *task);

gboolean task_is_tag_existed(const Task *task, const gchar *tag);
gint task_compare(const Task *first, const Task *second);

void task_add_tag(Task *task, const gchar *tag);
void task_remove_tag(Task *task, const gchar *tag);
