/* ide-context-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-context-addin"

#include "config.h"

#include "ide-context-addin.h"

G_DEFINE_INTERFACE (IdeContextAddin, ide_context_addin, G_TYPE_OBJECT)

enum {
  PROJECT_LOADED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_context_addin_real_load_project_async (IdeContextAddin     *addin,
                                           IdeContext          *context,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_autoptr(GTask) task = g_task_new (addin, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_context_addin_real_load_project_finish (IdeContextAddin  *addin,
                                            GAsyncResult     *result,
                                            GError          **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_context_addin_default_init (IdeContextAddinInterface *iface)
{
  iface->load_project_async = ide_context_addin_real_load_project_async;
  iface->load_project_finish = ide_context_addin_real_load_project_finish;

  /**
   * IdeContextAddin::project-loaded:
   * @self: an #IdeContextAddin
   * @context: an #IdeContext
   *
   * The "project-loaded" signal is emitted after a project has been loaded
   * in the #IdeContext.
   *
   * You might use this to setup any runtime features that rely on the project
   * being successfully loaded first. Every addin's
   * ide_context_addin_load_project_async() will have been called and completed
   * before this signal is emitted.
   *
   * Since: 3.32
   */
  signals [PROJECT_LOADED] =
    g_signal_new ("project-loaded",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeContextAddinInterface, project_loaded),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_CONTEXT);
  g_signal_set_va_marshaller (signals [PROJECT_LOADED],
                              G_TYPE_FROM_INTERFACE (iface),
                              g_cclosure_marshal_VOID__OBJECTv);
}

/**
 * ide_context_addin_load_project_async:
 * @self: an #IdeContextAddin
 * @context: an #IdeContext
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Requests to load a project with the #IdeContextAddin.
 *
 * This function is called when the #IdeContext requests loading a project.
 *
 * Since: 3.32
 */
void
ide_context_addin_load_project_async (IdeContextAddin     *self,
                                      IdeContext          *context,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_return_if_fail (IDE_IS_CONTEXT_ADDIN (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CONTEXT_ADDIN_GET_IFACE (self)->load_project_async (self, context, cancellable, callback, user_data);
}

/**
 * ide_context_addin_load_project_finish:
 * @self: an #IdeContextAddin
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes a request to load a project with the #IdeContextAddin.
 *
 * This function will be called from the callback provided to
 * ide_context_addin_load_project_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.32
 */
gboolean
ide_context_addin_load_project_finish (IdeContextAddin  *self,
                                       GAsyncResult     *result,
                                       GError          **error)
{
  g_return_val_if_fail (IDE_IS_CONTEXT_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_CONTEXT_ADDIN_GET_IFACE (self)->load_project_finish (self, result, error);
}

/**
 * ide_context_addin_load:
 * @self: an #IdeContextAddin
 * @context: an #IdeContext
 *
 * Requests that the #IdeContextAddin loads any necessary runtime features.
 *
 * This is called when the #IdeContext is created. If you would rather wait
 * until a project is loaded, then use #IdeContextAddin::project-loaded to
 * load runtime features.
 *
 * Since: 3.32
 */
void
ide_context_addin_load (IdeContextAddin *self,
                        IdeContext      *context)
{
  g_return_if_fail (IDE_IS_CONTEXT_ADDIN (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  if (IDE_CONTEXT_ADDIN_GET_IFACE (self)->load)
    IDE_CONTEXT_ADDIN_GET_IFACE (self)->load (self, context);
}

/**
 * ide_context_addin_unload:
 * @self: an #IdeContextAddin
 * @context: an #IdeContext
 *
 * Requests that the #IdeContextAddin unloads any previously loaded
 * resources.
 *
 * Since: 3.32
 */
void
ide_context_addin_unload (IdeContextAddin *self,
                          IdeContext      *context)
{
  g_return_if_fail (IDE_IS_CONTEXT_ADDIN (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  if (IDE_CONTEXT_ADDIN_GET_IFACE (self)->unload)
    IDE_CONTEXT_ADDIN_GET_IFACE (self)->unload (self, context);
}

/**
 * ide_context_addin_project_loaded:
 * @self: an #IdeContextAddin
 * @context: an #IdeContext
 *
 * Emits the #IdeContextAddin::project-loaded signal.
 *
 * This is called when the context has completed loading a project.
 *
 * Since: 3.32
 */
void
ide_context_addin_project_loaded (IdeContextAddin *self,
                                  IdeContext      *context)
{
  g_return_if_fail (IDE_IS_CONTEXT_ADDIN (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  g_signal_emit (self, signals [PROJECT_LOADED], 0, context);
}
