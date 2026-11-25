#pragma once

#include <glib.h>

typedef struct _Task Task;
typedef enum
{
    TASK_ERROR_INVALID_PRIORITY,
    TASK_ERROR_INVALID_TAG,
    TASK_ERROR_DUPLICATE_TAG
} TaskError;

#define TASK_ERROR (task_error_quark())
/**
 * task_error_quark:
 *
 * Returns: the error domain for task validation
 */
GQuark task_error_quark(void);

/**
 * task_new:
 * @id: task identifier, stored without validation
 * @title: (nullable): title to copy
 * @priority: priority from 1 to 10
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Creates an independent task with no tags. The caller owns the task.
 *
 * Returns: (transfer full) (nullable): a new task, or %NULL with
 *   %TASK_ERROR_INVALID_PRIORITY if @error is provided
 */
Task *task_new(guint id, const gchar *title, gint priority, GError **error);

/**
 * task_new_with_tags:
 * @id: task identifier, stored without validation
 * @title: (nullable): title to copy
 * @priority: priority from 1 to 10
 * @tags: (nullable) (element-type utf8) (transfer none): borrowed tags without
 *   '#', or %NULL; the array and its strings remain owned by the caller
 * @error: (nullable): return location for a #GError, or %NULL; if provided,
 *   must point to %NULL
 *
 * Creates an independent task with copies of its initial tags. An empty array
 * adds no tags. Tags must be non-empty, contain only [a-z0-9_], and be unique.
 * All fields are validated before the task is created.
 *
 * Returns: (transfer full) (nullable): a task owned by the caller, or %NULL
 *   with %TASK_ERROR_INVALID_PRIORITY, %TASK_ERROR_INVALID_TAG, or
 *   %TASK_ERROR_DUPLICATE_TAG if @error is provided
 */
Task *task_new_with_tags(guint id, const gchar *title, gint priority, const GPtrArray *tags, GError **error);

/**
 * task_free:
 * @task: (transfer full): a non-NULL task
 *
 * Frees the task and its owned title and tags.
 */
void task_free(Task *task);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Task, task_free);

/**
 * task_get_id:
 * @task: a task
 *
 * Returns: the task identifier
 */
guint task_get_id(const Task *task);

/**
 * task_get_title:
 * @task: a task
 *
 * Returns: (transfer none) (nullable): the task-owned title, valid until the
 *   task is freed; do not modify or free it
 */
const gchar *task_get_title(const Task *task);

/**
 * task_get_priority:
 * @task: a task
 *
 * Returns: the task priority
 */
gint task_get_priority(const Task *task);

/**
 * task_set_priority:
 * @task: an independently owned task
 * @priority: priority from 1 to 10
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Returns: %TRUE on success; otherwise %FALSE, leaving the task unchanged and
 *   setting %TASK_ERROR_INVALID_PRIORITY if @error is provided
 */
gboolean task_set_priority(Task *task, gint priority, GError **error);

/**
 * task_validate_tag:
 * @tag: (nullable): tag to validate
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Returns: %TRUE for a non-empty tag containing only [a-z0-9_]; otherwise
 *   %FALSE, setting %TASK_ERROR_INVALID_TAG if @error is provided
 */
gboolean task_validate_tag(const gchar *tag, GError **error);

/**
 * task_get_tag:
 * @task: a task
 * @index: zero-based tag index
 *
 * Returns: (transfer none) (nullable): a task-owned tag, valid until it is
 *   removed or the task is freed; %NULL if @index is out of range
 */
const gchar *task_get_tag(const Task *task, guint index);

/**
 * task_get_tag_count:
 * @task: a task
 *
 * Returns: the number of tags
 */
guint task_get_tag_count(const Task *task);

/**
 * task_has_tag:
 * @task: a task
 * @tag: a non-NULL, NUL-terminated tag
 *
 * Returns: %TRUE if the task contains @tag, otherwise %FALSE
 */
gboolean task_has_tag(const Task *task, const gchar *tag);

/**
 * task_compare:
 * @first: a task
 * @second: another task
 *
 * Compares descending priority, then ascending creation time and identifier.
 *
 * Returns: a negative value, zero, or a positive value if @first sorts before,
 *   equal to, or after @second, respectively
 */
gint task_compare(const Task *first, const Task *second);

/**
 * task_add_tag:
 * @task: a task owned by the caller
 * @tag: (nullable): tag to copy
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Adds a valid tag if absent. The task is unchanged on failure.
 *
 * Returns: %TRUE on success; %FALSE with %TASK_ERROR_INVALID_TAG or
 *   %TASK_ERROR_DUPLICATE_TAG if @error is provided
 */
gboolean task_add_tag(Task *task, const gchar *tag, GError **error);

/**
 * task_remove_tag:
 * @task: a task owned by the caller
 * @tag: (nullable): tag to remove
 * @removed: (out) (optional): whether the tag existed
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Removes a valid tag, if present; leaves the task unchanged on failure.
 *
 * Returns: %TRUE on success, or %FALSE with %TASK_ERROR_INVALID_TAG
 *   if @error is provided
 */
gboolean task_remove_tag(Task *task, const gchar *tag, gboolean *removed, GError **error);
