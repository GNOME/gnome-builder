/* ide-workbench-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-workbench-addin"

#include "ide-workbench-addin.h"

G_DEFINE_INTERFACE (IdeWorkbenchAddin, ide_workbench_addin, G_TYPE_OBJECT)

static void
ide_workbench_addin_real_load (IdeWorkbenchAddin *self,
                               IdeWorkbench      *workbench)
{
}

static void
ide_workbench_addin_real_unload (IdeWorkbenchAddin *self,
                                 IdeWorkbench      *workbench)
{
}

static gboolean
ide_workbench_addin_real_can_open (IdeWorkbenchAddin *self,
                                   IdeUri            *uri,
                                   const gchar       *content_type,
                                   gint              *priority)
{
  *priority = 0;
  return FALSE;
}

static void
ide_workbench_addin_default_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = ide_workbench_addin_real_load;
  iface->unload = ide_workbench_addin_real_unload;
  iface->can_open = ide_workbench_addin_real_can_open;
}

/**
 * ide_workbench_addin_load:
 * @self: An #IdeWorkbenchAddin
 * @workbench: An #IdeWorkbench
 *
 * This interface method is called to load @self. Addin implementations should add any
 * required UI or actions to @workbench here. You should remove anything you've added
 * in ide_workbench_addin_unload(), as that will be called when your plugin is deactivated
 * or the workbench is in the destruction process.
 */
void
ide_workbench_addin_load (IdeWorkbenchAddin *self,
                          IdeWorkbench      *workbench)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->load (self, workbench);
}

/**
 * ide_workbench_addin_unload:
 * @self: An #IdeWorkbenchAddin
 * @workbench: An #IdeWorkbench
 *
 * This interface method should cleanup after anything added to @workbench in
 * ide_workbench_addin_load().
 *
 * This might be called when a plugin is deactivated, or the workbench is in the
 * destruction process.
 */
void
ide_workbench_addin_unload (IdeWorkbenchAddin *self,
                            IdeWorkbench      *workbench)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->unload (self, workbench);
}

/**
 * ide_workbench_addin_can_open:
 * @self: An #IdeWorkbenchAddin.
 * @uri: An #IdeUri.
 * @content_type: (nullable): A content-type or %NULL.
 * @priority: (out): the priority at which this loader should be used.
 *
 * This interface method indicates if the workbench addin can load the content
 * found at @uri. If so, @priority should be set to an integer priority
 * indicating how important it is for this addin to load @uri.
 *
 * The lowest integer value wins. However, a load fails, the next addin which
 * returned %TRUE from this method will be consulted.
 *
 * Returns: %TRUE if @self and open @uri.
 */
gboolean
ide_workbench_addin_can_open (IdeWorkbenchAddin *self,
                              IdeUri            *uri,
                              const gchar       *content_type,
                              gint              *priority)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_ADDIN (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (priority != NULL, FALSE);

  return IDE_WORKBENCH_ADDIN_GET_IFACE (self)->can_open (self, uri, content_type, priority);
}

void
ide_workbench_addin_open_async (IdeWorkbenchAddin   *self,
                                IdeUri              *uri,
                                const gchar         *content_type,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (uri != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if ((IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_async == NULL) ||
      (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_finish == NULL))
    g_return_if_reached ();

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_async (self, uri, content_type, cancellable, callback, user_data);
}

gboolean
ide_workbench_addin_open_finish (IdeWorkbenchAddin  *self,
                                 GAsyncResult       *result,
                                 GError            **error)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_finish == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "%s does not contain open_finish",
                   G_OBJECT_TYPE_NAME (self));
      return FALSE;
    }

  return IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_finish (self, result, error);
}
