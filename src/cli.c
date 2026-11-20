#include "cli.h"
#include "task.h"

#include <stddef.h>

typedef struct _CliCommand CliCommand;
typedef gboolean (*CliHandler)(TaskManager *, gchar *const *, GString *, GError **);

struct _CliCommand
{
    const gchar *name;
    CliHandler handler;
};

static gboolean parse_id(const gchar *word, guint *id, GError **error)
{
    guint64 value;

    if (!g_ascii_string_to_unsigned(word, 10, 1, G_MAXUINT, &value, NULL)) {
        g_set_error(error, CLI_ERROR, CLI_ERROR_USAGE, "invalid id %s", word);
        return FALSE;
    }

    *id = (guint)value;
    return TRUE;
}

static gboolean validate_add_tags(gchar *const *first_tag, GError **error)
{
    if (*first_tag == NULL) {
        return TRUE;
    }

    g_autoptr(GHashTable) seen = g_hash_table_new(g_str_hash, g_str_equal);
    for (gchar *const *word = first_tag; *word != NULL; ++word) {
        if ((*word)[0] != '#') {
            g_set_error(error, CLI_ERROR, CLI_ERROR_USAGE, "expected a tag, got: %s", *word);
            return FALSE;
        }

        const gchar *tag = *word + 1;
        if (!task_manager_is_valid_tag(tag, error)) {
            return FALSE;
        }

        if (!g_hash_table_add(seen, *word)) {
            g_set_error(error, TM_ERROR, TM_ERROR_DUPLICATE_TAG, "duplicate tag: %s", tag);
            return FALSE;
        }
    }

    return TRUE;
}

// add <priority> <title> [#tag1 #tag2 ...]
static gboolean cmd_add(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    // validate args
    gint64 priority;
    if (args[0] == NULL || !g_ascii_string_to_signed(args[0], 10, G_MININT, G_MAXINT, &priority, NULL)) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: add <priority> <title> [#tag ...]");
        return FALSE;
    }

    gchar *const *const title_words = args + 1;
    gchar *const *first_tag = title_words;

    while (*first_tag != NULL && (*first_tag)[0] != '#') {
        ++first_tag;
    }

    if (first_tag == title_words) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "missing task title");
        return FALSE;
    }

    if (!validate_add_tags(first_tag, error)) {
        return FALSE;
    }

    GString *title = g_string_new(NULL);

    for (gchar *const *word = title_words; word != first_tag; ++word) {
        if (word != title_words) {
            g_string_append_c(title, ' ');
        }
        g_string_append(title, *word);
    }

    // add task
    const guint task_id = task_manager_add_task(manager, title->str, (gint)priority, error);
    g_string_free(title, TRUE);

    if (task_id == 0) {
        return FALSE;
    }

    for (gchar *const *tag = first_tag; *tag != NULL; ++tag) {
        if (!task_manager_tag_task(manager, task_id, *tag + 1, error)) {
            task_manager_delete_task(manager, task_id, NULL);
            return FALSE;
        }
    }

    g_string_append_printf(output, "added task %u\n", task_id);
    return TRUE;
}

static gboolean cmd_del(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: del <id>");
        return FALSE;
    }

    guint id;
    if (!parse_id(args[0], &id, error)) {
        return FALSE;
    }

    if (!task_manager_delete_task(manager, id, error)) {
        return FALSE;
    }

    g_string_append_printf(output, "deleted task %u\n", id);
    return TRUE;
}

static gboolean cmd_tag(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] == NULL || args[2] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: tag <id> <#tag>");
        return FALSE;
    }

    guint id;
    if (!parse_id(args[0], &id, error)) {
        return FALSE;
    }

    if (args[1][0] != '#') {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: tag <id> <#tag>");
        return FALSE;
    }

    const gchar *tag = args[1] + 1;
    if (!task_manager_tag_task(manager, id, tag, error)) {
        return FALSE;
    }

    g_string_append_printf(output, "tagged task %u with %s\n", id, tag);
    return TRUE;
}

static gboolean cmd_untag(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] == NULL || args[2] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: untag <id> <#tag>");
        return FALSE;
    }

    guint id;
    if (!parse_id(args[0], &id, error)) {
        return FALSE;
    }

    if (args[1][0] != '#') {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: untag <id> <#tag>");
        return FALSE;
    }

    const gchar *tag = args[1] + 1;
    if (!task_manager_untag_task(manager, id, tag, error)) {
        return FALSE;
    }

    g_string_append_printf(output, "untagged task %u from %s\n", id, tag);
    return TRUE;
}

static gboolean cmd_enqueue(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: enqueue <id>");
        return FALSE;
    }

    guint id;
    if (!parse_id(args[0], &id, error)) {
        return FALSE;
    }

    if (!task_manager_enqueue_task(manager, id, error)) {
        return FALSE;
    }

    g_string_append_printf(output, "enqueued task %u\n", id);
    return TRUE;
}

static gboolean cmd_find(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] != NULL || args[0][0] != '#') {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: find <#tag>");
        return FALSE;
    }

    GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, args[0] + 1, error);
    if (tasks == NULL) {
        return FALSE;
    }

    for (guint i = 0; i < tasks->len; ++i) {
        const Task *task = g_ptr_array_index(tasks, i);
        g_string_append_printf(output, "id: %u, priority: %d, title: %s\n", task->id, task->priority, task->title);
    }

    g_ptr_array_unref(tasks);
    return TRUE;
}

static gboolean cmd_top(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: top <n>");
        return FALSE;
    }

    guint64 n;
    if (!g_ascii_string_to_unsigned(args[0], 10, 0, G_MAXUINT, &n, NULL)) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "expected an unsigned int <n>");
        return FALSE;
    }

    GPtrArray *tasks = task_manager_top_tasks(manager, (guint)n);
    for (guint i = 0; i < tasks->len; ++i) {
        const Task *task = g_ptr_array_index(tasks, i);
        g_string_append_printf(output, "id: %u, priority: %d, title: %s\n", task->id, task->priority, task->title);
    }

    g_ptr_array_unref(tasks);
    return TRUE;
}

static gboolean cmd_run(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: run");
        return FALSE;
    }

    Task *task = task_manager_run_next(manager);
    if (task == NULL) {
        g_string_append_printf(output, "queue is empty\n");
        return TRUE;
    }

    g_string_append_printf(output, "running: %s (id %u, priority %d)\n", task->title, task->id, task->priority);

    task_free(task);
    return TRUE;
}

static gboolean cmd_history(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: history");
        return FALSE;
    }

    GPtrArray *history = task_manager_get_history(manager);
    for (guint i = 0; i < history->len; ++i) {
        const gchar *command = g_ptr_array_index(history, i);
        g_string_append_printf(output, "%s\n", command);
    }

    g_ptr_array_unref(history);
    return TRUE;
}

static gboolean cmd_stats(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: stats");
        return FALSE;
    }

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_string_append_printf(output, "tasks: %u\ntags: %u\nqueue: %u\n", stats.tasks, stats.unique_tags, stats.queue_length);

    return TRUE;
}

static const CliCommand commands[] = {
    {"add", cmd_add},
    {"del", cmd_del},
    {"tag", cmd_tag},
    {"untag", cmd_untag},
    {"enqueue", cmd_enqueue},
    {"find", cmd_find},
    {"top", cmd_top},
    {"run", cmd_run},
    {"history", cmd_history},
    {"stats", cmd_stats},
};

GQuark cli_error_quark(void) { return g_quark_from_static_string("cli-error-quark"); }

CliStatus cli_execute_line(TaskManager *manager, const gchar *line, GString *output, GError **error)
{
    if (line == NULL) {
        return CLI_CONTINUE;
    }

    g_autofree gchar *command = g_strdup(line);
    g_strstrip(command);

    if (command[0] == '\0') {
        return CLI_CONTINUE;
    }

    if (g_strcmp0(command, "quit") == 0) {
        return CLI_QUIT;
    }

    g_auto(GStrv) words = g_regex_split_simple("[ \t]+", command, 0, 0);
    if (g_strcmp0(words[0], "history") != 0) {
        task_manager_record_command(manager, command);
    }

    for (gsize i = 0; i < G_N_ELEMENTS(commands); ++i) {
        const CliCommand *entry = &commands[i];

        if (g_strcmp0(words[0], entry->name) == 0) {
            entry->handler(manager, words + 1, output, error);
            return CLI_CONTINUE;
        }
    }

    g_set_error(error, CLI_ERROR, CLI_ERROR_UNKNOWN_COMMAND, "unknown command: %s", words[0]);
    return CLI_CONTINUE;
}
