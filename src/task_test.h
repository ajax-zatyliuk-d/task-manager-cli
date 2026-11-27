#pragma once

#include "task.h"

#ifdef TASK_MANAGER_TESTING
void task_set_created_at_for_test(Task *task, gint64 created_at);
#endif
