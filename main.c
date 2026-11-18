#include <unistd.h>

#include "cli.h"

int main(void) {
    gint exit_code = 0;
    TaskManager *manager = task_manager_new();
    GString *output = g_string_new(NULL);
    GIOChannel *channel = g_io_channel_unix_new(STDIN_FILENO);

    while(TRUE) {
        gchar *line = NULL;
        GError *error = NULL;
        GIOStatus read_status = g_io_channel_read_line(channel, &line, NULL, NULL, &error);

        switch (read_status)
        {
        case G_IO_STATUS_NORMAL: {
            g_string_truncate(output, 0);
            CliStatus cli_status = cli_execute_line(manager, line, output, &error);
            g_free(line);

            g_print("%s", output->str);

            if (error != NULL) {
                g_printerr("error: %s\n", error->message);
                g_clear_error(&error);
            }

            if (cli_status == CLI_QUIT) {
                goto cleanup;    
            }

            break;
        }
        case G_IO_STATUS_EOF:
            goto cleanup;
        case G_IO_STATUS_ERROR:
            g_printerr("error: %s\n", error->message);
            g_clear_error(&error);
            exit_code = 1;
            goto cleanup;
        default:
            break;
        }
    }

cleanup:
    g_io_channel_unref(channel);
    g_string_free(output, TRUE);
    task_manager_free(manager);

    return exit_code;
}
