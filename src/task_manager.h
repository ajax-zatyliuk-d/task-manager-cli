#pragma once

#include "task.h"

typedef struct _TaskManager TaskManager;
typedef struct _TaskManagerStats TaskManagerStats;
typedef enum _TaskManagerError TaskManagerError;

enum _TaskManagerError
{
    TM_ERROR_INVALID_PRIORITY,
    TM_ERROR_TASK_NOT_FOUND,
    TM_ERROR_INVALID_TAG,
    TM_ERROR_DUPLICATE_TAG,
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

guint task_manager_add_task(TaskManager *manager, const gchar *title, gint priority, GError **error);
const Task *task_manager_find_task_by_id(TaskManager *manager, guint id, GError **error);
gboolean task_manager_delete_task(TaskManager *manager, guint id, GError **error);

// Adds a non-empty tag containing only [a-z0-9_] and returns TRUE.
// Returns FALSE with TM_ERROR_TASK_NOT_FOUND, TM_ERROR_INVALID_TAG,
// or TM_ERROR_DUPLICATE_TAG. On failure, the task is unchanged.
gboolean task_manager_tag_task(TaskManager *manager, guint id, const gchar *tag, GError **error);

// returns a new array of borrowed Task pointers
// no matches: an empty array
// invalid tag: NULL and TM_ERROR_INVALID_TAG
// the caller releases the array with g_ptr_array_unref()
// task pointers remain valid until their tasks are deleted
GPtrArray *task_manager_find_tasks_by_tag(TaskManager *manager, const gchar *tag, GError **error);

// snake_case like tag and is not empty
// return TRUE if tag is valid and ID is existed and FALSE if get some error
// errors that can be added: TM_ERROR_TASK_NOT_FOUND or TM_ERROR_INVALID_TAG
gboolean task_manager_untag_task(TaskManager *manager, guint id, const gchar *tag, GError **error);

// caller is owner of new GPtrArray of tasks (Task *task)
// n == 0 return empty array
// n > length return all tasks
GPtrArray *task_manager_top_tasks(TaskManager *manager, guint n);

gboolean task_manager_enqueue_task(TaskManager *manager, guint id, GError **error);

// Removes the next task from the manager and transfers ownership to the caller.
// The caller must free the returned task with task_free().
// Returns NULL if the run queue is empty.
Task *task_manager_run_next(TaskManager *manager);

void task_manager_record_command(TaskManager *manager, const gchar *command);
GPtrArray *task_manager_get_history(TaskManager *manager);
TaskManagerStats task_manager_get_stats(TaskManager *manager);
gboolean task_manager_is_valid_tag(const gchar *tag, GError **error);
