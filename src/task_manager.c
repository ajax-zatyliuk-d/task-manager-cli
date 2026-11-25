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

static void destroy_task(gpointer task) { task_free(task); }

static void destroy_tasks_array(gpointer ptr) { g_ptr_array_free(ptr, TRUE); }

static Task *lookup_task_or_error(GHashTable *tasks_by_id, guint id, GError **error)
{
    Task *task = g_hash_table_lookup(tasks_by_id, GUINT_TO_POINTER(id));

    if (task == NULL) {
        g_set_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND, "task %u not found", id);
    }

    return task;
}

static void index_task_tag(TaskManager *manager, Task *task, const gchar *tag)
{
    GPtrArray *tag_tasks = g_hash_table_lookup(manager->tasks_by_tag, tag);

    if (tag_tasks == NULL) {
        tag_tasks = g_ptr_array_new();
        g_hash_table_insert(manager->tasks_by_tag, g_strdup(tag), tag_tasks);
    }

    g_ptr_array_add(tag_tasks, task);
    if (!g_hash_table_contains(manager->unique_tags, tag)) {
        g_hash_table_add(manager->unique_tags, g_strdup(tag));
    }
}

static void unindex_task_tag(TaskManager *manager, const Task *task, const gchar *tag)
{
    GPtrArray *tag_tasks = g_hash_table_lookup(manager->tasks_by_tag, tag);
    g_assert_nonnull(tag_tasks);

    guint index = 0;
    const gboolean found = g_ptr_array_find(tag_tasks, task, &index);
    g_assert_true(found);

    g_ptr_array_remove_index(tag_tasks, index);

    if (tag_tasks->len == 0) {
        g_hash_table_remove(manager->tasks_by_tag, tag);
        g_hash_table_remove(manager->unique_tags, tag);
    }
}

static gint compare_tasks(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    return task_compare(a, b);
}

static void index_task(TaskManager *manager, Task *task)
{
    g_sequence_insert_sorted(manager->tasks_sorted, task, compare_tasks, NULL);

    for (guint i = 0; i < task_get_tag_count(task); ++i) {
        const gchar *tag = task_get_tag(task, i);
        index_task_tag(manager, task, tag);
    }
}

static void unindex_task(TaskManager *manager, Task *task)
{
    const guint tag_count = task_get_tag_count(task);
    for (guint i = 0; i < tag_count; ++i) {
        unindex_task_tag(manager, task, task_get_tag(task, i));
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
    manager->tasks_by_id = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, destroy_task);
    manager->tasks_by_tag = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, destroy_tasks_array);
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
    return task_manager_add_task_with_tags(manager, title, priority, NULL, error);
}

guint task_manager_add_task_with_tags(TaskManager *manager, const gchar *title, gint priority, const GPtrArray *tags, GError **error)
{
    const guint id = manager->next_id;
    if (id == 0 || id == G_MAXUINT) {
        g_set_error_literal(error, TM_ERROR, TM_ERROR_ID_EXHAUSTED, "no more task IDs available");
        return 0;
    }

    Task *task = task_new_with_tags(id, title, priority, tags, error);
    if (task == NULL) {
        return 0;
    }
    g_hash_table_insert(manager->tasks_by_id, GUINT_TO_POINTER(id), task);
    index_task(manager, task);

    manager->next_id = id + 1;
    return id;
}

gboolean task_manager_remove_task(TaskManager *manager, guint id, GError **error)
{
    Task *task = lookup_task_or_error(manager->tasks_by_id, id, error);
    if (task == NULL) {
        return FALSE;
    }

    unindex_task(manager, task);

    // Removing from the owning table frees the task after its references are removed.
    g_hash_table_remove(manager->tasks_by_id, GUINT_TO_POINTER(task_get_id(task)));

    return TRUE;
}

gboolean task_manager_tag_task(TaskManager *manager, guint id, const gchar *tag, GError **error)
{
    Task *task = lookup_task_or_error(manager->tasks_by_id, id, error);
    if (task == NULL) {
        return FALSE;
    }
    if (!task_add_tag(task, tag, error)) {
        return FALSE;
    }
    index_task_tag(manager, task, tag);

    return TRUE;
}

gboolean task_manager_untag_task(TaskManager *manager, guint id, const gchar *tag, GError **error)
{
    Task *task = lookup_task_or_error(manager->tasks_by_id, id, error);
    if (task == NULL) {
        return FALSE;
    }
    g_autofree gchar *tag_copy = g_strdup(tag);
    gboolean removed = FALSE;
    if (!task_remove_tag(task, tag_copy, &removed, error)) {
        return FALSE;
    }
    if (removed) {
        unindex_task_tag(manager, task, tag_copy);
    }

    return TRUE;
}

const Task *task_manager_find_task_by_id(const TaskManager *manager, guint id, GError **error)
{
    return lookup_task_or_error(manager->tasks_by_id, id, error);
}

GPtrArray *task_manager_find_tasks_by_tag(const TaskManager *manager, const gchar *tag, GError **error)
{
    if (!task_validate_tag(tag, error)) {
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
    Task *task = lookup_task_or_error(manager->tasks_by_id, id, error);
    if (task == NULL) {
        return FALSE;
    }

    g_queue_push_tail(manager->run_queue, task);
    return TRUE;
}

Task *task_manager_extract_next_task(TaskManager *manager)
{
    if (g_queue_is_empty(manager->run_queue)) {
        return NULL;
    }

    Task *task = g_queue_pop_head(manager->run_queue);
    unindex_task(manager, task);

    // Transfer ownership to the caller without freeing the task.
    g_hash_table_steal(manager->tasks_by_id, GUINT_TO_POINTER(task_get_id(task)));

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
