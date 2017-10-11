/* ide-workbench-addin.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#include "workbench/ide-workbench-addin.h"
#include "workbench/ide-workbench-private.h"

G_DEFINE_INTERFACE (IdeWorkbenchAddin, ide_workbench_addin, G_TYPE_OBJECT)

static void
ide_workbench_addin_real_perspective_set (IdeWorkbenchAddin *self,
                                          IdePerspective    *perspective)
{
}

static gchar *
ide_workbench_addin_real_get_id (IdeWorkbenchAddin *self)
{
  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

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
  iface->can_open = ide_workbench_addin_real_can_open;
  iface->get_id = ide_workbench_addin_real_get_id;
  iface->load = ide_workbench_addin_real_load;
  iface->unload = ide_workbench_addin_real_unload;
  iface->perspective_set = ide_workbench_addin_real_perspective_set;
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
ide_workbench_addin_open_async (IdeWorkbenchAddin     *self,
                                IdeUri                *uri,
                                const gchar           *content_type,
                                IdeWorkbenchOpenFlags  flags,
                                GCancellable          *cancellable,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (uri != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if ((IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_async == NULL) ||
      (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_finish == NULL))
    g_return_if_reached ();

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_async (self, uri, content_type, flags, cancellable, callback, user_data);
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

/**
 * ide_workbench_addin_get_id:
 * @self: An #IdeWorkbenchAddin.
 *
 * Gets the identifier for this workbench addin. By default this is the
 * name of the classes GType (such as "MyObject").
 *
 * This can be used as the hint to various open operations in IdeWorkbench
 * to prefer a given loader.
 *
 * Returns: (transfer full): a newly allocated string.
 */
gchar *
ide_workbench_addin_get_id (IdeWorkbenchAddin *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_ADDIN (self), NULL);

  return IDE_WORKBENCH_ADDIN_GET_IFACE (self)->get_id (self);
}

/**
 * ide_workbench_addin_perspective_set:
 * @self: an #IdeWorkbenchAddin
 * @perspective: An #IdePerspective
 *
 * This function is called when the workbench changes the perspective.
 *
 * Addins that wish to add buttons to the header bar may want to show or
 * hide the widgets in this vfunc.
 */
void
ide_workbench_addin_perspective_set (IdeWorkbenchAddin *self,
                                     IdePerspective    *perspective)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_PERSPECTIVE (perspective));

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->perspective_set (self, perspective);
}

/**
 * ide_workbench_addin_find_by_module_name:
 * @workbench: a #IdeWorkbench
 * @addin_name: the name of the addin
 *
 * This locates a loaded #IdeWorkbenchAddin based on the module name.
 * If the module is missing or has not been loaded, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeWorkbenchAddin or %NULL
 *
 * Since: 3.26
 */
IdeWorkbenchAddin *
ide_workbench_addin_find_by_module_name (IdeWorkbench *workbench,
                                         const gchar  *module_name)
{
  PeasPluginInfo *plugin_info;
  PeasExtension *exten = NULL;

  g_return_val_if_fail (IDE_IS_WORKBENCH (workbench), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), module_name);
  if (plugin_info != NULL)
    exten = peas_extension_set_get_extension (workbench->addins, plugin_info);

  return exten ? IDE_WORKBENCH_ADDIN (exten) : NULL;
}
