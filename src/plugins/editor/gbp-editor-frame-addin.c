/* gbp-editor-frame-addin.c
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

#define G_LOG_DOMAIN "gbp-editor-frame-addin.h"

#include "config.h"

#include <libide-gui.h>
#include <libide-editor.h>

#include "gbp-editor-frame-addin.h"
#include "gbp-editor-frame-controls.h"

struct _GbpEditorFrameAddin
{
  GObject                 parent_instance;
  IdeFrame               *frame;
  GbpEditorFrameControls *controls;
};

static void
open_in_new_workspace_cb (GSimpleAction *action,
                          GVariant      *variant,
                          gpointer       user_data)
{
  GbpEditorFrameAddin *self = user_data;
  IdeEditorWorkspace *workspace;
  IdeWorkbench *workbench;
  IdeSurface *editor;
  IdePage *page;
  IdePage *split_page;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_FRAME (self->frame));

  if (!(page = ide_frame_get_visible_child (self->frame)))
    {
      g_warning ("No page available to split");
      return;
    }

  if (!ide_page_get_can_split (page))
    {
      g_warning ("Attempt to split a page that cannot be split");
      return;
    }

  if (!(split_page = ide_page_create_split (page)))
    {
      g_warning ("%s failed to create a split", G_OBJECT_TYPE_NAME (page));
      return;
    }

  g_assert (IDE_IS_PAGE (split_page));

  workspace = ide_editor_workspace_new (IDE_APPLICATION_DEFAULT);
  workbench = ide_widget_get_workbench (GTK_WIDGET (self->frame));
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

  editor = ide_workspace_get_surface_by_name (IDE_WORKSPACE (workspace), "editor");
  gtk_container_add (GTK_CONTAINER (editor), GTK_WIDGET (split_page));

  ide_gtk_window_present (GTK_WINDOW (workspace));
}

static void
gbp_editor_frame_addin_load (IdeFrameAddin *addin,
                             IdeFrame      *stack)
{
  GbpEditorFrameAddin *self = (GbpEditorFrameAddin *)addin;
  g_autoptr(GSimpleActionGroup) actions = NULL;
  GtkWidget *header;
  static const GActionEntry entries[] = {
    { "open-in-new-workspace", open_in_new_workspace_cb },
  };

  g_assert (GBP_IS_EDITOR_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (stack));

  self->frame = stack;

  header = ide_frame_get_titlebar (stack);

  self->controls = g_object_new (GBP_TYPE_EDITOR_FRAME_CONTROLS, NULL);
  g_signal_connect (self->controls,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->controls);
  gtk_container_add_with_properties (GTK_CONTAINER (header), GTK_WIDGET (self->controls),
                                     "pack-type", GTK_PACK_END,
                                     "priority", 100,
                                     NULL);

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (stack),
                                  "editor-frame-addin",
                                  G_ACTION_GROUP (actions));
}

static void
gbp_editor_frame_addin_unload (IdeFrameAddin *addin,
                               IdeFrame      *stack)
{
  GbpEditorFrameAddin *self = (GbpEditorFrameAddin *)addin;

  g_assert (GBP_IS_EDITOR_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (stack));

  gtk_widget_insert_action_group (GTK_WIDGET (stack), "editor-frame-addin", NULL);

  if (self->controls != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->controls));

  self->frame = NULL;
}

static void
gbp_editor_frame_addin_set_page (IdeFrameAddin *addin,
                                 IdePage       *page)
{
  GbpEditorFrameAddin *self = (GbpEditorFrameAddin *)addin;

  g_assert (GBP_IS_EDITOR_FRAME_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  if (IDE_IS_EDITOR_PAGE (page))
    {
      gbp_editor_frame_controls_set_page (self->controls, IDE_EDITOR_PAGE (page));
      gtk_widget_show (GTK_WIDGET (self->controls));
    }
  else
    {
      gbp_editor_frame_controls_set_page (self->controls, NULL);
      gtk_widget_hide (GTK_WIDGET (self->controls));
    }
}

static void
frame_addin_iface_init (IdeFrameAddinInterface *iface)
{
  iface->load = gbp_editor_frame_addin_load;
  iface->unload = gbp_editor_frame_addin_unload;
  iface->set_page = gbp_editor_frame_addin_set_page;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditorFrameAddin,
                         gbp_editor_frame_addin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_FRAME_ADDIN, frame_addin_iface_init))

static void
gbp_editor_frame_addin_class_init (GbpEditorFrameAddinClass *klass)
{
}

static void
gbp_editor_frame_addin_init (GbpEditorFrameAddin *self)
{
}
