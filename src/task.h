#pragma once

#include <glib.h>

typedef struct _Task Task;

/**
 * task_new:
 * @id: task identifier, stored without validation
 * @title: (nullable): title to copy
 * @priority: priority, stored without validation
 *
 * Creates an independent task with no tags. The caller owns the task.
 *
 * Returns: (transfer full): a new task; free it with task_free()
 */
Task *task_new(guint id, const gchar *title, gint priority);

/**
 * task_free:
 * @task: (transfer full): a non-NULL task
 *
 * Frees the task and its owned title and tags.
 */
void task_free(Task *task);

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
 * @task: a task
 * @tag: a non-NULL, NUL-terminated tag to copy
 *
 * Adds a copy without validating the tag or checking for duplicates.
 */
void task_add_tag(Task *task, const gchar *tag);

/**
 * task_remove_tag:
 * @task: a task
 * @tag: a non-NULL, NUL-terminated tag
 *
 * Removes the first matching tag. Does nothing if the tag is absent.
 */
void task_remove_tag(Task *task, const gchar *tag);
