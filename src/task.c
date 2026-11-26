#include "task.h"
#include "task_test.h"

#include <stddef.h>

struct _Task
{
    guint id;
    gchar *title;
    gint priority;
    GPtrArray *tags;
    gint64 created_at;
};

static gboolean validate_priority(gint priority, GError **error)
{
    if (priority < MIN_PRIORITY || priority > MAX_PRIORITY) {
        g_set_error(error, TASK_ERROR, TASK_ERROR_INVALID_PRIORITY, "priority must be between %d and %d, got %d", MIN_PRIORITY,
            MAX_PRIORITY, priority);
        return FALSE;
    }

    return TRUE;
}

static gboolean validate_tag(const gchar *tag, GError **error)
{
    if (tag == NULL || *tag == '\0') {
        g_set_error_literal(error, TASK_ERROR, TASK_ERROR_INVALID_TAG, "tag must not be empty");
        return FALSE;
    }

    for (const gchar *p = tag; *p != '\0'; ++p) {
        if (!(g_ascii_isdigit(*p) || g_ascii_islower(*p) || *p == '_')) {
            g_set_error_literal(
                error, TASK_ERROR, TASK_ERROR_INVALID_TAG, "tag must contain only lowercase ASCII letters, digits, or underscores");
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean validate_tag_and_duplication(const gchar *tag, GPtrArray *tags, GError **error)
{
    if (!validate_tag(tag, error)) {
        return FALSE;
    }

    if (g_ptr_array_find_with_equal_func(tags, tag, g_str_equal, NULL)) {
        g_set_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG, "duplicate tag: %s", tag);
        return FALSE;
    }

    return TRUE;
}

static gboolean validate_tags(const GPtrArray *tags, GError **error)
{
    if (tags == NULL) {
        return TRUE;
    }

    g_autoptr(GHashTable) seen_tags = g_hash_table_new(g_str_hash, g_str_equal);

    for (guint i = 0; i < tags->len; ++i) {
        const gchar *tag = g_ptr_array_index(tags, i);
        if (!validate_tag(tag, error)) {
            return FALSE;
        }

        if (!g_hash_table_add(seen_tags, (gpointer)tag)) {
            g_set_error(error, TASK_ERROR, TASK_ERROR_DUPLICATE_TAG, "duplicate tag: %s", tag);
            return FALSE;
        }
    }

    return TRUE;
}

GQuark task_error_quark(void) { return g_quark_from_static_string("task-error-quark"); }

Task *task_new(guint id, const gchar *title, gint priority, GError **error) { return task_new_with_tags(id, title, priority, NULL, error); }

Task *task_new_with_tags(guint id, const gchar *title, gint priority, const GPtrArray *tags, GError **error)
{
    if (!validate_priority(priority, error)) {
        return NULL;
    }

    if (!validate_tags(tags, error)) {
        return NULL;
    }

    Task *task = g_new(Task, 1);

    task->id = id;
    task->title = g_strdup(title);
    task->priority = priority;
    task->tags = g_ptr_array_new_with_free_func(g_free);
    task->created_at = g_get_monotonic_time();

    if (tags == NULL) {
        return task;
    }

    for (guint i = 0; i < tags->len; ++i) {
        g_ptr_array_add(task->tags, g_strdup(g_ptr_array_index(tags, i)));
    }

    return task;
}

void task_free(Task *task)
{
    g_free(task->title);
    g_ptr_array_free(task->tags, TRUE);
    g_free(task);
}

guint task_get_id(const Task *task) { return task->id; }

const gchar *task_get_title(const Task *task) { return task->title; }

gint task_get_priority(const Task *task) { return task->priority; }

gboolean task_set_priority(Task *task, gint priority, GError **error)
{
    if (!validate_priority(priority, error)) {
        return FALSE;
    }

    task->priority = priority;

    return TRUE;
}

guint task_get_tag_count(const Task *task) { return task->tags->len; }

const gchar *task_get_tag_at(const Task *task, guint index)
{
    if (index >= task->tags->len) {
        return NULL;
    }

    return g_ptr_array_index(task->tags, index);
}

gboolean task_add_tag(Task *task, const gchar *tag, GError **error)
{
    if (!validate_tag_and_duplication(tag, task->tags, error)) {
        return FALSE;
    }

    g_ptr_array_add(task->tags, g_strdup(tag));

    return TRUE;
}

gboolean task_remove_tag(Task *task, const gchar *tag, GError **error)
{
    if (!validate_tag(tag, error)) {
        return FALSE;
    }

    guint index;
    const gboolean found = g_ptr_array_find_with_equal_func(task->tags, tag, g_str_equal, &index);

    if (found) {
        g_ptr_array_remove_index(task->tags, index);
    }

    return TRUE;
}

gint task_compare(const Task *first, const Task *second)
{
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

#ifdef TASK_MANAGER_TESTING
void task_set_created_at_for_test(Task *task, gint64 created_at) { task->created_at = created_at; }
#endif
