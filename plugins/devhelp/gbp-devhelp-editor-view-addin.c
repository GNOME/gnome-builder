/* gbp-devhelp-editor-view-addin.c
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

#include "gbp-devhelp-editor-view-addin.h"
#include "gbp-devhelp-panel.h"

struct _GbpDevhelpEditorViewAddin
{
  GObject parent_instance;
};

static void iface_init (IdeEditorViewAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpDevhelpEditorViewAddin, gbp_devhelp_editor_view_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN, iface_init))

static void
request_documentation_cb (GbpDevhelpEditorViewAddin *self,
                          const gchar               *word,
                          IdeEditorView             *view)
{
  GtkWidget *layout;
  GtkWidget *panel;
  GtkWidget *pane;

  g_assert (IDE_IS_EDITOR_VIEW (view));
  g_assert (GBP_IS_DEVHELP_EDITOR_VIEW_ADDIN (self));

  layout = gtk_widget_get_ancestor (GTK_WIDGET (view), IDE_TYPE_LAYOUT);
  if (layout == NULL)
    return;

  pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (layout));
  panel = ide_widget_find_child_typed (pane, GBP_TYPE_DEVHELP_PANEL);
  gbp_devhelp_panel_focus_search (GBP_DEVHELP_PANEL (panel), word);
}

static void
gbp_devhelp_editor_view_addin_load (IdeEditorViewAddin *addin,
                                    IdeEditorView      *view)
{
  g_assert (GBP_IS_DEVHELP_EDITOR_VIEW_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  g_signal_connect_object (view,
                           "request-documentation",
                           G_CALLBACK (request_documentation_cb),
                           addin,
                           G_CONNECT_SWAPPED);
}

static void
gbp_devhelp_editor_view_addin_class_init (GbpDevhelpEditorViewAddinClass *klass)
{
}

static void
gbp_devhelp_editor_view_addin_init (GbpDevhelpEditorViewAddin *self)
{
}

static void
iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_devhelp_editor_view_addin_load;
}
