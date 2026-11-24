#include "task_manager.h"
#include "task_manager_test.h"

#include <stddef.h>

#define CMD_HISTORY_LEN 10

struct _TaskManager
{
    GHashTable *tasks_by_id;
    GHashTable *tasks_by_tag;
    GHashTable *unique_tags;
    GSequence *tasks_sorted;
    GQueue *run_queue;
    GQueue *command_history;
    guint next_id;
};

static void task_destroy(gpointer task) { task_free(task); }

static void tasks_array_destroy(gpointer ptr) { g_ptr_array_free(ptr, TRUE); }

static Task *task_manager_task_lookup(GHashTable *tasks_by_id, guint id, GError **error)
{
    Task *task = g_hash_table_lookup(tasks_by_id, GUINT_TO_POINTER(id));

    if (task == NULL) {
        g_set_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND, "task %u not found", id);
    }

    return task;
}

static gboolean validate_tag(const gchar *tag, GError **error)
{
    if (tag == NULL || *tag == '\0') {
        g_set_error_literal(error, TM_ERROR, TM_ERROR_INVALID_TAG, "tag must not be empty");

        return FALSE;
    }

    for (const gchar *p = tag; *p != '\0'; ++p) {
        const gchar c = *p;

        if (!(g_ascii_isdigit(c) || g_ascii_islower(c) || c == '_')) {
            g_set_error_literal(
                error, TM_ERROR, TM_ERROR_INVALID_TAG, "tag must contain only lowercase ASCII letters, digits, or underscores");

            return FALSE;
        }
    }

    return TRUE;
}

static void task_manager_tasks_by_tag_add(GHashTable *tasks_by_tag, Task *task, const gchar *tag)
{
    GPtrArray *tasks = g_hash_table_lookup(tasks_by_tag, tag);

    if (tasks == NULL) {
        tasks = g_ptr_array_new();
        g_hash_table_insert(tasks_by_tag, g_strdup(tag), tasks);
    }

    g_ptr_array_add(tasks, task);
}

static void task_manager_unique_tags_add(GHashTable *unique_tags, const gchar *tag)
{
    if (g_hash_table_contains(unique_tags, tag)) {
        return;
    }

    g_hash_table_add(unique_tags, g_strdup(tag));
}

static void task_manager_remove_task_from_tag(TaskManager *manager, const Task *task, const gchar *tag)
{
    GPtrArray *tasks = g_hash_table_lookup(manager->tasks_by_tag, tag);
    g_assert_nonnull(tasks);

    guint index = 0;
    const gboolean found = g_ptr_array_find(tasks, task, &index);
    g_assert_true(found);

    g_ptr_array_remove_index(tasks, index);

    if (tasks->len == 0) {
        g_hash_table_remove(manager->tasks_by_tag, tag);
        g_hash_table_remove(manager->unique_tags, tag);
    }
}

static gint compare_tasks(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;  // unused data
    const Task *first = a;
    const Task *second = b;

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

static void task_manager_unindex_task(TaskManager *manager, Task *task)
{
    for (guint i = 0; i < task->tags->len; ++i) {
        const gchar *tag = g_ptr_array_index(task->tags, i);
        task_manager_remove_task_from_tag(manager, task, tag);
    }

    GSequenceIter *it = g_sequence_lookup(manager->tasks_sorted, task, compare_tasks, NULL);
    g_assert_nonnull(it);
    g_sequence_remove(it);

    g_queue_remove_all(manager->run_queue, task);
}

GQuark task_manager_error_quark(void) { return g_quark_from_static_string("task-manager-error-quark"); }

TaskManager *task_manager_new(void)
{
    TaskManager *manager = g_new(TaskManager, 1);
    manager->tasks_by_id = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, task_destroy);
    manager->tasks_by_tag = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, tasks_array_destroy);
    manager->unique_tags = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    manager->tasks_sorted = g_sequence_new(NULL);
    manager->run_queue = g_queue_new();
    manager->command_history = g_queue_new();
    manager->next_id = 1;

    return manager;
}

void task_manager_free(TaskManager *manager)
{
    if (manager == NULL) {
        return;
    }

    g_hash_table_destroy(manager->unique_tags);
    g_hash_table_destroy(manager->tasks_by_tag);
    g_sequence_free(manager->tasks_sorted);
    g_hash_table_destroy(manager->tasks_by_id);
    g_queue_free(manager->run_queue);
    g_queue_free_full(manager->command_history, g_free);

    g_free(manager);
}

guint task_manager_add_task(TaskManager *manager, const gchar *title, gint priority, GError **error)
{
    if (priority > 10 || priority < 1) {
        g_set_error(error, TM_ERROR, TM_ERROR_INVALID_PRIORITY, "priority must be between 1 and 10, got %d", priority);
        return 0;
    }

    const guint task_id = manager->next_id;
    if (task_id == 0 || task_id == G_MAXUINT) {
        g_set_error_literal(error, TM_ERROR, TM_ERROR_ID_EXHAUSTED, "no more task IDs available");
        return 0;
    }

    Task *task = task_new(task_id, title, priority);
    g_hash_table_insert(manager->tasks_by_id, GUINT_TO_POINTER(task_id), task);
    g_sequence_insert_sorted(manager->tasks_sorted, task, compare_tasks, NULL);

    manager->next_id++;

    return task->id;
}

gboolean task_manager_remove_task(TaskManager *manager, guint id, GError **error)
{
    Task *task = task_manager_task_lookup(manager->tasks_by_id, id, error);
    if (task == NULL) {
        return FALSE;
    }

    task_manager_unindex_task(manager, task);

    // free task original memory
    g_hash_table_remove(manager->tasks_by_id, GUINT_TO_POINTER(task->id));

    return TRUE;
}

gboolean task_manager_tag_task(TaskManager *manager, guint id, const gchar *tag, GError **error)
{
    if (!validate_tag(tag, error)) {
        return FALSE;
    }

    Task *task = task_manager_task_lookup(manager->tasks_by_id, id, error);
    if (task == NULL) {
        return FALSE;
    }

    if (task_is_tag_existed(task, tag)) {
        g_set_error(error, TM_ERROR, TM_ERROR_DUPLICATE_TAG, "task %u already has tag %s", id, tag);
        return FALSE;
    }

    task_add_tag(task, tag);
    task_manager_tasks_by_tag_add(manager->tasks_by_tag, task, tag);
    task_manager_unique_tags_add(manager->unique_tags, tag);

    return TRUE;
}

gboolean task_manager_untag_task(TaskManager *manager, guint id, const gchar *tag, GError **error)
{
    if (!validate_tag(tag, error)) {
        return FALSE;
    }

    Task *task = task_manager_task_lookup(manager->tasks_by_id, id, error);
    if (task == NULL) {
        return FALSE;
    }

    if (!task_is_tag_existed(task, tag)) {
        return TRUE;
    }

    task_manager_remove_task_from_tag(manager, task, tag);
    task_remove_tag(task, tag);

    return TRUE;
}

gboolean task_manager_is_valid_tag(const gchar *tag, GError **error) { return validate_tag(tag, error); }

const Task *task_manager_find_task_by_id(const TaskManager *manager, guint id, GError **error)
{
    return task_manager_task_lookup(manager->tasks_by_id, id, error);
}

GPtrArray *task_manager_find_tasks_by_tag(const TaskManager *manager, const gchar *tag, GError **error)
{
    if (!validate_tag(tag, error)) {
        return NULL;
    }

    GPtrArray *matches = g_ptr_array_new();
    const GPtrArray *tasks = g_hash_table_lookup(manager->tasks_by_tag, tag);

    if (tasks == NULL) {
        return matches;
    }

    for (guint i = 0; i < tasks->len; ++i) {
        g_ptr_array_add(matches, g_ptr_array_index(tasks, i));
    }

    return matches;
}

GPtrArray *task_manager_top_tasks(const TaskManager *manager, guint n)
{
    GPtrArray *tasks = g_ptr_array_new();
    for (GSequenceIter *it = g_sequence_get_begin_iter(manager->tasks_sorted); tasks->len < n && !g_sequence_iter_is_end(it);
         it = g_sequence_iter_next(it)) {
        g_ptr_array_add(tasks, g_sequence_get(it));
    }

    return tasks;
}

gboolean task_manager_enqueue_task(TaskManager *manager, guint id, GError **error)
{
    Task *task = task_manager_task_lookup(manager->tasks_by_id, id, error);
    if (task == NULL) {
        return FALSE;
    }

    g_queue_push_tail(manager->run_queue, task);
    return TRUE;
}

Task *task_manager_run_next(TaskManager *manager)
{
    if (g_queue_is_empty(manager->run_queue)) {
        return NULL;
    }

    Task *task = g_queue_pop_head(manager->run_queue);
    task_manager_unindex_task(manager, task);

    // steal owning of *task
    g_hash_table_steal(manager->tasks_by_id, GUINT_TO_POINTER(task->id));

    return task;
}

void task_manager_record_command(TaskManager *manager, const gchar *command)
{
    if (command == NULL || command[0] == '\0') {
        return;
    }

    g_queue_push_tail(manager->command_history, g_strdup(command));

    if (g_queue_get_length(manager->command_history) <= CMD_HISTORY_LEN) {
        return;
    }

    gchar *old_cmd = g_queue_pop_head(manager->command_history);
    g_free(old_cmd);
}

GPtrArray *task_manager_get_history(const TaskManager *manager)
{
    GPtrArray *history = g_ptr_array_new_with_free_func(g_free);

    for (const GList *node = manager->command_history->head; node != NULL; node = node->next) {
        const gchar *cmd = node->data;
        g_ptr_array_add(history, g_strdup(cmd));
    }

    return history;
}

TaskManagerStats task_manager_get_stats(const TaskManager *manager)
{
    const TaskManagerStats stats = {
        .tasks = g_hash_table_size(manager->tasks_by_id),
        .unique_tags = g_hash_table_size(manager->unique_tags),
        .queue_length = g_queue_get_length(manager->run_queue),
    };

    return stats;
}

#ifdef TASK_MANAGER_TESTING
void task_manager_set_next_id_for_test(TaskManager *manager, guint next_id) { manager->next_id = next_id; }
#endif
