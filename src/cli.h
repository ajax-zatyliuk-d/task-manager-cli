#pragma once

#include "task_manager.h"

#include <glib.h>

typedef enum _CliStatus CliStatus;
typedef enum _CliError CliError;
typedef struct _Cli Cli;

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
GQuark cli_error_quark(void);

Cli *cli_new(GError **error);
void cli_free(Cli *cli);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Cli, cli_free);

CliStatus cli_execute_line(Cli *cli, TaskManager *manager, const gchar *command_line, GString *output, GError **error);
