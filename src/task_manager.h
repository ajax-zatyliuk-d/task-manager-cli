#pragma once

#include "task.h"

#include <glib.h>

typedef struct _TaskManager TaskManager;
typedef struct _TaskManagerStats TaskManagerStats;
typedef enum _TaskManagerError TaskManagerError;

enum _TaskManagerError
{
    TM_ERROR_INVALID_PRIORITY,
    TM_ERROR_TASK_NOT_FOUND,
    TM_ERROR_INVALID_TAG,
    TM_ERROR_DUPLICATE_TAG,
    TM_ERROR_ID_EXHAUSTED,
    TM_ERROR_INVALID_ID,
    TM_ERROR_DUPLICATE_ID,
};

struct _TaskManagerStats
{
    guint tasks;
    guint unique_tags;
    guint queue_length;
};

#define TM_ERROR (task_manager_error_quark())

/**
 * task_manager_error_quark:
 *
 * Returns: the error domain for task manager operations
 */
GQuark task_manager_error_quark(void);

/**
 * task_manager_new:
 *
 * Returns: (transfer full): a new manager; free it with task_manager_free()
 */
TaskManager *task_manager_new(void);

/**
 * task_manager_free:
 * @manager: (transfer full) (nullable): a task manager
 *
 * Frees the manager and every task it still owns. Does nothing for %NULL.
 */
void task_manager_free(TaskManager *manager);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(TaskManager, task_manager_free);

/**
 * task_manager_add_task:
 * @manager: a task manager
 * @title: a non-NULL, NUL-terminated title to copy
 * @priority: task priority, from 1 to 10 inclusive
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Creates a task and assigns it a unique ID. If @error is provided, it must
 * point to %NULL. Failure leaves the manager unchanged.
 *
 * Returns: the new ID, or 0 on failure. If @error is provided, failure sets
 *   %TM_ERROR_INVALID_PRIORITY or %TM_ERROR_ID_EXHAUSTED.
 */
guint task_manager_add_task(TaskManager *manager, const gchar *title, gint priority, GError **error);

/**
 * task_manager_add_task_with_tags:
 * @manager: a task manager
 * @title: a non-NULL, NUL-terminated title to copy
 * @priority: task priority, from 1 to 10 inclusive
 * @tags: (nullable) (element-type utf8): borrowed tags without '#', or %NULL
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Creates a task with copies of its initial tags. An empty array adds no tags.
 * Tags must be non-empty and contain only [a-z0-9_], with no duplicates.
 * If @error is provided, it must point to %NULL. Failure leaves the manager
 * unchanged.
 *
 * Returns: the new ID, or 0 on failure. If @error is provided, failure sets
 *   %TM_ERROR_INVALID_TAG, %TM_ERROR_DUPLICATE_TAG,
 *   %TM_ERROR_INVALID_PRIORITY, or %TM_ERROR_ID_EXHAUSTED.
 */
guint task_manager_add_task_with_tags(TaskManager *manager, const gchar *title, gint priority, const GPtrArray *tags, GError **error);

/**
 * task_manager_take_task:
 * @manager: a task manager
 * @task: a caller-owned task
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Takes ownership on success. The caller must not modify or free the task
 * while it belongs to the manager. On failure, the caller retains ownership
 * and the manager is unchanged. IDs must be in 1..G_MAXUINT-1 and unique.
 * Automatic IDs skip IDs already in use. If @error is provided it must point
 * to %NULL.
 *
 * Returns: %TRUE on success, or %FALSE with %TM_ERROR_INVALID_ID or
 *   %TM_ERROR_DUPLICATE_ID if @error is provided
 */
gboolean task_manager_take_task(TaskManager *manager, Task *task, GError **error);

/**
 * task_manager_remove_task:
 * @manager: a task manager
 * @id: task identifier
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Removes and frees the task, including all its queue entries. If @error is
 * provided, it must point to %NULL. Failure leaves the manager unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on failure; sets
 *   %TM_ERROR_TASK_NOT_FOUND if @error is provided
 */
gboolean task_manager_remove_task(TaskManager *manager, guint id, GError **error);

/**
 * task_manager_tag_task:
 * @manager: a task manager
 * @id: task identifier
 * @tag: (nullable): tag without '#'; must be non-empty and contain only [a-z0-9_]
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Adds a copy of @tag. If @error is provided, it must point to %NULL. Failure
 * leaves the manager unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on failure; sets
 *   %TM_ERROR_INVALID_TAG, %TM_ERROR_TASK_NOT_FOUND, or
 *   %TM_ERROR_DUPLICATE_TAG if @error is provided
 */
gboolean task_manager_tag_task(TaskManager *manager, guint id, const gchar *tag, GError **error);

/**
 * task_manager_untag_task:
 * @manager: a task manager
 * @id: task identifier
 * @tag: (nullable): tag without '#'; must be non-empty and contain only [a-z0-9_]
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Removes @tag if present; an absent tag is a successful no-op. If @error is
 * provided, it must point to %NULL. Failure leaves the manager unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on failure; sets %TM_ERROR_INVALID_TAG
 *   or %TM_ERROR_TASK_NOT_FOUND if @error is provided
 */
gboolean task_manager_untag_task(TaskManager *manager, guint id, const gchar *tag, GError **error);

/**
 * task_manager_find_task_by_id:
 * @manager: a task manager
 * @id: task identifier
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * If @error is provided, it must point to %NULL. Failure leaves the manager
 * unchanged. The borrowed task is read-only and valid while owned by @manager,
 * until removal, transfer by task_manager_run_next(), or manager destruction.
 *
 * Returns: (transfer none) (nullable): the task, or %NULL on failure; sets
 *   %TM_ERROR_TASK_NOT_FOUND if @error is provided
 */
const Task *task_manager_find_task_by_id(const TaskManager *manager, guint id, GError **error);

/**
 * task_manager_find_tasks_by_tag:
 * @manager: a task manager
 * @tag: (nullable): tag without '#'; must be non-empty and contain only [a-z0-9_]
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * The array is a snapshot of borrowed, read-only task pointers; it is not
 * updated when a task is removed. Each pointer is valid while @manager owns
 * that task. If @error is provided, it must point to %NULL.
 *
 * Returns: (transfer container) (element-type Task) (nullable): a new array,
 *   empty if no tasks match, or %NULL for an invalid tag, setting
 *   %TM_ERROR_INVALID_TAG if @error is provided. Release a returned array
 *   with g_ptr_array_unref(), without freeing its elements.
 */
GPtrArray *task_manager_find_tasks_by_tag(const TaskManager *manager, const gchar *tag, GError **error);

/**
 * task_manager_top_tasks:
 * @manager: a task manager
 * @n: maximum number of tasks to return
 *
 * Returns tasks in task_compare() order. The array is a snapshot of borrowed,
 * read-only task pointers, each valid while @manager owns the task.
 *
 * Returns: (transfer container) (element-type Task): a new array of up to @n
 *   tasks; release it with g_ptr_array_unref(), without freeing its elements
 */
GPtrArray *task_manager_top_tasks(const TaskManager *manager, guint n);

/**
 * task_manager_enqueue_task:
 * @manager: a task manager
 * @id: task identifier
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Appends the task to the FIFO queue. Repeated entries are allowed. If @error
 * is provided, it must point to %NULL. Failure leaves the manager unchanged.
 *
 * Returns: %TRUE on success, or %FALSE on failure; sets
 *   %TM_ERROR_TASK_NOT_FOUND if @error is provided
 */
gboolean task_manager_enqueue_task(TaskManager *manager, guint id, GError **error);

/**
 * task_manager_run_next:
 * @manager: a task manager
 *
 * Removes the next queued task from the manager and transfers its ownership.
 * All other queue entries for the same task are removed as well.
 *
 * Returns: (transfer full) (nullable): the original task, to free with
 *   task_free(), or %NULL if the queue is empty
 */
Task *task_manager_run_next(TaskManager *manager);

/**
 * task_manager_record_command:
 * @manager: a task manager
 * @command: (nullable): command to copy
 *
 * Records a copy, evicting the oldest entry after ten commands. Ignores %NULL
 * and empty strings.
 */
void task_manager_record_command(TaskManager *manager, const gchar *command);

/**
 * task_manager_get_history:
 * @manager: a task manager
 *
 * Returns: (transfer full) (element-type utf8): a new array of copied strings,
 *   oldest first; release the array and strings with g_ptr_array_unref()
 */
GPtrArray *task_manager_get_history(const TaskManager *manager);

/**
 * task_manager_get_stats:
 * @manager: a task manager
 *
 * Returns: counts by value; queue_length includes repeated entries
 */
TaskManagerStats task_manager_get_stats(const TaskManager *manager);
