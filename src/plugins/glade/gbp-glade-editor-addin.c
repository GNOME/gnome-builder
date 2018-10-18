/* gbp-glade-editor-addin.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-glade-editor-addin"

#include "gbp-glade-editor-addin.h"
#include "gbp-glade-properties.h"
#include "gbp-glade-view.h"

struct _GbpGladeEditorAddin
{
  GObject parent_instance;
  IdeEditorPerspective *editor;
  GbpGladeProperties *properties;
  DzlSignalGroup *project_signals;
  GActionGroup *actions;
  guint has_hold : 1;
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
  g_clear_object (&self->actions);

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
gbp_glade_editor_addin_open_glade_project (GSimpleAction *action,
                                           GVariant      *param,
                                           gpointer       user_data)
{
  GbpGladeEditorAddin *self = user_data;
  g_autofree gchar *freeme = NULL;
  g_autoptr(GFile) file = NULL;
  GbpGladeView *view;
  const gchar *path;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  path = g_variant_get_string (param, NULL);

  if (!g_path_is_absolute (path))
    {
      IdeContext *context = ide_widget_get_context (GTK_WIDGET (self->editor));
      path = freeme = ide_context_build_filename (context, path, NULL);
    }

  file = g_file_new_for_path (path);

  view = gbp_glade_view_new ();
  gtk_container_add (GTK_CONTAINER (self->editor), GTK_WIDGET (view));
  gtk_widget_show (GTK_WIDGET (view));

  gbp_glade_view_load_file_async (view, file, NULL, NULL, NULL);
}

static void
gbp_glade_editor_addin_init (GbpGladeEditorAddin *self)
{
  static GActionEntry actions[] = {
    { "open-glade-project", gbp_glade_editor_addin_open_glade_project, "s" },
  };

  self->actions = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

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
    }

  gbp_glade_editor_addin_set_project (self, project);
}

static void
gbp_glade_editor_addin_load (IdeEditorAddin       *addin,
                             IdeEditorPerspective *editor)
{
  GbpGladeEditorAddin *self = (GbpGladeEditorAddin *)addin;
  IdeLayoutTransientSidebar *transient;
  g_autoptr(GFile) file = NULL;

  g_assert (GBP_IS_GLADE_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  self->editor = editor;

  gtk_widget_insert_action_group (GTK_WIDGET (editor), "glade", self->actions);

  transient = ide_editor_perspective_get_transient_sidebar (self->editor);

  self->properties = g_object_new (GBP_TYPE_GLADE_PROPERTIES,
                                   "visible", TRUE,
                                   NULL);
  gtk_container_add (GTK_CONTAINER (transient), GTK_WIDGET (self->properties));
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
