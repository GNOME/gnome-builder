/* ide-shortcut-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-shortcut-provider"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-shortcut-provider.h"

G_DEFINE_INTERFACE (IdeShortcutProvider, ide_shortcut_provider, IDE_TYPE_OBJECT)

static GListModel *
ide_shortcut_provider_real_list_shortcuts (IdeShortcutProvider *self)
{
  return G_LIST_MODEL (g_list_store_new (GTK_TYPE_SHORTCUT));
}

static void
ide_shortcut_provider_default_init (IdeShortcutProviderInterface *iface)
{
  iface->list_shortcuts = ide_shortcut_provider_real_list_shortcuts;
}

/**
 * ide_shortcut_provider_list_shortcuts:
 * @self: a #IdeShortcutProvider
 *
 * Gets a #GListModel of #GtkShortcut.
 *
 * This function should return a #GListModel of #GtkShortcut that are updated
 * as necessary by the plugin. This list model is used to activate shortcuts
 * based on user input and allows more control by plugins over when and how
 * shortcuts may activate.
 *
 * Returns: (transfer full): A #GListModel of #GtkShortcut
 */
GListModel *
ide_shortcut_provider_list_shortcuts (IdeShortcutProvider *self)
{
  GListModel *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SHORTCUT_PROVIDER (self), NULL);

  ret = IDE_SHORTCUT_PROVIDER_GET_IFACE (self)->list_shortcuts (self);

  IDE_RETURN (ret);
}
