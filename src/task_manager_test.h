#pragma once

#include "task_manager.h"

#ifdef TASK_MANAGER_TESTING
void task_manager_set_next_id_for_test(TaskManager *manager, guint next_id);
#endif
