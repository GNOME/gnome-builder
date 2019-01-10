/* gbp-devhelp-editor-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-devhelp-editor-addin"

#include "gbp-devhelp-editor-addin.h"
#include "gbp-devhelp-page.h"

struct _GbpDevhelpEditorAddin
{
  GObject               parent_instance;
  IdeEditorSurface *editor;
};

static void
gbp_devhelp_editor_addin_new_devhelp_page (GSimpleAction *action,
                                           GVariant      *variant,
                                           gpointer       user_data)
{
  GbpDevhelpEditorAddin *self = user_data;
  IdeGrid *grid;
  GtkWidget *view;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_DEVHELP_EDITOR_ADDIN (self));
  g_assert (self->editor != NULL);

  view = g_object_new (GBP_TYPE_DEVHELP_PAGE,
                       "visible", TRUE,
                       NULL);
  grid = ide_editor_surface_get_grid (self->editor);
  gtk_container_add (GTK_CONTAINER (grid), view);
}

static GActionEntry actions[] = {
  { "new-devhelp-page", gbp_devhelp_editor_addin_new_devhelp_page },
};

static void
gbp_devhelp_editor_addin_load (IdeEditorAddin   *addin,
                               IdeEditorSurface *editor)
{
  GbpDevhelpEditorAddin *self = (GbpDevhelpEditorAddin *)addin;
  GtkWidget *workspace;

  g_assert (GBP_IS_DEVHELP_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (editor));

  self->editor = editor;

  workspace = gtk_widget_get_ancestor (GTK_WIDGET (editor), IDE_TYPE_WORKSPACE);

  if (workspace != NULL)
    g_action_map_add_action_entries (G_ACTION_MAP (workspace),
                                     actions,
                                     G_N_ELEMENTS (actions),
                                     self);
}

static void
gbp_devhelp_editor_addin_unload (IdeEditorAddin       *addin,
                                 IdeEditorSurface *editor)
{
  GbpDevhelpEditorAddin *self = (GbpDevhelpEditorAddin *)addin;
  GtkWidget *workspace;

  g_assert (GBP_IS_DEVHELP_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (editor));

  workspace = gtk_widget_get_ancestor (GTK_WIDGET (editor), IDE_TYPE_WORKSPACE);

  if (G_IS_ACTION_MAP (workspace))
    {
      for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
        g_action_map_remove_action (G_ACTION_MAP (workspace), actions[i].name);
    }

  self->editor = NULL;
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = gbp_devhelp_editor_addin_load;
  iface->unload = gbp_devhelp_editor_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpDevhelpEditorAddin, gbp_devhelp_editor_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN, editor_addin_iface_init))

static void
gbp_devhelp_editor_addin_class_init (GbpDevhelpEditorAddinClass *klass)
{
}

static void
gbp_devhelp_editor_addin_init (GbpDevhelpEditorAddin *self)
{
}
