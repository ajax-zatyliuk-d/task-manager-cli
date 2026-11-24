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
};

struct _TaskManagerStats
{
    guint tasks;
    guint unique_tags;
    guint queue_length;
};

#define TM_ERROR (task_manager_error_quark())
GQuark task_manager_error_quark(void);

TaskManager *task_manager_new(void);
void task_manager_free(TaskManager *manager);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(TaskManager, task_manager_free);

/**
 * task_manager_add_task:
 * @manager: a valid, non-NULL task manager
 * @title: a non-NULL, NUL-terminated task title. The function copies it.
 * @priority: the task priority, from 1 to 10 inclusive
 * @error: return location for a #GError, or %NULL
 *
 * Adds a task and assigns it a unique ID within @manager.
 * If @error is provided, it must point to %NULL on entry.
 *
 * Returns: the new task ID on success, or 0 on failure. An invalid
 * priority sets %TM_ERROR_INVALID_PRIORITY. If no IDs remain,
 * sets %TM_ERROR_ID_EXHAUSTED. Neither error adds a task.
 */
guint task_manager_add_task(TaskManager *manager, const gchar *title, gint priority, GError **error);
// Adds a task with its initial tags. NULL or an empty array means no tags.
// Tags are borrowed strings without '#' and are copied on success.
// Returns the new task ID, or 0 on failure.
guint task_manager_add_task_with_tags(TaskManager *manager, const gchar *title, gint priority, const GPtrArray *tags, GError **error);
gboolean task_manager_remove_task(TaskManager *manager, guint id, GError **error);

// Adds a non-empty tag containing only [a-z0-9_] and returns TRUE.
// Returns FALSE with TM_ERROR_TASK_NOT_FOUND, TM_ERROR_INVALID_TAG,
// or TM_ERROR_DUPLICATE_TAG. On failure, the task is unchanged.
gboolean task_manager_tag_task(TaskManager *manager, guint id, const gchar *tag, GError **error);

// snake_case like tag and is not empty
// return TRUE if tag is valid and ID is existed and FALSE if get some error
// errors that can be added: TM_ERROR_TASK_NOT_FOUND or TM_ERROR_INVALID_TAG
gboolean task_manager_untag_task(TaskManager *manager, guint id, const gchar *tag, GError **error);

const Task *task_manager_find_task_by_id(const TaskManager *manager, guint id, GError **error);

// returns a new array of borrowed Task pointers
// no matches: an empty array
// invalid tag: NULL and TM_ERROR_INVALID_TAG
// the caller releases the array with g_ptr_array_unref()
// task pointers remain valid until their tasks are deleted
GPtrArray *task_manager_find_tasks_by_tag(const TaskManager *manager, const gchar *tag, GError **error);

// caller is owner of new GPtrArray of tasks (Task *task)
// n == 0 return empty array
// n > length return all tasks
GPtrArray *task_manager_top_tasks(const TaskManager *manager, guint n);

gboolean task_manager_enqueue_task(TaskManager *manager, guint id, GError **error);

// Removes the next task from the manager and transfers ownership to the caller.
// The caller must free the returned task with task_free().
// Returns NULL if the run queue is empty.
Task *task_manager_run_next(TaskManager *manager);

void task_manager_record_command(TaskManager *manager, const gchar *command);
GPtrArray *task_manager_get_history(const TaskManager *manager);

TaskManagerStats task_manager_get_stats(const TaskManager *manager);
