#include "cli.h"

typedef struct _CliCommand CliCommand;
typedef gboolean (*CliHandler)(TaskManager *, gchar **, GString *, GError **);

struct _CliCommand {
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

// add <priority> <title> [#tag1 #tag2 ...]
static gboolean cmd_add(TaskManager *manager, gchar **args, GString *output, GError **error)
{
    // validate args
    gint64 priority;
    if (args[0] == NULL || !g_ascii_string_to_signed(args[0], 10, G_MININT, G_MAXINT, &priority, NULL)) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "expected an integer priority");
        return FALSE;
    }

    gchar **title_words = args + 1;
    gchar **first_tag = title_words;

    while (*first_tag != NULL && (*first_tag)[0] != '#') {
        ++first_tag;
    }
    
    if (first_tag == title_words) {
        g_set_error_literal(error, CLI_ERROR, CLI_ERROR_USAGE, "missing task title");
        return FALSE;
    }

    for (gchar **word = first_tag; *word != NULL; ++word) {
        if ((*word)[0] != '#') {
            g_set_error(error, CLI_ERROR, CLI_ERROR_USAGE, "expected a tag, got: %s", *word);
            return FALSE;
        }

        const gchar *tag = *word + 1; // save without '#'
        if (!task_manager_is_valid_tag(tag, error)) {
            return FALSE;
        }
    }

    GString *title = g_string_new(NULL);

    for (gchar **word = title_words; word != first_tag; ++word) {
        if(word != title_words) {
            g_string_append_c(title, ' ');
        }
        g_string_append(title, *word);
    }

    // add task
    guint task_id = task_manager_add_task(manager, title->str, (gint)priority, error);
    g_string_free(title, TRUE);

    if (task_id == 0) {
        return FALSE;
    }

    for (gchar **tag = first_tag; *tag != NULL; ++tag) {
        if (!task_manager_tag_task(manager, task_id, *tag + 1, error)) {
            task_manager_delete_task(manager, task_id, NULL);
            return FALSE;
        }
    }

    g_string_append_printf(output, "added task %u\n", task_id);
    return TRUE;
}

static gboolean cmd_del(TaskManager *manager, gchar **args, GString *output, GError **error)
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

static gboolean cmd_tag(TaskManager *manager, gchar **args, GString *output, GError **error)
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

static gboolean cmd_untag(TaskManager *manager, gchar **args, GString *output, GError **error)
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

static gboolean cmd_enqueue(TaskManager *manager, gchar **args, GString *output, GError **error)
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

static gboolean cmd_find(TaskManager *manager, gchar **args, GString *output, GError **error)
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
        g_string_append_printf(output, "%u %d %s\n", task->id, task->priority, task->title);
    }

    g_ptr_array_unref(tasks);
    return TRUE;
}

static const CliCommand commands[] = {
    { "add", cmd_add },
    { "del", cmd_del },
    { "tag", cmd_tag },
    { "untag", cmd_untag },
    { "enqueue", cmd_enqueue },
    { "find", cmd_find },
};

GQuark cli_error_quark(void)
{
    return g_quark_from_static_string("cli-error-quark");
}

CliStatus cli_execute_line(TaskManager *manager, const gchar *line, GString *output, GError **error)
{
    CliStatus result = CLI_CONTINUE;

    if (line == NULL) {
        return result;
    }

    gchar *command = g_strstrip(g_strdup(line));
    if (command == NULL || command[0] == '\0') {
        goto exit_fail;
    }

    if (g_strcmp0(command, "quit") == 0) {
        result = CLI_QUIT;
        goto exit_fail;
    }

    task_manager_record_command(manager, command);
    gchar **words = g_regex_split_simple("[ \t]+", command, 0, 0);

    for (gsize i = 0; i < G_N_ELEMENTS(commands); ++i) {
        const CliCommand *command = &commands[i];

        if (g_strcmp0(words[0], command->name) == 0) {
            command->handler(manager, words + 1, output, error);
            goto exit;
        }
    }

    g_set_error(error, CLI_ERROR, CLI_ERROR_UNKNOWN_COMMAND, "unknown command: %s", words[0]);
exit:
    g_strfreev(words);

exit_fail:
    g_free(command);
    return result;
}
