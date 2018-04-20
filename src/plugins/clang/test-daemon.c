#include "config.h"

#include <gio/gio.h>
#include <jsonrpc-glib.h>
#include <stdlib.h>

typedef void (*TestFunc) (JsonrpcClient *client,
                          GTask         *task);

static void tick_tests      (JsonrpcClient *client);
static void test_diagnose   (JsonrpcClient *client,
                             GTask         *task);
static void test_index_file (JsonrpcClient *client,
                             GTask         *task);

static gchar **flags;
static const gchar *path;
static GMainLoop *main_loop;
static TestFunc test_funcs[] = {
  test_diagnose,
  test_index_file,
};

static void
test_index_file_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_error ("index-file: %s", error->message);

  g_printerr ("index-file: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_index_file (JsonrpcClient *client,
                 GTask         *task)
{
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV ((const gchar * const *)flags)
  );

  jsonrpc_client_call_async (client,
                             "clang/indexFile",
                             params,
                             NULL,
                             test_index_file_cb,
                             g_object_ref (task));
}

static void
test_diagnose_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_error ("diagnose: %s", error->message);

  g_printerr ("diagnose: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_diagnose (JsonrpcClient *client,
               GTask         *task)
{
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV ((const gchar * const *)flags)
  );

  jsonrpc_client_call_async (client,
                             "clang/diagnose",
                             params,
                             NULL,
                             test_diagnose_cb,
                             g_object_ref (task));
}

static void
finished_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (G_IS_TASK (result));

  if (!g_task_propagate_boolean (G_TASK (result), &error))
    g_error ("%s", error->message);

  tick_tests (client);
}

static void
tick_tests (JsonrpcClient *client)
{
  g_autoptr(GTask) task = NULL;
  static guint test_pos = 0;

  if (test_pos >= G_N_ELEMENTS (test_funcs))
    {
      g_main_loop_quit (main_loop);
      return;
    }

  task = g_task_new (client, NULL, finished_cb, NULL);
  test_funcs[test_pos++] (client, task);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(JsonrpcClient) client = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GError) error = NULL;

  if (argc != 4)
    {
      g_printerr ("usage: %s path-to-daemon source-file build-flags\n", argv[0]);
      return EXIT_FAILURE;
    }

  subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                                 &error,
                                 argv[1],
                                 NULL);

  if (subprocess == NULL)
    {
      g_printerr ("Failed to spawn daemon: %s\n", error->message);
      return EXIT_FAILURE;
    }

  path = argv[2];

  if (!g_shell_parse_argv (argv[3], NULL, &flags, NULL))
    flags = NULL;

  main_loop = g_main_loop_new (NULL, FALSE);

  stream = g_simple_io_stream_new (g_subprocess_get_stdout_pipe (subprocess),
                                   g_subprocess_get_stdin_pipe (subprocess));

  client = jsonrpc_client_new (stream);

  tick_tests (client);

  g_subprocess_wait_async (subprocess, NULL, NULL, NULL);

  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}
