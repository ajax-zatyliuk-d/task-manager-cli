#include "cli.h"

static void test_cli_base(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    CliStatus status;
    GError *error = NULL;
    GPtrArray *history;
    GString *result = g_string_new(NULL);
    const gchar *string = "test 1 2 #d";

    // empty cmd
    status = cli_execute_line(manager, "", result, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);

    history = task_manager_get_history(manager);
    g_assert_nonnull(history);
    g_assert_cmpuint(history->len, ==, 0);
    g_assert_cmpstr(result->str, ==, "");

    g_ptr_array_unref(history);

    // spaces cmd
    status = cli_execute_line(manager, "    ", result, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);

    history = task_manager_get_history(manager);
    g_assert_nonnull(history);
    g_assert_cmpuint(history->len, ==, 0);
    g_assert_cmpstr(result->str, ==, "");

    g_ptr_array_unref(history);

    // quit cmd
    status = cli_execute_line(manager, "quit", result, &error);
    g_assert_cmpuint(status, ==, CLI_QUIT);
    g_assert_no_error(error);

    history = task_manager_get_history(manager);
    g_assert_nonnull(history);
    g_assert_cmpuint(history->len, ==, 0);
    g_assert_cmpstr(result->str, ==, "");

    g_ptr_array_unref(history);

    // random cmd
    status = cli_execute_line(manager, string, result, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_UNKNOWN_COMMAND);
    g_clear_error(&error);

    g_assert_cmpstr(result->str, ==, "");
    history = task_manager_get_history(manager);
    g_assert_nonnull(history);
    g_assert_cmpuint(history->len, ==, 1);
    g_assert_cmpstr(history->pdata[0], ==, string);
    g_assert_cmpstr(result->str, ==, "");

    g_string_free(result, TRUE);
    g_ptr_array_unref(history);

    task_manager_free(manager);
}

static void test_cli_add(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    CliStatus status;
    GError *error = NULL;
    GString *output = g_string_new(NULL);
    const gchar *cmd1 = "add 5 buy milk #home #shop";
    const gchar *cmd2 = "add 5 title";
    const gchar *cmd3 = "add  5   buy   milk  #a";

    // cmd1 with 2 tags
    status = cli_execute_line(manager, cmd1, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "added task 1\n");
    
    const Task *task1 = task_manager_find_task_by_id(manager, 1, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(task1->title, ==, "buy milk");
    g_assert_cmpuint(task1->tags->len, ==, 2);
    g_assert_cmpint(task1->priority, ==, 5);

    g_string_truncate(output, 0);
    
    // cmd2 without tags
    status = cli_execute_line(manager, cmd2, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "added task 2\n");
    
    const Task *task2 = task_manager_find_task_by_id(manager, 2, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(task2->title, ==, "title");
    g_assert_cmpuint(task2->tags->len, ==, 0);
    g_assert_cmpint(task2->priority, ==, 5);

    g_string_truncate(output, 0);

    // cmd3 with tag
    status = cli_execute_line(manager, cmd3, output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "added task 3\n");
    
    const Task *task3 = task_manager_find_task_by_id(manager, 3, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(task3->title, ==, "buy milk");
    g_assert_cmpuint(task3->tags->len, ==, 1);
    g_assert_cmpint(task3->priority, ==, 5);

    g_string_free(output, TRUE);
    task_manager_free(manager);
}


static void test_cli_add_invalid(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    CliStatus status;
    GError *error = NULL;
    // GPtrArray *history;
    GString *output = g_string_new(NULL);

    status = cli_execute_line(manager, "add", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);

    status = cli_execute_line(manager, "add 5", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);

    status = cli_execute_line(manager, "add x title", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);

    status = cli_execute_line(manager, "add 11 title", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);

    status = cli_execute_line(manager, "add 5 title #Bad", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);

    status = cli_execute_line(manager, "add 5 title #ok word", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);

    status = cli_execute_line(manager, "add 5 #tag", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(output->len, ==, 0);

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_del(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    CliStatus status = cli_execute_line(manager, "del 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "deleted task 1\n");

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_null(task);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_del_invalid(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    const struct {
        const gchar *command;
        GQuark domain;
        gint code;
    } cases[] = {
        { "del",     CLI_ERROR, CLI_ERROR_USAGE },
        { "del 1 2", CLI_ERROR, CLI_ERROR_USAGE },
        { "del abc", CLI_ERROR, CLI_ERROR_USAGE },
        { "del 0",   CLI_ERROR, CLI_ERROR_USAGE },
        { "del 999", TM_ERROR,  TM_ERROR_TASK_NOT_FOUND },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_test_message("%s", cases[i].command);

        CliStatus status = cli_execute_line(manager, cases[i].command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_error(error, cases[i].domain, cases[i].code);
        g_clear_error(&error);
        g_assert_cmpstr(output->str, ==, "");

        TaskManagerStats stats = task_manager_get_stats(manager);
        g_assert_cmpuint(stats.tasks, ==, 1);

        const Task *task = task_manager_find_task_by_id(manager, id, &error);
        g_assert_no_error(error);
        g_assert_nonnull(task);
    }

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_tag(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    CliStatus status = cli_execute_line(manager, "tag 1 #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "tagged task 1 with work\n");

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpuint(task->tags->len, ==, 1);
    g_assert_cmpstr(g_ptr_array_index(task->tags, 0), ==, "work");

    GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(tasks, 0) == task);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_ptr_array_unref(tasks);

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_tag_invalid(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    const struct {
        const gchar *command;
        GQuark domain;
        gint code;
    } cases[] = {
        { "tag",               CLI_ERROR, CLI_ERROR_USAGE },
        { "tag 1",             CLI_ERROR, CLI_ERROR_USAGE },
        { "tag 1 #work extra", CLI_ERROR, CLI_ERROR_USAGE },
        { "tag abc #work",     CLI_ERROR, CLI_ERROR_USAGE },
        { "tag 0 #work",       CLI_ERROR, CLI_ERROR_USAGE },
        { "tag 1 work",        CLI_ERROR, CLI_ERROR_USAGE },
        { "tag 1 #Bad",      TM_ERROR,  TM_ERROR_INVALID_TAG },
        { "tag 1 #",           TM_ERROR,  TM_ERROR_INVALID_TAG },
        { "tag 999 #work",     TM_ERROR,  TM_ERROR_TASK_NOT_FOUND },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_test_message("%s", cases[i].command);

        CliStatus status = cli_execute_line(manager, cases[i].command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_error(error, cases[i].domain, cases[i].code);
        g_clear_error(&error);
        g_assert_cmpstr(output->str, ==, "");

        TaskManagerStats stats = task_manager_get_stats(manager);
        g_assert_cmpuint(stats.tasks, ==, 1);

        const Task *task = task_manager_find_task_by_id(manager, id, &error);
        g_assert_no_error(error);
        g_assert_nonnull(task);
        g_assert_cmpuint(task->tags->len, ==, 0);

        GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, "work", &error);
        g_assert_no_error(error);
        g_assert_nonnull(tasks);
        g_assert_cmpuint(tasks->len, ==, 0);
        g_ptr_array_unref(tasks);
    }

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_untag(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id1 = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    guint id2 = task_manager_add_task(manager, "second", 7, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id1, ==, 1);
    g_assert_cmpuint(id2, ==, 2);

    g_assert_true(task_manager_tag_task(manager, id1, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id2, "work", &error));
    g_assert_no_error(error);

    // Remove the tag from the first task.
    CliStatus status = cli_execute_line(manager, "untag 1 #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "untagged task 1 from work\n");

    const Task *task1 = task_manager_find_task_by_id(manager, id1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task1);
    g_assert_cmpuint(task1->tags->len, ==, 0);

    const Task *task2 = task_manager_find_task_by_id(manager, id2, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task2);
    g_assert_cmpuint(task2->tags->len, ==, 1);
    g_assert_cmpstr(g_ptr_array_index(task2->tags, 0), ==, "work");

    GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(tasks, 0) == task2);
    g_ptr_array_unref(tasks);

    g_string_truncate(output, 0);

    // Remove the last occurrence of the tag.
    status = cli_execute_line(manager, "untag 2 #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "untagged task 2 from work\n");
    g_assert_cmpuint(task2->tags->len, ==, 0);

    tasks = task_manager_find_tasks_by_tag(manager, "work", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tasks);
    g_assert_cmpuint(tasks->len, ==, 0);
    g_ptr_array_unref(tasks);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 2);

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_untag_invalid(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    const struct {
        const gchar *command;
        GQuark domain;
        gint code;
    } cases[] = {
        { "untag",               CLI_ERROR, CLI_ERROR_USAGE },
        { "untag 1",             CLI_ERROR, CLI_ERROR_USAGE },
        { "untag 1 #work extra", CLI_ERROR, CLI_ERROR_USAGE },
        { "untag abc #work",     CLI_ERROR, CLI_ERROR_USAGE },
        { "untag 0 #work",       CLI_ERROR, CLI_ERROR_USAGE },
        { "untag 1 work",        CLI_ERROR, CLI_ERROR_USAGE },
        { "untag 1 #Bad",      TM_ERROR,  TM_ERROR_INVALID_TAG },
        { "untag 1 #",           TM_ERROR,  TM_ERROR_INVALID_TAG },
        { "untag 999 #work",     TM_ERROR,  TM_ERROR_TASK_NOT_FOUND },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_test_message("%s", cases[i].command);

        CliStatus status = cli_execute_line(manager, cases[i].command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_error(error, cases[i].domain, cases[i].code);
        g_clear_error(&error);
        g_assert_cmpstr(output->str, ==, "");

        TaskManagerStats stats = task_manager_get_stats(manager);
        g_assert_cmpuint(stats.tasks, ==, 1);

        const Task *task = task_manager_find_task_by_id(manager, id, &error);
        g_assert_no_error(error);
        g_assert_nonnull(task);
        g_assert_cmpuint(task->tags->len, ==, 1);
        g_assert_cmpstr(g_ptr_array_index(task->tags, 0), ==, "work");

        GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, "work", &error);
        g_assert_no_error(error);
        g_assert_nonnull(tasks);
        g_assert_cmpuint(tasks->len, ==, 1);
        g_assert_true(g_ptr_array_index(tasks, 0) == task);
        g_ptr_array_unref(tasks);
    }

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_enqueue(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.queue_length, ==, 0);

    CliStatus status = cli_execute_line(manager, "enqueue 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "enqueued task 1\n");

    stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 1);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);
    g_assert_cmpstr(task->title, ==, "title");

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_enqueue_invalid(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    const struct {
        const gchar *command;
        GQuark domain;
        gint code;
    } cases[] = {
        { "enqueue",     CLI_ERROR, CLI_ERROR_USAGE },
        { "enqueue 1 2", CLI_ERROR, CLI_ERROR_USAGE },
        { "enqueue abc", CLI_ERROR, CLI_ERROR_USAGE },
        { "enqueue 0",   CLI_ERROR, CLI_ERROR_USAGE },
        { "enqueue 999", TM_ERROR,  TM_ERROR_TASK_NOT_FOUND },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_test_message("%s", cases[i].command);

        CliStatus status = cli_execute_line(manager, cases[i].command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_error(error, cases[i].domain, cases[i].code);
        g_clear_error(&error);
        g_assert_cmpstr(output->str, ==, "");

        TaskManagerStats stats = task_manager_get_stats(manager);
        g_assert_cmpuint(stats.tasks, ==, 1);
        g_assert_cmpuint(stats.queue_length, ==, 0);

        const Task *task = task_manager_find_task_by_id(manager, id, &error);
        g_assert_no_error(error);
        g_assert_nonnull(task);
        g_assert_cmpstr(task->title, ==, "title");
    }

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_find(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    // Empty manager.
    CliStatus status = cli_execute_line(manager, "find #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");

    guint id1 = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    guint id2 = task_manager_add_task(manager, "second", 7, &error);
    g_assert_no_error(error);
    guint id3 = task_manager_add_task(manager, "third", 3, &error);
    g_assert_no_error(error);

    g_assert_cmpuint(id1, ==, 1);
    g_assert_cmpuint(id2, ==, 2);
    g_assert_cmpuint(id3, ==, 3);

    g_assert_true(task_manager_tag_task(manager, id1, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id2, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id3, "home", &error));
    g_assert_no_error(error);

    // Two matching tasks, either order.
    status = cli_execute_line(manager, "find #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_true(
        g_strcmp0(output->str, "1 5 first\n2 7 second\n") == 0 ||
        g_strcmp0(output->str, "2 7 second\n1 5 first\n") == 0
    );

    g_string_truncate(output, 0);

    // Repeated lookup returns the same matches.
    status = cli_execute_line(manager, "find #work", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_true(
        g_strcmp0(output->str, "1 5 first\n2 7 second\n") == 0 ||
        g_strcmp0(output->str, "2 7 second\n1 5 first\n") == 0
    );

    g_string_truncate(output, 0);

    // A different tag.
    status = cli_execute_line(manager, "find #home", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "3 3 third\n");

    g_string_truncate(output, 0);

    // Valid tag without matches.
    status = cli_execute_line(manager, "find #missing", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 3);

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_find_invalid(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    g_assert_true(task_manager_tag_task(manager, id, "work", &error));
    g_assert_no_error(error);

    const struct {
        const gchar *command;
        GQuark domain;
        gint code;
    } cases[] = {
        { "find",             CLI_ERROR, CLI_ERROR_USAGE },
        { "find #work extra", CLI_ERROR, CLI_ERROR_USAGE },
        { "find work",        CLI_ERROR, CLI_ERROR_USAGE },
        { "find #Bad",      TM_ERROR,  TM_ERROR_INVALID_TAG },
        { "find #",           TM_ERROR,  TM_ERROR_INVALID_TAG },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_test_message("%s", cases[i].command);

        CliStatus status = cli_execute_line(manager, cases[i].command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_error(error, cases[i].domain, cases[i].code);
        g_clear_error(&error);
        g_assert_cmpstr(output->str, ==, "");

        TaskManagerStats stats = task_manager_get_stats(manager);
        g_assert_cmpuint(stats.tasks, ==, 1);

        const Task *task = task_manager_find_task_by_id(manager, id, &error);
        g_assert_no_error(error);
        g_assert_nonnull(task);
        g_assert_cmpstr(task->title, ==, "title");
        g_assert_cmpint(task->priority, ==, 5);
        g_assert_cmpuint(task->tags->len, ==, 1);
        g_assert_cmpstr(g_ptr_array_index(task->tags, 0), ==, "work");

        GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, "work", &error);
        g_assert_no_error(error);
        g_assert_nonnull(tasks);
        g_assert_cmpuint(tasks->len, ==, 1);
        g_assert_true(g_ptr_array_index(tasks, 0) == task);
        g_ptr_array_unref(tasks);
    }

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_top(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    CliStatus status = cli_execute_line(manager, "top 2", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");

    guint id1 = task_manager_add_task(manager, "low", 3, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id1, ==, 1);

    guint id2 = task_manager_add_task(manager, "high", 9, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id2, ==, 2);

    guint id3 = task_manager_add_task(manager, "medium", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id3, ==, 3);

    const struct {
        const gchar *command;
        const gchar *expected_output;
    } cases[] = {
        { "top 2",  "2 9 high\n3 5 medium\n" },
        { "top 10", "2 9 high\n3 5 medium\n1 3 low\n" },
        { "top 0",  "" },
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_test_message("%s", cases[i].command);
        g_string_truncate(output, 0);

        status = cli_execute_line(manager, cases[i].command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_no_error(error);
        g_assert_cmpstr(output->str, ==, cases[i].expected_output);

        TaskManagerStats stats = task_manager_get_stats(manager);
        g_assert_cmpuint(stats.tasks, ==, 3);
    }

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_top_invalid(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id = task_manager_add_task(manager, "title", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id, ==, 1);

    const gchar *cases[] = {
        "top",
        "top 1 2",
        "top abc",
        "top -1",
    };

    for (gsize i = 0; i < G_N_ELEMENTS(cases); ++i) {
        g_test_message("%s", cases[i]);

        CliStatus status = cli_execute_line(manager, cases[i], output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
        g_clear_error(&error);
        g_assert_cmpstr(output->str, ==, "");

        TaskManagerStats stats = task_manager_get_stats(manager);
        g_assert_cmpuint(stats.tasks, ==, 1);

        const Task *task = task_manager_find_task_by_id(manager, id, &error);
        g_assert_no_error(error);
        g_assert_nonnull(task);
        g_assert_cmpstr(task->title, ==, "title");
        g_assert_cmpint(task->priority, ==, 5);
    }

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_run(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    guint id1 = task_manager_add_task(manager, "first", 3, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id1, ==, 1);

    guint id2 = task_manager_add_task(manager, "second", 9, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id2, ==, 2);

    CliStatus status = cli_execute_line(manager, "enqueue 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);

    status = cli_execute_line(manager, "enqueue 2", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_string_truncate(output, 0);

    // Invalid arguments leave both tasks queued.
    status = cli_execute_line(manager, "run 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    TaskManagerStats stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 2);
    g_assert_cmpuint(stats.queue_length, ==, 2);

    // FIFO runs the first enqueued task even though it has lower priority.
    status = cli_execute_line(manager, "run", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "running: first (id 1, priority 3)\n");

    const Task *task = task_manager_find_task_by_id(manager, id1, &error);
    g_assert_null(task);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    task = task_manager_find_task_by_id(manager, id2, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task);

    stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 1);
    g_assert_cmpuint(stats.queue_length, ==, 1);

    g_string_truncate(output, 0);

    status = cli_execute_line(manager, "run", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "running: second (id 2, priority 9)\n");

    stats = task_manager_get_stats(manager);
    g_assert_cmpuint(stats.tasks, ==, 0);
    g_assert_cmpuint(stats.queue_length, ==, 0);

    g_string_truncate(output, 0);

    status = cli_execute_line(manager, "run", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "queue is empty\n");

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_history(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    CliStatus status = cli_execute_line(manager, "history", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "");

    status = cli_execute_line(manager, "add 5 title", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_string_truncate(output, 0);

    status = cli_execute_line(manager, "del 999", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    for (guint i = 0; i < 2; ++i) {
        status = cli_execute_line(manager, "history", output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_no_error(error);
        g_assert_cmpstr(output->str, ==, "add 5 title\ndel 999\n");
        g_string_truncate(output, 0);
    }

    status = cli_execute_line(manager, "history extra", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    status = cli_execute_line(manager, "history", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "add 5 title\ndel 999\n");

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_history_limit(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    for (guint i = 1; i <= 11; ++i) {
        gchar *command = g_strdup_printf("add 5 task%u", i);

        CliStatus status = cli_execute_line(manager, command, output, &error);
        g_assert_cmpuint(status, ==, CLI_CONTINUE);
        g_assert_no_error(error);

        g_free(command);
        g_string_truncate(output, 0);
    }

    CliStatus status = cli_execute_line(manager, "history", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==,
                    "add 5 task2\n"
                    "add 5 task3\n"
                    "add 5 task4\n"
                    "add 5 task5\n"
                    "add 5 task6\n"
                    "add 5 task7\n"
                    "add 5 task8\n"
                    "add 5 task9\n"
                    "add 5 task10\n"
                    "add 5 task11\n");

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

static void test_cli_stats(void)
{
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GError *error = NULL;
    GString *output = g_string_new(NULL);

    CliStatus status = cli_execute_line(manager, "stats", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "tasks: 0\ntags: 0\nqueue: 0\n");
    g_string_truncate(output, 0);

    guint id1 = task_manager_add_task(manager, "first", 5, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id1, ==, 1);

    guint id2 = task_manager_add_task(manager, "second", 7, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(id2, ==, 2);

    g_assert_true(task_manager_tag_task(manager, id1, "work", &error));
    g_assert_no_error(error);
    g_assert_true(task_manager_tag_task(manager, id2, "work", &error));
    g_assert_no_error(error);

    status = cli_execute_line(manager, "enqueue 1", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_string_truncate(output, 0);

    status = cli_execute_line(manager, "stats", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_no_error(error);
    g_assert_cmpstr(output->str, ==, "tasks: 2\ntags: 1\nqueue: 1\n");
    g_string_truncate(output, 0);

    status = cli_execute_line(manager, "stats extra", output, &error);
    g_assert_cmpuint(status, ==, CLI_CONTINUE);
    g_assert_error(error, CLI_ERROR, CLI_ERROR_USAGE);
    g_clear_error(&error);
    g_assert_cmpstr(output->str, ==, "");

    g_string_free(output, TRUE);
    task_manager_free(manager);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/cli/basic", test_cli_base);

    g_test_add_func("/cli/add", test_cli_add);
    g_test_add_func("/cli/add-invalid", test_cli_add_invalid);

    g_test_add_func("/cli/del", test_cli_del);
    g_test_add_func("/cli/del-invalid", test_cli_del_invalid);

    g_test_add_func("/cli/tag", test_cli_tag);
    g_test_add_func("/cli/tag-invalid", test_cli_tag_invalid);

    g_test_add_func("/cli/untag", test_cli_untag);
    g_test_add_func("/cli/untag-invalid", test_cli_untag_invalid);

    g_test_add_func("/cli/enqueue", test_cli_enqueue);
    g_test_add_func("/cli/enqueue-invalid", test_cli_enqueue_invalid);

    g_test_add_func("/cli/find", test_cli_find);
    g_test_add_func("/cli/find-invalid", test_cli_find_invalid);

    g_test_add_func("/cli/top", test_cli_top);
    g_test_add_func("/cli/top-invalid", test_cli_top_invalid);

    g_test_add_func("/cli/run", test_cli_run);

    g_test_add_func("/cli/history", test_cli_history);
    g_test_add_func("/cli/history-limit", test_cli_history_limit);

    g_test_add_func("/cli/stats", test_cli_stats);

    return g_test_run();
}
