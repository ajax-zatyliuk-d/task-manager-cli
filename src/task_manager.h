#pragma once

#include "task.h"

#include <glib.h>

#define CMD_HISTORY_LEN 10

typedef struct _TaskManager TaskManager;
typedef struct _TaskManagerStats TaskManagerStats;
typedef enum _TaskManagerError TaskManagerError;

enum _TaskManagerError
{
    TM_ERROR_TASK_NOT_FOUND,
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

guint task_manager_add_task(TaskManager *manager, const gchar *title, gint priority, GError **error);
guint task_manager_add_task_with_tags(TaskManager *manager, const gchar *title, gint priority, const GPtrArray *tags, GError **error);
gboolean task_manager_remove_task(TaskManager *manager, guint id, GError **error);
gboolean task_manager_tag_task(TaskManager *manager, guint id, const gchar *tag, GError **error);
gboolean task_manager_untag_task(TaskManager *manager, guint id, const gchar *tag, GError **error);
const Task *task_manager_find_task_by_id(const TaskManager *manager, guint id, GError **error);
GPtrArray *task_manager_find_tasks_by_tag(const TaskManager *manager, const gchar *tag);
GPtrArray *task_manager_top_tasks(const TaskManager *manager, guint n);
gboolean task_manager_enqueue_task(TaskManager *manager, guint id, GError **error);
Task *task_manager_extract_next_task(TaskManager *manager);
void task_manager_record_command(TaskManager *manager, const gchar *command);
GPtrArray *task_manager_get_history(const TaskManager *manager);
TaskManagerStats task_manager_get_stats(const TaskManager *manager);
