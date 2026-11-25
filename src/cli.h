#pragma once

#include "task_manager.h"

#include <glib.h>

typedef enum _CliStatus CliStatus;
typedef enum _CliError CliError;

enum _CliStatus
{
    CLI_CONTINUE,
    CLI_QUIT,
};

enum _CliError
{
    CLI_ERROR_UNKNOWN_COMMAND,
    CLI_ERROR_USAGE,
};

#define CLI_ERROR (cli_error_quark())
/**
 * cli_error_quark:
 *
 * Returns: the error domain for CLI syntax and command errors
 */
GQuark cli_error_quark(void);

/**
 * cli_execute_line:
 * @manager: a task manager
 * @line: (nullable): command line to parse; %NULL is ignored
 * @output: output buffer to append successful command results to
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Parses and executes one command. If @error is provided, it must point to
 * %NULL. Syntax errors use #CliError; task manager errors are propagated.
 * Non-empty commands are recorded before execution, except history and quit.
 *
 * Returns: %CLI_QUIT for quit, otherwise %CLI_CONTINUE
 */
CliStatus cli_execute_line(TaskManager *manager, const gchar *line, GString *output, GError **error);
