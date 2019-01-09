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
  GbpEditorFrameControls *controls;
};

static void
gbp_editor_frame_addin_load (IdeFrameAddin *addin,
                             IdeFrame      *stack)
{
  GbpEditorFrameAddin *self = (GbpEditorFrameAddin *)addin;
  GtkWidget *header;

  g_assert (GBP_IS_EDITOR_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (stack));

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
}

static void
gbp_editor_frame_addin_unload (IdeFrameAddin *addin,
                               IdeFrame      *stack)
{
  GbpEditorFrameAddin *self = (GbpEditorFrameAddin *)addin;

  g_assert (GBP_IS_EDITOR_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (stack));

  if (self->controls != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->controls));
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
