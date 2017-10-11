/* ide-project-template.c
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

#define G_LOG_DOMAIN "ide-project-template"

#include "template/ide-project-template.h"

G_DEFINE_INTERFACE (IdeProjectTemplate, ide_project_template, G_TYPE_OBJECT)

static void
ide_project_template_default_init (IdeProjectTemplateInterface *iface)
{
}

gchar *
ide_project_template_get_id (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_IFACE (self)->get_id (self);
}

gchar *
ide_project_template_get_name (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_IFACE (self)->get_name (self);
}

gchar *
ide_project_template_get_description (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_IFACE (self)->get_description (self);
}

/**
 * ide_project_template_get_widget:
 * @self: An #IdeProjectTemplate
 *
 * Get's the configuration widget for the template if there is one.
 *
 * Returns: (transfer none): A #GtkWidget.
 */
GtkWidget *
ide_project_template_get_widget (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_IFACE (self)->get_widget (self);
}

/**
 * ide_project_template_get_languages:
 * @self: an #IdeProjectTemplate
 *
 * Gets the list of languages that this template can support when generating
 * the project.
 *
 * Returns: (transfer full): A newly allocated, NULL terminated list of
 *   supported languages.
 */
gchar **
ide_project_template_get_languages (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_IFACE (self)->get_languages (self);
}

gchar *
ide_project_template_get_icon_name (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_IFACE (self)->get_icon_name (self);
}

/**
 * ide_project_template_expand_async:
 * @self: an #IdeProjectTemplate
 * @params: (element-type utf8 GLib.Variant): A hashtable of template parameters.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @callback: the callback for the asynchronous operation.
 * @user_data: user data for @callback.
 *
 * Asynchronously requests expansion of the template.
 *
 * This may involve creating files and directories on disk as well as
 * expanding files based on the contents of @params.
 *
 * It is expected that this method is only called once on an #IdeProjectTemplate.
 */
void
ide_project_template_expand_async (IdeProjectTemplate  *self,
                                   GHashTable          *params,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_PROJECT_TEMPLATE (self));
  g_return_if_fail (params != NULL);
  g_return_if_fail (g_hash_table_contains (params, "name"));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_PROJECT_TEMPLATE_GET_IFACE (self)->expand_async (self, params, cancellable, callback, user_data);
}

gboolean
ide_project_template_expand_finish (IdeProjectTemplate  *self,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_PROJECT_TEMPLATE_GET_IFACE (self)->expand_finish (self, result, error);
}
