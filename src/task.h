#pragma once

#include <glib.h>

#define MIN_PRIORITY 1
#define MAX_PRIORITY 10

typedef struct _Task Task;
typedef enum
{
    TASK_ERROR_INVALID_PRIORITY,
    TASK_ERROR_INVALID_TAG,
    TASK_ERROR_DUPLICATE_TAG,
    TASK_ERROR_TAG_NOT_FOUND
} TaskError;

#define TASK_ERROR (task_error_quark())
GQuark task_error_quark(void);

Task *task_new(guint id, const gchar *title, gint priority, GError **error);
Task *task_new_with_tags(guint id, const gchar *title, gint priority, const GPtrArray *tags, GError **error);
void task_free(Task *task);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Task, task_free);

guint task_get_id(const Task *task);
const gchar *task_get_title(const Task *task);
gint task_get_priority(const Task *task);
gboolean task_set_priority(Task *task, gint priority, GError **error);
guint task_get_tag_count(const Task *task);
const gchar *task_get_tag_at(const Task *task, guint index);
gboolean task_add_tag(Task *task, const gchar *tag, GError **error);
gboolean task_remove_tag(Task *task, const gchar *tag, GError **error);
gint task_compare(const Task *first, const Task *second);
