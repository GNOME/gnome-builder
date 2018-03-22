/* ide-editor-layout-stack-addin.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-layout-stack-addin.h"

#include "config.h"

#include <dazzle.h>

#include "editor/ide-editor-layout-stack-addin.h"
#include "editor/ide-editor-layout-stack-controls.h"
#include "editor/ide-editor-view.h"
#include "layout/ide-layout-stack-header.h"

struct _IdeEditorLayoutStackAddin
{
  GObject                       parent_instance;
  IdeEditorLayoutStackControls *controls;
};

static void
ide_editor_layout_stack_addin_load (IdeLayoutStackAddin *addin,
                                    IdeLayoutStack      *stack)
{
  IdeEditorLayoutStackAddin *self = (IdeEditorLayoutStackAddin *)addin;
  GtkWidget *header;

  g_assert (IDE_IS_EDITOR_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  header = ide_layout_stack_get_titlebar (stack);

  self->controls = g_object_new (IDE_TYPE_EDITOR_LAYOUT_STACK_CONTROLS, NULL);
  g_signal_connect (self->controls,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->controls);
  gtk_container_add_with_properties (GTK_CONTAINER (header), GTK_WIDGET (self->controls),
                                     "pack-type", GTK_PACK_END,
                                     "priority", 100,
                                     NULL);
}

static void
ide_editor_layout_stack_addin_unload (IdeLayoutStackAddin *addin,
                                      IdeLayoutStack      *stack)
{
  IdeEditorLayoutStackAddin *self = (IdeEditorLayoutStackAddin *)addin;

  g_assert (IDE_IS_EDITOR_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  if (self->controls != NULL)
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
      ide_editor_layout_stack_controls_set_view (self->controls, NULL);
      gtk_widget_hide (GTK_WIDGET (self->controls));
    }
}

static void
layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface)
{
  iface->load = ide_editor_layout_stack_addin_load;
  iface->unload = ide_editor_layout_stack_addin_unload;
  iface->set_view = ide_editor_layout_stack_addin_set_view;
}

G_DEFINE_TYPE_WITH_CODE (IdeEditorLayoutStackAddin,
                         ide_editor_layout_stack_addin,
                         G_TYPE_OBJECT,
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
