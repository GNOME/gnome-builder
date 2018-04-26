#include "config.h"

#include <gio/gio.h>
#include <jsonrpc-glib.h>
#include <stdlib.h>

typedef void (*TestFunc) (JsonrpcClient *client,
                          GTask         *task);

static void tick_tests      (JsonrpcClient *client);
static void test_initialize (JsonrpcClient *client,
                             GTask         *task);
static void test_complete   (JsonrpcClient *client,
                             GTask         *task);
static void test_find_scope (JsonrpcClient *client,
                             GTask         *task);
static void test_diagnose   (JsonrpcClient *client,
                             GTask         *task);
static void test_locate     (JsonrpcClient *client,
                             GTask         *task);
static void test_index_file (JsonrpcClient *client,
                             GTask         *task);
static void test_symtree    (JsonrpcClient *client,
                             GTask         *task);
static void test_highlight  (JsonrpcClient *client,
                             GTask         *task);
static void test_index_key  (JsonrpcClient *client,
                             GTask         *task);

static gchar **flags;
static const gchar *path;
static GMainLoop *main_loop;
static TestFunc test_funcs[] = {
  test_initialize,
  test_complete,
  test_diagnose,
  test_find_scope,
  test_index_file,
  test_locate,
  test_symtree,
  test_highlight,
  test_index_key,
};

static void
test_index_key_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_printerr ("getIndexKey: %s\n", error->message);
  else
    g_printerr ("getIndexKey: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_index_key (JsonrpcClient *client,
                GTask         *task)
{
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV ((const gchar * const *)flags),
    "line", JSONRPC_MESSAGE_PUT_INT64 (5),
    "column", JSONRPC_MESSAGE_PUT_INT64 (5)
  );

  jsonrpc_client_call_async (client,
                             "clang/getIndexKey",
                             params,
                             NULL,
                             test_index_key_cb,
                             g_object_ref (task));
}

static void
test_highlight_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_error ("getHighlightIndex: %s", error->message);

  g_printerr ("getHighlightIndex: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_highlight (JsonrpcClient *client,
                GTask         *task)
{
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV ((const gchar * const *)flags)
  );

  jsonrpc_client_call_async (client,
                             "clang/getHighlightIndex",
                             params,
                             NULL,
                             test_highlight_cb,
                             g_object_ref (task));
}

static void
test_symtree_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_error ("getSymbolTree: %s", error->message);

  g_printerr ("getSymbolTree: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_symtree (JsonrpcClient *client,
              GTask         *task)
{
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV ((const gchar * const *)flags)
  );

  jsonrpc_client_call_async (client,
                             "clang/getSymbolTree",
                             params,
                             NULL,
                             test_symtree_cb,
                             g_object_ref (task));
}

static void
test_locate_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_error ("locate-symbol: %s", error->message);

  g_printerr ("locate-symbol: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_locate (JsonrpcClient *client,
             GTask         *task)
{
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV ((const gchar * const *)flags),
    "line", JSONRPC_MESSAGE_PUT_INT64 (5),
    "column", JSONRPC_MESSAGE_PUT_INT64 (5)
  );

  jsonrpc_client_call_async (client,
                             "clang/locateSymbol",
                             params,
                             NULL,
                             test_locate_cb,
                             g_object_ref (task));
}

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
test_find_scope_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_printerr ("find-nearest-scope: %s\n", error->message);
  else
    g_printerr ("find-nearest-scope: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_find_scope (JsonrpcClient *client,
                 GTask         *task)
{
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV ((const gchar * const *)flags),
    "line", JSONRPC_MESSAGE_PUT_INT64 (5),
    "column", JSONRPC_MESSAGE_PUT_INT64 (3)
  );

  jsonrpc_client_call_async (client,
                             "clang/findNearestScope",
                             params,
                             NULL,
                             test_find_scope_cb,
                             g_object_ref (task));
}

static void
test_complete_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_error ("complete: %s", error->message);

  g_printerr ("complete: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_complete (JsonrpcClient *client,
               GTask         *task)
{
  g_autoptr(GVariant) params = NULL;
  guint line = 0;
  guint column = 0;

  params = JSONRPC_MESSAGE_NEW (
    "path", JSONRPC_MESSAGE_PUT_STRING (path),
    "flags", JSONRPC_MESSAGE_PUT_STRV ((const gchar * const *)flags),
    "line", JSONRPC_MESSAGE_PUT_INT64 (line),
    "column", JSONRPC_MESSAGE_PUT_INT64 (column)
  );

  jsonrpc_client_call_async (client,
                             "clang/complete",
                             params,
                             NULL,
                             test_complete_cb,
                             g_object_ref (task));
}

static void
test_initialize_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    g_error ("initialize: %s", error->message);

  g_printerr ("initialize: %s\n", g_variant_print (reply, TRUE));

  g_task_return_boolean (task, TRUE);
}

static void
test_initialize (JsonrpcClient *client,
                 GTask         *task)
{
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *root = NULL;
  g_autofree gchar *uri = NULL;

  root = g_path_get_dirname (path);
  uri = g_strdup_printf ("file://%s", root);

  params = JSONRPC_MESSAGE_NEW (
    "rootUri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "rootPath", JSONRPC_MESSAGE_PUT_STRING (root),
    "processId", JSONRPC_MESSAGE_PUT_INT64 (getpid ()),
    "capabilities", "{", "}"
  );

  jsonrpc_client_call_async (client,
                             "initialize",
                             params,
                             NULL,
                             test_initialize_cb,
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
#if 0
                                 "gdbserver",
                                 "localhost:8888",
#endif
#if 0
                                 "valgrind",
                                 "--suppressions=glib.supp",
                                 "--leak-check=full",
#endif
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
