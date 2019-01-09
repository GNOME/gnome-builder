/* ide-surfaces-button.c
 *
 * Copyright 2018 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "ide-surfaces-button"

#include "config.h"

#include <libide-core.h>

#include "ide-surfaces-button.h"

struct _IdeSurfacesButton
{
  DzlMenuButton parent_instance;
};

G_DEFINE_TYPE (IdeSurfacesButton, ide_surfaces_button, DZL_TYPE_MENU_BUTTON)

static void
ide_surfaces_button_items_changed_cb (IdeSurfacesButton *self,
                                      guint              position,
                                      guint              added,
                                      guint              removed,
                                      GMenuModel        *model)
{
  gboolean visible = FALSE;
  guint n_items;
  guint count = 0;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SURFACES_BUTTON (self));
  g_assert (G_IS_MENU_MODEL (model));

  /* We either have multiple sections, or a single section with
   * possibly multiple children. Any of these means visible.
   */

  n_items = g_menu_model_get_n_items (model);
  visible = n_items > 1;

  for (guint i = 0; !visible && i < n_items; i++)
    {
      g_autoptr(GMenuLinkIter) iter = g_menu_model_iterate_item_links (model, i);

      while (g_menu_link_iter_next (iter))
        {
          g_autoptr(GMenuModel) child = g_menu_link_iter_get_value (iter);
          count += g_menu_model_get_n_items (child);
        }

      visible = count > 1;
    }

  gtk_widget_set_visible (GTK_WIDGET (self), visible);
}

static void
ide_surfaces_button_notify_model (IdeSurfacesButton *self,
                                  GParamSpec        *pspec,
                                  gpointer           user_data)
{
  GMenuModel *model;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SURFACES_BUTTON (self));

  if ((model = dzl_menu_button_get_model (DZL_MENU_BUTTON (self))))
    {
      g_signal_connect_object (model,
                               "items-changed",
                               G_CALLBACK (ide_surfaces_button_items_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      ide_surfaces_button_items_changed_cb (self, 0, 0, 0, model);
    }
}

static void
ide_surfaces_button_class_init (IdeSurfacesButtonClass *klass)
{
}

static void
ide_surfaces_button_init (IdeSurfacesButton *self)
{
  g_signal_connect (self,
                    "notify::model",
                    G_CALLBACK (ide_surfaces_button_notify_model),
                    NULL);
}
