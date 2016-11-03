/* ide-editor-layout-stack-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-layout-stack-view"

#include <glib/gi18n.h>

#include "editor/ide-editor-layout-stack-addin.h"
#include "editor/ide-editor-layout-stack-controls.h"
#include "editor/ide-editor-view.h"

struct _IdeEditorLayoutStackAddin
{
  GObject parent_instance;
  IdeEditorLayoutStackControls *controls;
};

static void layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeEditorLayoutStackAddin,
                        ide_editor_layout_stack_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_LAYOUT_STACK_ADDIN,
                                               layout_stack_addin_iface_init))

static void
ide_editor_layout_stack_addin_class_init (IdeEditorLayoutStackAddinClass *klass)
{
}

static void
ide_editor_layout_stack_addin_init (IdeEditorLayoutStackAddin *self)
{
}

static void
goto_line_activate (GSimpleAction *action,
                    GVariant      *param,
                    gpointer       user_data)
{
  IdeEditorLayoutStackAddin *self = user_data;

  g_assert (IDE_IS_EDITOR_LAYOUT_STACK_ADDIN (self));

  gtk_widget_show (GTK_WIDGET (self->controls->goto_line_popover));
}

static void
ide_editor_layout_stack_addin_load (IdeLayoutStackAddin *addin,
                                    IdeLayoutStack      *stack)
{
  IdeEditorLayoutStackAddin *self = (IdeEditorLayoutStackAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  static const GActionEntry entries[] = {
    { "goto-line", goto_line_activate },
  };

  g_assert (IDE_IS_EDITOR_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  self->controls = g_object_new (IDE_TYPE_EDITOR_LAYOUT_STACK_CONTROLS, NULL);
  g_signal_connect (self->controls,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->controls);
  ide_layout_stack_add_control (stack, GTK_WIDGET (self->controls), 0);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (stack), "editor-controls", G_ACTION_GROUP (group));
}

static void
ide_editor_layout_stack_addin_unload (IdeLayoutStackAddin *addin,
                                      IdeLayoutStack      *stack)
{
  IdeEditorLayoutStackAddin *self = (IdeEditorLayoutStackAddin *)addin;

  g_assert (IDE_IS_EDITOR_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  gtk_widget_destroy (GTK_WIDGET (self->controls));
}

static void
ide_editor_layout_stack_addin_set_view (IdeLayoutStackAddin *addin,
                                        IdeLayoutView       *view)
{
  IdeEditorLayoutStackAddin *self = (IdeEditorLayoutStackAddin *)addin;

  g_assert (IDE_IS_EDITOR_LAYOUT_STACK_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  if (IDE_IS_EDITOR_VIEW (view))
    {
      ide_editor_layout_stack_controls_set_view (self->controls, IDE_EDITOR_VIEW (view));
      gtk_widget_show (GTK_WIDGET (self->controls));
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self->controls));
      ide_editor_layout_stack_controls_set_view (self->controls, NULL);
    }
}

static void
layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface)
{
  iface->load = ide_editor_layout_stack_addin_load;
  iface->unload = ide_editor_layout_stack_addin_unload;
  iface->set_view = ide_editor_layout_stack_addin_set_view;
}
