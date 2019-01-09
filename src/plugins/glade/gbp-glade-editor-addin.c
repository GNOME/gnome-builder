/* gbp-glade-editor-addin.c
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

#define G_LOG_DOMAIN "gbp-glade-editor-addin"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-glade-editor-addin.h"
#include "gbp-glade-private.h"
#include "gbp-glade-properties.h"
#include "gbp-glade-view.h"

struct _GbpGladeEditorAddin
{
  GObject               parent_instance;

  /* Widgets */
  IdeEditorPerspective *editor;
  GbpGladeProperties   *properties;
  GladeSignalEditor    *signals;
  DzlDockWidget        *signals_dock;

  /* Owned references */
  DzlSignalGroup       *project_signals;

  guint                 has_hold : 1;
};

static void editor_addin_iface_init (IdeEditorAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpGladeEditorAddin, gbp_glade_editor_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN, editor_addin_iface_init))

static void
gbp_glade_editor_addin_dispose (GObject *object)
{
  GbpGladeEditorAddin *self = (GbpGladeEditorAddin *)object;

  dzl_signal_group_set_target (self->project_signals, NULL);
  g_clear_object (&self->project_signals);

  G_OBJECT_CLASS (gbp_glade_editor_addin_parent_class)->dispose (object);
}

static void
gbp_glade_editor_addin_class_init (GbpGladeEditorAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_glade_editor_addin_dispose;
}

static void
gbp_glade_editor_addin_selection_changed_cb (GbpGladeEditorAddin *self,
                                             GladeProject        *project)
{
  GList *selection = NULL;

  g_assert (GBP_IS_GLADE_EDITOR_ADDIN (self));
  g_assert (!project || GLADE_IS_PROJECT (project));

  if (project != NULL)
    selection = glade_project_selection_get (project);

  if (selection != NULL && selection->next == NULL)
    {
      GtkWidget *widget = selection->data;
      GladeWidget *glade = glade_widget_get_from_gobject (widget);

      gbp_glade_properties_set_widget (self->properties, glade);
      glade_signal_editor_load_widget (self->signals, glade);
      gtk_widget_show (GTK_WIDGET (self->signals_dock));
    }
  else
    {
      gbp_glade_properties_set_widget (self->properties, NULL);
      glade_signal_editor_load_widget (self->signals, NULL);
      gtk_widget_hide (GTK_WIDGET (self->signals_dock));
    }
}

static void
gbp_glade_editor_addin_bind_cb (GbpGladeEditorAddin *self,
                                GladeProject        *project,
                                DzlSignalGroup      *project_signals)
{
  g_assert (GBP_IS_GLADE_EDITOR_ADDIN (self));
  g_assert (GLADE_IS_PROJECT (project));
  g_assert (DZL_IS_SIGNAL_GROUP (project_signals));

  gbp_glade_editor_addin_selection_changed_cb (self, project);
}

static void
gbp_glade_editor_addin_init (GbpGladeEditorAddin *self)
{
  self->project_signals = dzl_signal_group_new (GLADE_TYPE_PROJECT);

  g_signal_connect_object (self->project_signals,
                           "bind",
                           G_CALLBACK (gbp_glade_editor_addin_bind_cb),
                           self,
                           G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->project_signals,
                                   "selection-changed",
                                   G_CALLBACK (gbp_glade_editor_addin_selection_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
gbp_glade_editor_addin_set_project (GbpGladeEditorAddin *self,
                                    GladeProject        *project)
{
  g_assert (GBP_IS_GLADE_EDITOR_ADDIN (self));
  g_assert (!project || GLADE_IS_PROJECT (project));

  dzl_signal_group_set_target (self->project_signals, project);
}

static void
gbp_glade_editor_addin_view_set (IdeEditorAddin *addin,
                                 IdeLayoutView  *view)
{
  GbpGladeEditorAddin *self = (GbpGladeEditorAddin *)addin;
  IdeLayoutTransientSidebar *transient;
  GladeProject *project = NULL;

  g_assert (GBP_IS_GLADE_EDITOR_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  transient = ide_editor_perspective_get_transient_sidebar (self->editor);

  if (self->has_hold)
    {
      ide_layout_transient_sidebar_unlock (transient);
      self->has_hold = FALSE;
    }

  if (GBP_IS_GLADE_VIEW (view))
    {
      project = gbp_glade_view_get_project (GBP_GLADE_VIEW (view));
      ide_layout_transient_sidebar_set_view (transient, view);
      ide_layout_transient_sidebar_lock (transient);
      gtk_widget_show (GTK_WIDGET (transient));
      dzl_dock_item_present (DZL_DOCK_ITEM (self->properties));
      self->has_hold = TRUE;
      dzl_gtk_widget_mux_action_groups (GTK_WIDGET (self->properties),
                                        GTK_WIDGET (view),
                                        "GBP_GLADE_VIEW");
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self->signals_dock));
      dzl_gtk_widget_mux_action_groups (GTK_WIDGET (self->properties),
                                        NULL,
                                        "GBP_GLADE_VIEW");
    }

  gbp_glade_editor_addin_set_project (self, project);
}

static void
gbp_glade_editor_addin_load (IdeEditorAddin       *addin,
                             IdeEditorPerspective *editor)
{
  GbpGladeEditorAddin *self = (GbpGladeEditorAddin *)addin;
  IdeLayoutTransientSidebar *transient;
  GtkWidget *utils;

  g_assert (GBP_IS_GLADE_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  self->editor = editor;

  transient = ide_editor_perspective_get_transient_sidebar (self->editor);
  utils = ide_editor_perspective_get_utilities (self->editor);

  self->properties = g_object_new (GBP_TYPE_GLADE_PROPERTIES,
                                   "visible", TRUE,
                                   NULL);
  gtk_container_add (GTK_CONTAINER (transient), GTK_WIDGET (self->properties));

  self->signals_dock = g_object_new (DZL_TYPE_DOCK_WIDGET,
                                     "title", _("Signals"),
                                     "icon-name", "glade-symbolic",
                                     "visible", TRUE,
                                     NULL);
  gtk_container_add (GTK_CONTAINER (utils), GTK_WIDGET (self->signals_dock));

  self->signals = g_object_new (GLADE_TYPE_SIGNAL_EDITOR,
                                "visible", TRUE,
                                NULL);
  gtk_container_add (GTK_CONTAINER (self->signals_dock), GTK_WIDGET (self->signals));

  /* Wire up the shortcuts to the panel too */
  _gbp_glade_view_init_shortcuts (GTK_WIDGET (self->properties));
}

static void
gbp_glade_editor_addin_unload (IdeEditorAddin       *addin,
                               IdeEditorPerspective *editor)
{
  GbpGladeEditorAddin *self = (GbpGladeEditorAddin *)addin;
  IdeLayoutTransientSidebar *transient;

  g_assert (GBP_IS_GLADE_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  transient = ide_editor_perspective_get_transient_sidebar (self->editor);

  if (self->has_hold)
    {
      ide_layout_transient_sidebar_unlock (transient);
      self->has_hold = FALSE;
    }

  gtk_widget_insert_action_group (GTK_WIDGET (editor), "glade", NULL);
  gtk_widget_destroy (GTK_WIDGET (self->properties));

  self->editor = NULL;
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = gbp_glade_editor_addin_load;
  iface->unload = gbp_glade_editor_addin_unload;
  iface->view_set = gbp_glade_editor_addin_view_set;
}
