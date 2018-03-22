/* ide-application-tests.c
 *
 * Copyright © 2016 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-application-tests"

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "ide-debug.h"

#include "application/ide-application-private.h"
#include "application/ide-application-tests.h"

typedef struct
{
  IdeApplication                *self;
  gchar                         *name;
  gchar                        **required_plugins;
  IdeApplicationTest             test_func;
  IdeApplicationTestCompletion   test_completion;
} AsyncTest;

static void ide_application_run_next_test (IdeApplication *self);

static gboolean
fatal_log_handler (const gchar    *log_domain,
                   GLogLevelFlags  log_level,
                   const gchar    *message,
                   gpointer        user_data)
{
  if ((g_strcmp0 (log_domain, "Devhelp") == 0) ||
      (g_strcmp0 (log_domain, "Gtk") == 0))
    return FALSE;

  return TRUE;
}

static void
ide_application_run_tests_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  AsyncTest *test = user_data;
  g_autoptr(GError) error = NULL;
  gboolean ret;

  ret = test->test_completion (result, &error);

  if (error != NULL)
    g_error ("%s: %s", g_quark_to_string (error->domain), error->message);

  if (ret == FALSE)
    g_error ("Test failed without an error");

  if (test->self->test_funcs)
    ide_application_run_next_test (test->self);
  else
    g_application_release (G_APPLICATION (test->self));

  g_clear_pointer (&test->name, g_free);
  g_clear_object (&test->self);
  g_slice_free (AsyncTest, test);
}

static void
ide_application_tests_update_plugins (IdeApplication *self,
                                      AsyncTest      *test)
{
  g_auto(GStrv) loaded = NULL;
  PeasEngine *engine;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (test != NULL);

  engine = peas_engine_get_default ();
  loaded = peas_engine_get_loaded_plugins (engine);

  if (loaded != NULL)
    {
      for (guint i = 0; loaded[i]; i++)
        {
          PeasPluginInfo *info = peas_engine_get_plugin_info (engine, loaded[i]);

          g_debug ("Unloading plugin %s", loaded[i]);
          peas_engine_unload_plugin (engine, info);
        }
    }

  if (test->required_plugins != NULL)
    {
      for (guint i = 0; test->required_plugins[i]; i++)
        {
          PeasPluginInfo *info = peas_engine_get_plugin_info (engine, test->required_plugins[i]);

          g_debug ("Loading plugin %s [%p] for test", test->required_plugins[i], info);

          if (info == NULL)
            g_error ("No such plugin %s", test->required_plugins[i]);

          peas_engine_load_plugin (engine, info);
        }
    }
}

static void
ide_application_run_next_test (IdeApplication *self)
{
  g_autoptr(GCancellable) cancellable = NULL;
  AsyncTest *test;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  cancellable = g_cancellable_new ();

  test = self->test_funcs->data;
  test->self = g_object_ref (self);

  /* ensure proper plugins are loaded */
  ide_application_tests_update_plugins (self, test);

  /* run the async test */
  test->test_func (cancellable, ide_application_run_tests_cb, test);

  /* remove it from our todo list. this is safe to do after calling
   * our async func because the finish cannot be called until we
   * return from this function and yield to the main loop.
   */
  self->test_funcs = g_list_delete_link (self->test_funcs, self->test_funcs);

  IDE_EXIT;
}

void
ide_application_run_tests (IdeApplication *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  if (self->test_funcs != NULL)
    {
      g_test_log_set_fatal_handler (fatal_log_handler, NULL);
      g_application_hold (G_APPLICATION (self));
      ide_application_run_next_test (self);
    }

  IDE_EXIT;
}

static gboolean
ide_application_task_completion (GAsyncResult  *result,
                                 GError       **error)
{
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
ide_application_add_test (IdeApplication               *self,
                          const gchar                  *test_name,
                          IdeApplicationTest            test_func,
                          IdeApplicationTestCompletion  test_completion,
                          const gchar * const          *required_plugins)
{
  AsyncTest *test;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (test_name != NULL);
  g_return_if_fail (test_func != NULL);

  if (test_completion == NULL)
    test_completion = ide_application_task_completion;

  test = g_slice_new0 (AsyncTest);
  test->name = g_strdup (test_name);
  test->test_func = test_func;
  test->test_completion = test_completion;
  test->required_plugins = g_strdupv ((gchar **)required_plugins);

  self->test_funcs = g_list_append (self->test_funcs, test);

  IDE_EXIT;
}
