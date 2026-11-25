#include "cli.h"
#include "task_manager.h"

#include <glib.h>

#include <termios.h>
#include <unistd.h>

#include <errno.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>

static void setup_terminal(void)
{
    struct termios terminal;
    if (tcgetattr(STDIN_FILENO, &terminal) != 0) {
        return;
    }

    if ((terminal.c_lflag & ICANON) == 0) {
        return;
    }

    terminal.c_iflag |= IUTF8;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &terminal) != 0) {
        perror("tcsetattr");
    }
}

static gboolean execute_and_print_line(TaskManager *manager, const gchar *line, GString *output, CliStatus *status)
{
    g_autoptr(GError) error = NULL;
    g_string_truncate(output, 0);
    *status = cli_execute_line(manager, line, output, &error);

    g_print("%s", output->str);
    // g_print() may already have flushed stdout, so check both the stream and flush errors.
    if (ferror(stdout) || fflush(stdout) != 0) {
        g_printerr("error: stdout: %s\n", g_strerror(errno));
        return FALSE;
    }

    if (error != NULL) {
        g_printerr("error: %s\n", error->message);
    }

    return TRUE;
}

int main(void)
{
    setlocale(LC_CTYPE, "");
    setup_terminal();
    g_autoptr(TaskManager) manager = task_manager_new();
    g_autoptr(GString) output = g_string_new(NULL);
    g_autoptr(GIOChannel) channel = g_io_channel_unix_new(STDIN_FILENO);

    while (TRUE) {
        g_autofree gchar *line = NULL;
        g_autoptr(GError) error = NULL;
        const GIOStatus read_status = g_io_channel_read_line(channel, &line, NULL, NULL, &error);

        switch (read_status) {
            case G_IO_STATUS_NORMAL: {
                CliStatus status;
                if (!execute_and_print_line(manager, line, output, &status)) {
                    return 1;
                }
                if (status == CLI_QUIT) {
                    return 0;
                }

                break;
            }
            case G_IO_STATUS_EOF:
                return 0;
            case G_IO_STATUS_ERROR:
                g_printerr("error: %s\n", error->message);
                return 1;
            case G_IO_STATUS_AGAIN:
                g_printerr("error: nonblocking stdin is not supported\n");
                return 1;
        }
    }
}
