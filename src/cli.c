#include "cli.h"
#include "task.h"

#include <stddef.h>

typedef struct _CliCommand CliCommand;
typedef void (*CliHandler)(TaskManager *, gchar *const *, GString *, GError **);

struct _CliCommand
{
    const gchar *name;
    CliHandler handler;
};

static void append_title_words(GString *title, gchar *const *first_word, gchar *const *end)
{
    for (gchar *const *word = first_word; word != end; ++word) {
        if (word != first_word) {
            g_string_append_c(title, ' ');
        }
        g_string_append(title, *word);
    }
}

static void append_task_list(GString *output, const GPtrArray *tasks)
{
    for (guint i = 0; i < tasks->len; ++i) {
        const Task *task = g_ptr_array_index(tasks, i);
        g_string_append_printf(
            output, "id: %u, priority: %d, title: %s\n", task_get_id(task), task_get_priority(task), task_get_title(task));
    }
}

static gchar *strip_tag_prefix(gchar *word) { return word != NULL && word[0] == '#' ? word + 1 : NULL; }

static gchar *const *find_first_tag_token(gchar *const *words)
{
    while (*words != NULL && strip_tag_prefix(*words) == NULL) {
        ++words;
    }

    return words;
}

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

static gboolean parse_add_tags(gchar *const *first_tag, GPtrArray *tags, GError **error)
{
    for (gchar *const *word = first_tag; *word != NULL; ++word) {
        gchar *tag = strip_tag_prefix(*word);
        if (tag == NULL) {
            g_set_error(error, CLI_ERROR, CLI_ERROR_USAGE, "expected a tag, got: %s", *word);
            return FALSE;
        }

        g_ptr_array_add(tags, tag);
    }

    return TRUE;
}

static gboolean parse_add_args(gchar *const *args, gint *priority, GString *title, GPtrArray *tags, GError **error)
{
    const gchar *usage = "usage: add <priority> <title> [#tag ...]";
    if (args[0] == NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, usage);
        return FALSE;
    }

    gint64 parsed_priority;
    if (!g_ascii_string_to_signed(args[0], 10, G_MININT, G_MAXINT, &parsed_priority, NULL)) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, usage);
        return FALSE;
    }

    gchar *const *const title_words = args + 1;
    gchar *const *const first_tag = find_first_tag_token(title_words);
    if (first_tag == title_words) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "missing task title");
        return FALSE;
    }

    if (!parse_add_tags(first_tag, tags, error)) {
        return FALSE;
    }

    append_title_words(title, title_words, first_tag);

    *priority = (gint)parsed_priority;
    return TRUE;
}

// add <priority> <title> [#tag1 #tag2 ...]
static void cmd_add(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    gint priority;
    g_autoptr(GString) title = g_string_new(NULL);
    g_autoptr(GPtrArray) tags = g_ptr_array_new();
    if (!parse_add_args(args, &priority, title, tags, error)) {
        return;
    }

    const guint task_id = task_manager_add_task_with_tags(manager, title->str, priority, tags, error);
    if (task_id == 0) {
        return;
    }

    g_string_append_printf(output, "added task %u\n", task_id);
}

static void cmd_del(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: del <id>");
        return;
    }

    guint id;
    if (!parse_id(args[0], &id, error)) {
        return;
    }

    if (!task_manager_remove_task(manager, id, error)) {
        return;
    }

    g_string_append_printf(output, "deleted task %u\n", id);
}

static void cmd_tag(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] == NULL || args[2] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: tag <id> <#tag>");
        return;
    }

    guint id;
    if (!parse_id(args[0], &id, error)) {
        return;
    }

    const gchar *tag = strip_tag_prefix(args[1]);
    if (tag == NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: tag <id> <#tag>");
        return;
    }

    if (!task_manager_tag_task(manager, id, tag, error)) {
        return;
    }

    g_string_append_printf(output, "tagged task %u with %s\n", id, tag);
}

static void cmd_untag(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] == NULL || args[2] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: untag <id> <#tag>");
        return;
    }

    guint id;
    if (!parse_id(args[0], &id, error)) {
        return;
    }

    const gchar *tag = strip_tag_prefix(args[1]);
    if (tag == NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: untag <id> <#tag>");
        return;
    }

    if (!task_manager_untag_task(manager, id, tag, error)) {
        return;
    }

    g_string_append_printf(output, "untagged task %u from %s\n", id, tag);
}

static void cmd_enqueue(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: enqueue <id>");
        return;
    }

    guint id;
    if (!parse_id(args[0], &id, error)) {
        return;
    }

    if (!task_manager_enqueue_task(manager, id, error)) {
        return;
    }

    g_string_append_printf(output, "enqueued task %u\n", id);
}

static void cmd_run(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: run");
        return;
    }

    Task *task = task_manager_extract_next_task(manager);
    if (task == NULL) {
        g_string_append_printf(output, "queue is empty\n");
        return;
    }

    g_string_append_printf(output, "running: %s (id %u, priority %d)\n", task_get_title(task), task_get_id(task), task_get_priority(task));

    task_free(task);
}

static void cmd_find(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: find <#tag>");
        return;
    }

    const gchar *tag = strip_tag_prefix(args[0]);
    if (tag == NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: find <#tag>");
        return;
    }

    g_autoptr(GPtrArray) tasks = task_manager_find_tasks_by_tag(manager, tag);
    append_task_list(output, tasks);
}

static void cmd_top(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] == NULL || args[1] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: top <n>");
        return;
    }

    guint64 n;
    if (!g_ascii_string_to_unsigned(args[0], 10, 0, G_MAXUINT, &n, NULL)) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "expected an unsigned int <n>");
        return;
    }

    g_autoptr(GPtrArray) tasks = task_manager_top_tasks(manager, (guint)n);
    append_task_list(output, tasks);
}

static void cmd_history(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: history");
        return;
    }

    g_autoptr(GPtrArray) history = task_manager_get_history(manager);
    for (guint i = 0; i < history->len; ++i) {
        const gchar *command = g_ptr_array_index(history, i);
        g_string_append_printf(output, "%s\n", command);
    }
}

static void cmd_stats(TaskManager *manager, gchar *const *args, GString *output, GError **error)
{
    if (args[0] != NULL) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "usage: stats");
        return;
    }

    const TaskManagerStats stats = task_manager_get_stats(manager);
    g_string_append_printf(output, "tasks: %u\ntags: %u\nqueue: %u\n", stats.tasks, stats.unique_tags, stats.queue_length);
}

static const CliCommand commands[] = {
    {"add", cmd_add},
    {"del", cmd_del},
    {"tag", cmd_tag},
    {"untag", cmd_untag},
    {"enqueue", cmd_enqueue},
    {"run", cmd_run},
    {"find", cmd_find},
    {"top", cmd_top},
    {"history", cmd_history},
    {"stats", cmd_stats},
};

GQuark cli_error_quark(void) { return g_quark_from_static_string("cli-error-quark"); }

CliStatus cli_execute_line(TaskManager *manager, const gchar *command_line, GString *output, GError **error)
{
    if (command_line == NULL) {
        return CLI_CONTINUE;
    }

    g_autofree gchar *command = g_strdup(command_line);
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
