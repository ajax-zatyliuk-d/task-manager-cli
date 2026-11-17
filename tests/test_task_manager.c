#include "task_manager.h"

static void test_add_find(void)
{
    GError *error = NULL;

    gchar *title_first = "First";
    gint priority_first = 1;
    gchar *title_second = "Second";
    gint priority_second = 10;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint id_first = task_manager_add_task(manager, title_first, priority_first, &error);
    g_assert_no_error(error);

    guint id_second = task_manager_add_task(manager, title_second, priority_second, &error);
    g_assert_no_error(error);

    const Task *task_first = task_manager_find_task_by_id(manager, id_first, &error);
    g_assert_no_error(error);
    do { if ((task_first) != ((void*)0)) ; else g_assertion_message (((gchar*) 0), "/home/zatyliuk/Work/glib_test/tests/test_task_manager.c", 23, ((const char*) (__func__)), "'" "task_first" "' should not be NULL"); } while (0);
    g_assert_nonnull(task_first->tags);
    g_assert_nonnull(task_first->title);
    g_assert_cmpuint(task_first->id, ==, 1);
    g_assert_cmpint(task_first->priority, ==, priority_first);
    g_assert_cmpstr(task_first->title, ==, title_first);
    
    const Task *task_second = task_manager_find_task_by_id(manager, id_second, &error);
    g_assert_no_error(error);
    g_assert_nonnull(task_second);
    g_assert_nonnull(task_second->tags);
    g_assert_nonnull(task_second->title);
    g_assert_cmpuint(task_second->id, ==, 2);
    g_assert_cmpint(task_second->priority, ==, priority_second);
    g_assert_cmpstr(task_second->title, ==, title_second);

    task_manager_free(manager);
}

static void test_invalid_priority(void)
{
    GError *error = NULL;

    gchar *title = "Invalid priority";
    gint priority = 1;
    gint big_priority = 11;
    gint small_priority = 0;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint id = task_manager_add_task(manager, title, big_priority, &error);
    g_assert_cmpuint(id, ==, 0);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);

    id = task_manager_add_task(manager, title, small_priority, &error);
    g_assert_cmpuint(id, ==, 0);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_PRIORITY);
    g_clear_error(&error);

    id = task_manager_add_task(manager, title, small_priority, NULL);
    g_assert_cmpuint(id, ==, 0);

    id = task_manager_add_task(manager, title, priority, &error);
    g_assert_no_error(error);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_no_error(error);

    g_assert_nonnull(task);
    g_assert_nonnull(task->tags);
    g_assert_nonnull(task->title);
    g_assert_cmpuint(task->id, ==, 1);
    g_assert_cmpint(task->priority, ==, priority);
    g_assert_cmpstr(task->title, ==, title);

    task_manager_free(manager);
}

static void test_not_found(void)
{
    GError *error = NULL;

    guint id = 50;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    const Task *task = task_manager_find_task_by_id(manager, id, &error);
    g_assert_null(task);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);

    task = task_manager_find_task_by_id(manager, id, NULL);
    g_assert_null(task);

    task_manager_free(manager);
}

static void test_tag(void)
{
    GError *error = NULL;
    const gchar *tag = "tag";

    const gchar *title1 = "cmd";
    gint priority1 = 7;

    const gchar *title2 = "set";
    gint priority2 = 5;

    gboolean result = FALSE;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    // first task
    guint task1_id = task_manager_add_task(manager, title1, priority1, &error);
    g_assert_no_error(error);

    const Task *task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_no_error(error);

    result = task_manager_tag_task(manager, task1_id, tag, &error);
    g_assert_no_error(error);
    g_assert_true(result);
    g_assert_cmpint(task1->tags->len, ==, 1);
    g_assert_cmpstr(task1->tags->pdata[0], ==, tag);

    result = task_manager_tag_task(manager, task1_id, tag, &error);
    g_assert_no_error(error);
    g_assert_true(result);
    g_assert_cmpint(task1->tags->len, ==, 1);

    // second task
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    g_assert_no_error(error);

    const Task *task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_no_error(error);

    result = task_manager_tag_task(manager, task2_id, tag, &error);
    g_assert_no_error(error);
    g_assert_true(result);
    g_assert_cmpint(task1->tags->len, ==, 1);
    g_assert_cmpstr(task1->tags->pdata[0], ==, tag);
    g_assert_cmpint(task2->tags->len, ==, 1);
    g_assert_cmpstr(task2->tags->pdata[0], ==, tag);

    task_manager_free(manager);
}

static void test_invalid_tag(void)
{
    GError *error = NULL;

    const gchar *title = "cmd";
    gint priority = 1;

    const gchar *tag = "tag";
    const gchar *empty = "";
    const gchar *camel_case = "Tag";
    const gchar *space = "t a g";

    gboolean result = TRUE;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task_id = task_manager_add_task(manager, title, priority, &error);
    g_assert_no_error(error);

    const Task *task = task_manager_find_task_by_id(manager, task_id, &error);
    g_assert_no_error(error);

    result = task_manager_tag_task(manager, task_id, empty, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);
    g_assert_cmpint(task->tags->len, ==, 0);

    result = task_manager_tag_task(manager, task_id, camel_case, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);
    g_assert_cmpint(task->tags->len, ==, 0);

    result = task_manager_tag_task(manager, task_id, space, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);
    g_assert_cmpint(task->tags->len, ==, 0);

    result = task_manager_tag_task(manager, 999, tag, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(result);
    g_assert_cmpint(task->tags->len, ==, 0);

    result = task_manager_tag_task(manager, task_id, space, NULL);
    g_assert_false(result);
    g_assert_cmpint(task->tags->len, ==, 0);

    task_manager_free(manager);
}

static void test_find_by_tag(void)
{
    GError *error = NULL;
    const gchar *tag = "tag";
    const gchar *tag_other = "other";
    const gchar *unused_tag = "unused_tag";

    const gchar *title1 = "cmd";
    gint priority1 = 7;

    const gchar *title2 = "set";
    gint priority2 = 5;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    // first task
    guint task1_id = task_manager_add_task(manager, title1, priority1, &error);
    g_assert_no_error(error);
    const Task *task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_no_error(error);
    task_manager_tag_task(manager, task1_id, tag, &error);
    g_assert_no_error(error);

    // second task
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    g_assert_no_error(error);
    const Task *task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_no_error(error);
    task_manager_tag_task(manager, task2_id, tag, &error);
    task_manager_tag_task(manager, task2_id, tag_other, &error);
    g_assert_no_error(error);

    const GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_nonnull(tasks);
    g_assert_cmpint(tasks->len, ==, 2);
    g_assert_true(g_ptr_array_index(tasks, 0) == task1);
    g_assert_true(g_ptr_array_index(tasks, 1) == task2);

    const GPtrArray *other_tasks = task_manager_find_tasks_by_tag(manager, tag_other, &error);
    g_assert_no_error(error);
    g_assert_nonnull(other_tasks);
    g_assert_cmpint(other_tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(other_tasks, 0) == task2);

    const GPtrArray *no_tasks = task_manager_find_tasks_by_tag(manager, unused_tag, &error);
    g_assert_no_error(error);
    g_assert_null(no_tasks);

    const GPtrArray *no_tasks_again = task_manager_find_tasks_by_tag(manager, unused_tag, NULL);
    g_assert_null(no_tasks_again);

    task_manager_tag_task(manager, task2_id, tag, &error);
    g_assert_no_error(error);

    const GPtrArray *same_tasks = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_nonnull(same_tasks);
    g_assert_cmpint(same_tasks->len, ==, 2);
    g_assert_true((g_ptr_array_index(same_tasks, 0) == task1) && (g_ptr_array_index(same_tasks, 1) == task2));

    task_manager_free(manager);
}

static void test_find_by_invalid_tag(void)
{
    GError *error = NULL;
    const gchar *empty = "";
    const gchar *camel_case = "Tag";
    const gchar *space = "t a g";

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    const GPtrArray *empty_tag_tasks = task_manager_find_tasks_by_tag(manager, empty, &error);
    g_assert_null(empty_tag_tasks);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);

    const GPtrArray *camel_case_tag_tasks = task_manager_find_tasks_by_tag(manager, camel_case, &error);
    g_assert_null(camel_case_tag_tasks);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);

    const GPtrArray *space_tag_tasks = task_manager_find_tasks_by_tag(manager, space, &error);
    g_assert_null(space_tag_tasks);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);

    const GPtrArray *space_tag_tasks_again = task_manager_find_tasks_by_tag(manager, space, NULL);
    g_assert_null(space_tag_tasks_again);

    task_manager_free(manager);
}

static void test_untag(void)
{
    GError *error = NULL;
    gboolean result = FALSE;

    const gchar *tag = "tag";
    const gchar *title1 = "cmd";
    gint priority1 = 7;
    const gchar *title2 = "set";
    gint priority2 = 5;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    // first task
    guint task1_id = task_manager_add_task(manager, title1, priority1, &error);
    const Task *task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    task_manager_tag_task(manager, task1_id, tag, &error);

    // second task
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    const Task *task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    task_manager_tag_task(manager, task2_id, tag, &error);

    g_assert_no_error(error);
    g_assert_true(task1_id != 0);
    g_assert_true(task2_id != 0);

    // untag tag from first
    result = task_manager_untag_task(manager, task1_id, tag, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    const GPtrArray *tasks = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_nonnull(tasks);
    g_assert_cmpint(tasks->len, ==, 1);
    g_assert_true(g_ptr_array_index(tasks, 0) == task2);
    g_assert_cmpint(task1->tags->len, ==, 0);
    g_assert_cmpint(task2->tags->len, ==, 1);

    // untag tag from second (and last)
    result = task_manager_untag_task(manager, task2_id, tag, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    const GPtrArray *empty_tasks = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_null(empty_tasks);
    g_assert_cmpint(task1->tags->len, ==, 0);
    g_assert_cmpint(task2->tags->len, ==, 0);

    // untag untagged
    result = task_manager_untag_task(manager, task1_id, tag, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    // tag again and find this
    result = task_manager_tag_task(manager, task1_id, tag, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    const GPtrArray *new_tasks = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_nonnull(new_tasks);
    g_assert_cmpint(new_tasks->len, ==, 1);
    g_assert_cmpint(task1->tags->len, ==, 1);
    g_assert_true(new_tasks->pdata[0] == task1);

    // untag unused tag
    const gchar *unused_tag = "unused_tag";
    result = task_manager_untag_task(manager, task1_id, unused_tag, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    task_manager_free(manager);
}

static void test_invalid_untag(void)
{
    GError *error = NULL;

    const gchar *title = "cmd";
    gint priority = 1;

    const gchar *tag = "tag";
    const gchar *empty = "";
    const gchar *camel_case = "Tag";
    const gchar *space = "t a g";

    gboolean result = TRUE;

    TaskManager *manager = task_manager_new();
    guint task_id = task_manager_add_task(manager, title, priority, &error);
    g_assert_no_error(error);
    g_assert_true(task_id != 0);

    result = task_manager_untag_task(manager, task_id, empty, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);

    result = task_manager_untag_task(manager, task_id, camel_case, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);

    result = task_manager_untag_task(manager, task_id, space, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_INVALID_TAG);
    g_clear_error(&error);
    g_assert_false(result);

    result = task_manager_untag_task(manager, 999, tag, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(result);

    result = task_manager_untag_task(manager, task_id, space, NULL);
    g_assert_false(result);

    task_manager_free(manager);
}


static void test_delete_by_valid_id(void)
{
    GError *error = NULL;
    const gchar *tag = "tag";
    const gchar *second_tag = "tag2";
    const gchar *title1 = "cmd";
    gint priority1 = 7;
    const gchar *title2 = "set";
    gint priority2 = 5;
    gboolean result = FALSE;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    // first task
    guint task1_id = task_manager_add_task(manager, title1, priority1, &error);
    task_manager_tag_task(manager, task1_id, tag, &error);
    task_manager_tag_task(manager, task1_id, second_tag, &error);

    // second task
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    const Task *task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    task_manager_tag_task(manager, task2_id, tag, &error);
    g_assert_no_error(error);

    // delete task with 2 tags
    result = task_manager_delete_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    const GPtrArray *unique_tag = task_manager_find_tasks_by_tag(manager, second_tag, &error);
    g_assert_no_error(error);
    g_assert_null(unique_tag);

    const GPtrArray *default_tag = task_manager_find_tasks_by_tag(manager, tag, &error);
    g_assert_no_error(error);
    g_assert_cmpint(default_tag->len, ==, 1);
    g_assert_true(g_ptr_array_index(default_tag, 0) == task2);

    GPtrArray *tasks_after_delete = task_manager_top_tasks(manager, 1);
    g_assert_nonnull(tasks_after_delete);
    g_assert_cmpuint(tasks_after_delete->len, ==, 1);
    g_assert_cmpuint(((Task *)g_ptr_array_index(tasks_after_delete, 0))->id, ==, task2_id);
    g_ptr_array_unref(tasks_after_delete);

    // delete deleted task id
    result = task_manager_delete_task(manager, task1_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(result);

    // delete unknown task id
    result = task_manager_delete_task(manager, 999, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(result);

    // delete unknown task id without error
    result = task_manager_delete_task(manager, 999, NULL);
    g_assert_false(result);

    // new_task_id != delete_task_id
    guint new_task1_id = task_manager_add_task(manager, title1, priority1, &error);
    g_assert_no_error(error);
    g_assert_false(task1_id == new_task1_id);
    g_assert_cmpint(new_task1_id, ==, 3);

    GPtrArray *tasks_after_adding = task_manager_top_tasks(manager, 1);
    g_assert_nonnull(tasks_after_adding);
    g_assert_cmpuint(tasks_after_adding->len, ==, 1);
    g_assert_cmpuint(((Task *)g_ptr_array_index(tasks_after_adding, 0))->id, ==, new_task1_id);
    g_ptr_array_unref(tasks_after_adding);

    task_manager_free(manager);
}

static void test_top(void)
{
    GError *error = NULL;
    const gchar *title1 = "cmd1";
    const gchar *title2 = "cmd2";
    const gchar *title3 = "cmd3";
    const gchar *title4 = "cmd4";
    
    gint priority7 = 7;
    gint priority5 = 5;
    gint priority2 = 2;
    
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task1_id = task_manager_add_task(manager, title1, priority5, &error);
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    guint task3_id = task_manager_add_task(manager, title3, priority2, &error);
    guint task4_id = task_manager_add_task(manager, title4, priority7, &error);
    g_assert_no_error(error);

    GPtrArray *all_tasks = task_manager_top_tasks(manager, 4);
    g_assert_nonnull(all_tasks);
    g_assert_cmpint(all_tasks->len, ==, 4);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(all_tasks, 0))->id, ==, task4_id);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(all_tasks, 1))->id, ==, task1_id);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(all_tasks, 2))->id, ==, task2_id);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(all_tasks, 3))->id, ==, task3_id);

    g_ptr_array_unref(all_tasks);

    GPtrArray *all_tasks_again = task_manager_top_tasks(manager, 1000);
    g_assert_nonnull(all_tasks_again);
    g_assert_cmpint(all_tasks_again->len, ==, 4);
    g_ptr_array_unref(all_tasks_again);

    GPtrArray *first_tasks = task_manager_top_tasks(manager, 1);
    g_assert_nonnull(first_tasks);
    g_assert_cmpint(first_tasks->len, ==, 1);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(first_tasks, 0))->id, ==, task4_id);
    g_ptr_array_unref(first_tasks);

    GPtrArray *empty_tasks = task_manager_top_tasks(manager, 0);
    g_assert_nonnull(empty_tasks);
    g_assert_cmpint(empty_tasks->len, ==, 0);
    g_ptr_array_unref(empty_tasks);

    task_manager_free(manager);
}

static void test_run(void)
{
    GError *error = NULL;
    const gchar *title1 = "cmd1";
    const gchar *title2 = "cmd2";
    const gchar *title3 = "cmd3";
    
    gint priority5 = 5;
    gint priority2 = 2;
    
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    Task *empty_queue = task_manager_run_next(manager);
    g_assert_null(empty_queue);
    g_assert_no_error(error);

    guint task1_id = task_manager_add_task(manager, title1, priority5, &error);
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);
    guint task3_id = task_manager_add_task(manager, title3, priority2, &error);

    g_assert_no_error(error);

    gboolean result = FALSE;
    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task2_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task3_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    Task *next_task = NULL;
    GPtrArray *top_tasks = NULL;

    // run task1
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(next_task->id, ==, task1_id);

    const Task *not_founded_task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(not_founded_task1);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 2);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(top_tasks, 0))->id, !=, task1_id);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(top_tasks, 1))->id, !=, task1_id);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // run task2
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(next_task->id, ==, task2_id);

    const Task *not_founded_task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(not_founded_task2);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 1);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(top_tasks, 0))->id, !=, task2_id);
    
    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // run task3
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(next_task->id, ==, task3_id);

    const Task *not_founded_task3 = task_manager_find_task_by_id(manager, task3_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(not_founded_task3);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    task_manager_free(manager);
}

static void test_run_duplicated(void)
{
    GError *error = NULL;
    const gchar *title1 = "cmd1";
    const gchar *title2 = "cmd2";
    
    gint priority5 = 5;
    gint priority2 = 2;
    
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task1_id = task_manager_add_task(manager, title1, priority5, &error);
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);

    g_assert_no_error(error);

    gboolean result = FALSE;
    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task2_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    Task *next_task = NULL;
    GPtrArray *top_tasks = NULL;

    // run task1
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(next_task->id, ==, task1_id);

    const Task *not_founded_task1 = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(not_founded_task1);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 1);
    g_assert_cmpuint(((const Task *)g_ptr_array_index(top_tasks, 0))->id, !=, task1_id);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // run task2
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(next_task->id, ==, task2_id);

    const Task *not_founded_task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(not_founded_task2);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    // run epmty
    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_null(next_task);

    const Task *not_founded_task1_again = task_manager_find_task_by_id(manager, task1_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(not_founded_task1_again);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_manager_free(manager);
}

static void test_run_with_delete(void)
{
    GError *error = NULL;
    const gchar *title1 = "cmd1";
    const gchar *title2 = "cmd2";
    
    gint priority5 = 5;
    gint priority2 = 2;
    
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    guint task1_id = task_manager_add_task(manager, title1, priority5, &error);
    guint task2_id = task_manager_add_task(manager, title2, priority2, &error);

    g_assert_no_error(error);

    gboolean result = FALSE;
    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task2_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    result = task_manager_enqueue_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    Task *next_task = NULL;
    GPtrArray *top_tasks = NULL;

    // delete task1 and run next - get task2
    result = task_manager_delete_task(manager, task1_id, &error);
    g_assert_no_error(error);
    g_assert_true(result);

    next_task = task_manager_run_next(manager);
    g_assert_no_error(error);
    g_assert_cmpuint(next_task->id, ==, task2_id);

    const Task *not_founded_task2 = task_manager_find_task_by_id(manager, task2_id, &error);
    g_assert_error(error, TM_ERROR, TM_ERROR_TASK_NOT_FOUND);
    g_clear_error(&error);
    g_assert_null(not_founded_task2);

    top_tasks = task_manager_top_tasks(manager, 3);
    g_assert_nonnull(top_tasks);
    g_assert_cmpuint(top_tasks->len, ==, 0);

    g_ptr_array_unref(top_tasks);
    task_free(next_task);

    task_manager_free(manager);
}

static void test_history(void)
{   
    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    GPtrArray *result_no_history = task_manager_get_history(manager);
    g_assert_nonnull(result_no_history);
    g_assert_cmpuint(result_no_history->len, ==, 0);
    g_ptr_array_unref(result_no_history);

    task_manager_record_command(manager, "");
    GPtrArray *result_add_empty = task_manager_get_history(manager);
    g_assert_nonnull(result_add_empty);
    g_assert_cmpuint(result_add_empty->len, ==, 0);
    g_ptr_array_unref(result_add_empty);

    
    task_manager_record_command(manager, "cmd1");
    task_manager_record_command(manager, "cmd2");
    task_manager_record_command(manager, "cmd3");
    task_manager_record_command(manager, "cmd4");
    task_manager_record_command(manager, "cmd5");
    task_manager_record_command(manager, "cmd6");

    GPtrArray *result_add_6 = task_manager_get_history(manager);
    g_assert_nonnull(result_add_6);
    g_assert_cmpuint(result_add_6->len, ==, 6);
    g_assert_cmpstr(g_ptr_array_index(result_add_6, 0), ==, "cmd1");
    g_assert_cmpstr(g_ptr_array_index(result_add_6, 1), ==, "cmd2");
    g_assert_cmpstr(g_ptr_array_index(result_add_6, 2), ==, "cmd3");
    g_assert_cmpstr(g_ptr_array_index(result_add_6, 3), ==, "cmd4");
    g_assert_cmpstr(g_ptr_array_index(result_add_6, 4), ==, "cmd5");
    g_assert_cmpstr(g_ptr_array_index(result_add_6, 5), ==, "cmd6");
    g_ptr_array_unref(result_add_6);

    task_manager_record_command(manager, "cmd7");
    task_manager_record_command(manager, "cmd8");
    task_manager_record_command(manager, "cmd9");
    task_manager_record_command(manager, "cmd10");
    task_manager_record_command(manager, "cmd11");
    
    GPtrArray *result_add_10 = task_manager_get_history(manager);
    g_assert_nonnull(result_add_10);
    g_assert_cmpuint(result_add_10->len, ==, 10);
    g_assert_cmpstr(g_ptr_array_index(result_add_10, 0), ==, "cmd2");
    g_assert_cmpstr(g_ptr_array_index(result_add_10, 9), ==, "cmd11");

    task_manager_record_command(manager, "cmd12");
    task_manager_record_command(manager, "cmd13");
    task_manager_record_command(manager, "cmd14");

    g_assert_nonnull(result_add_10);
    g_assert_cmpuint(result_add_10->len, ==, 10);
    g_assert_cmpstr(g_ptr_array_index(result_add_10, 0), ==, "cmd2");
    g_assert_cmpstr(g_ptr_array_index(result_add_10, 9), ==, "cmd11");
    g_ptr_array_unref(result_add_10);

    GPtrArray *result_add_10_new = task_manager_get_history(manager);
    g_assert_nonnull(result_add_10_new);
    g_assert_cmpuint(result_add_10_new->len, ==, 10);
    g_assert_cmpstr(g_ptr_array_index(result_add_10_new, 0), ==, "cmd5");
    g_assert_cmpstr(g_ptr_array_index(result_add_10_new, 9), ==, "cmd14");
    g_ptr_array_unref(result_add_10_new);

    task_manager_free(manager);
}

static void test_stats(void)
{   
    GError *error = NULL;

    TaskManager *manager = task_manager_new();
    g_assert_nonnull(manager);

    TaskManagerStats statistics = {0};

    // no tasks
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 0);
    g_assert_cmpuint(statistics.tasks, ==, 0);
    g_assert_cmpuint(statistics.unique_tags, ==, 0);

    // 2 tasks, 2 tag
    guint task1_id = task_manager_add_task(manager, "cmd1", 7, &error);
    guint task2_id = task_manager_add_task(manager, "cmd2", 3, &error);

    task_manager_tag_task(manager, task1_id, "tag1", &error);
    task_manager_tag_task(manager, task1_id, "tag2", &error);
    task_manager_tag_task(manager, task2_id, "tag2", &error);

    g_assert_no_error(error);

    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 0);
    g_assert_cmpuint(statistics.tasks, ==, 2);
    g_assert_cmpuint(statistics.unique_tags, ==, 2);

    // enqueue one task 2 times -> queue size 2
    task_manager_enqueue_task(manager, task1_id, &error);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 1);
    g_assert_cmpuint(statistics.tasks, ==, 2);
    g_assert_cmpuint(statistics.unique_tags, ==, 2);

    task_manager_enqueue_task(manager, task1_id, &error);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 2);
    g_assert_cmpuint(statistics.tasks, ==, 2);
    g_assert_cmpuint(statistics.unique_tags, ==, 2);

    // untag task and unique tags = 1
    task_manager_untag_task(manager, task1_id, "tag1", &error);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 2);
    g_assert_cmpuint(statistics.tasks, ==, 2);
    g_assert_cmpuint(statistics.unique_tags, ==, 1);
    
    // del and run task
    Task *task = task_manager_run_next(manager);
    task_free(task);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 0);
    g_assert_cmpuint(statistics.tasks, ==, 1);
    g_assert_cmpuint(statistics.unique_tags, ==, 1);

    task_manager_delete_task(manager, task2_id, &error);
    statistics = task_manager_get_stats(manager);
    g_assert_cmpuint(statistics.queue_length, ==, 0);
    g_assert_cmpuint(statistics.tasks, ==, 0);
    g_assert_cmpuint(statistics.unique_tags, ==, 0);

    task_manager_free(manager);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/task-manager/add-find", test_add_find);
    g_test_add_func("/task-manager/invalid-priority", test_invalid_priority);
    g_test_add_func("/task-manager/not-found", test_not_found);

    g_test_add_func("/task-manager/tag/valid", test_tag);
    g_test_add_func("/task-manager/tag/invalid", test_invalid_tag);
    g_test_add_func("/task-manager/tag/find-valid", test_find_by_tag);
    g_test_add_func("/task-manager/tag/find-invalid", test_find_by_invalid_tag);
    
    g_test_add_func("/task-manager/untag/valid", test_untag);
    g_test_add_func("/task-manager/untag/invalid", test_invalid_untag);

    g_test_add_func("/task-manager/tag/delete", test_delete_by_valid_id);

    g_test_add_func("/task-manager/top", test_top);

    g_test_add_func("/task-manager/run", test_run);
    g_test_add_func("/task-manager/run/duplicated", test_run_duplicated);
    g_test_add_func("/task-manager/run/with-delete", test_run_with_delete);

    g_test_add_func("/task-manager/history", test_history);

    g_test_add_func("/task-manager/stats", test_stats);

    return g_test_run();
}
