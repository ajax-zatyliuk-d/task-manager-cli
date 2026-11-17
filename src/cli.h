#pragma once

#include "task_manager.h"

typedef enum _CliStatus CliStatus;
typedef enum _CliError CliError;

enum _CliStatus {
    CLI_CONTINUE,
    CLI_QUIT,
};

enum _CliError {
    CLI_ERROR_UNKNOWN_COMMAND,
    CLI_ERROR_USAGE,
};

#define CLI_ERROR (cli_error_quark())
GQuark cli_error_quark(void);

CliStatus cli_execute_line(TaskManager *manager, const gchar *line, GString *output, GError **error);
