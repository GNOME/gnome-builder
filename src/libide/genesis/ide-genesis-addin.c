/* ide-genesis-addin.c
 *
 * Copyright 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-genesis-addin"

#include "config.h"

#include "genesis/ide-genesis-addin.h"

G_DEFINE_INTERFACE (IdeGenesisAddin, ide_genesis_addin, G_TYPE_OBJECT)

static void
ide_genesis_addin_default_init (IdeGenesisAddinInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("is-ready",
                                                             "Is Ready",
                                                             "If the project genesis can be executed",
                                                             FALSE,
                                                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

gchar *
ide_genesis_addin_get_title (IdeGenesisAddin *self)
{
  g_return_val_if_fail (IDE_IS_GENESIS_ADDIN (self), NULL);

  return IDE_GENESIS_ADDIN_GET_IFACE (self)->get_title (self);
}

gchar *
ide_genesis_addin_get_icon_name (IdeGenesisAddin *self)
{
  g_return_val_if_fail (IDE_IS_GENESIS_ADDIN (self), NULL);

  return IDE_GENESIS_ADDIN_GET_IFACE (self)->get_icon_name (self);
}

/**
 * ide_genesis_addin_get_widget:
 *
 * Returns: (transfer none): a #GtkWidget.
 */
GtkWidget *
ide_genesis_addin_get_widget (IdeGenesisAddin *self)
{
  g_return_val_if_fail (IDE_IS_GENESIS_ADDIN (self), NULL);

  return IDE_GENESIS_ADDIN_GET_IFACE (self)->get_widget (self);
}

void
ide_genesis_addin_run_async (IdeGenesisAddin     *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_return_if_fail (IDE_IS_GENESIS_ADDIN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_GENESIS_ADDIN_GET_IFACE (self)->run_async (self, cancellable, callback, user_data);
}

gboolean
ide_genesis_addin_run_finish (IdeGenesisAddin  *self,
                              GAsyncResult     *result,
                              GError          **error)
{
  g_return_val_if_fail (IDE_IS_GENESIS_ADDIN (self), FALSE);

  return IDE_GENESIS_ADDIN_GET_IFACE (self)->run_finish (self, result, error);
}

gint
ide_genesis_addin_get_priority (IdeGenesisAddin *self)
{
  g_return_val_if_fail (IDE_IS_GENESIS_ADDIN (self), 0);

  if (IDE_GENESIS_ADDIN_GET_IFACE (self)->get_priority)
    return IDE_GENESIS_ADDIN_GET_IFACE (self)->get_priority (self);

  return 0;
}

gchar *
ide_genesis_addin_get_label (IdeGenesisAddin *self)
{
  g_return_val_if_fail (IDE_IS_GENESIS_ADDIN (self), NULL);

  if (IDE_GENESIS_ADDIN_GET_IFACE (self)->get_label)
    return IDE_GENESIS_ADDIN_GET_IFACE (self)->get_label (self);

  return NULL;
}

gchar *
ide_genesis_addin_get_next_label (IdeGenesisAddin *self)
{
  g_return_val_if_fail (IDE_IS_GENESIS_ADDIN (self), NULL);

  if (IDE_GENESIS_ADDIN_GET_IFACE (self)->get_next_label)
    return IDE_GENESIS_ADDIN_GET_IFACE (self)->get_next_label (self);

  return NULL;
}

/**
 * ide_genesis_addin_apply_uri:
 * @self: an #IdeGenesisAddin
 * @uri: an #IdeVcsUri
 *
 * If the #IdeGenesisAddin knows how to handle @uri, it should update it's
 * UI to reflect the uri and return %TRUE. If so, ide_genesis_addin_run_async()
 * will be called afterwards to begin a clone.
 *
 * Returns: %TRUE if @uri was handled; otherwise %FALSE.
 */
gboolean
ide_genesis_addin_apply_uri (IdeGenesisAddin *self,
                             IdeVcsUri       *uri)
{
  g_return_val_if_fail (IDE_IS_GENESIS_ADDIN (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  if (IDE_GENESIS_ADDIN_GET_IFACE (self)->apply_uri)
    return IDE_GENESIS_ADDIN_GET_IFACE (self)->apply_uri (self, uri);

  return FALSE;
}
