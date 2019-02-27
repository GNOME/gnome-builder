/* gbp-line-spacing-buffer-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-line-spacing-buffer-addin"

#include "config.h"

#include <libide-code.h>

#include "gbp-line-spacing-buffer-addin.h"

struct _GbpLineSpacingBufferAddin
{
  GObject     parent_instance;
  IdeBuffer  *buffer;
  GtkTextTag *tag;
  GSettings  *settings;
};

static void
gbp_line_spacing_buffer_addin_apply (GbpLineSpacingBufferAddin *self)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_BUFFER (self->buffer));
  g_assert (GTK_IS_TEXT_TAG (self->tag));

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end);
  gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (self->buffer), self->tag, &begin, &end);
}

static void
on_buffer_changed_cb (GbpLineSpacingBufferAddin *self,
                      IdeBuffer                 *buffer)
{
  g_assert (GBP_IS_LINE_SPACING_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->tag != NULL)
    gbp_line_spacing_buffer_addin_apply (self);
}

static void
on_line_spacing_changed_cb (GbpLineSpacingBufferAddin *self,
                            const gchar               *key,
                            GSettings                 *settings)
{
  gint spacing;

  g_assert (GBP_IS_LINE_SPACING_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (self->buffer));
  g_assert (G_IS_SETTINGS (settings));

  if (self->tag != NULL)
    {
      GtkTextTagTable *table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self->buffer));
      gtk_text_tag_table_remove (table, self->tag);
      self->tag = NULL;
    }

  spacing = g_settings_get_int (settings, "line-spacing");

  if (spacing > 0)
    {
      self->tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self->buffer), NULL,
                                              "pixels-above-lines", spacing,
                                              "pixels-below-lines", spacing,
                                              NULL);
      gbp_line_spacing_buffer_addin_apply (self);
    }
}

static void
gbp_line_spacing_buffer_addin_load (IdeBufferAddin *addin,
                                    IdeBuffer      *buffer)
{
  GbpLineSpacingBufferAddin *self = (GbpLineSpacingBufferAddin *)addin;

  g_assert (GBP_IS_LINE_SPACING_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->buffer = buffer;
  self->settings = g_settings_new ("org.gnome.builder.editor");

  g_signal_connect_object (self->settings,
                           "changed::line-spacing",
                           G_CALLBACK (on_line_spacing_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  on_line_spacing_changed_cb (self, NULL, self->settings);

  g_signal_connect_object (self->buffer,
                           "changed",
                           G_CALLBACK (on_buffer_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_line_spacing_buffer_addin_unload (IdeBufferAddin *addin,
                                      IdeBuffer      *buffer)
{
  GbpLineSpacingBufferAddin *self = (GbpLineSpacingBufferAddin *)addin;

  g_assert (GBP_IS_LINE_SPACING_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_signal_handlers_disconnect_by_func (self->settings,
                                        G_CALLBACK (on_line_spacing_changed_cb),
                                        self);

  g_signal_handlers_disconnect_by_func (self->buffer,
                                        G_CALLBACK (on_buffer_changed_cb),
                                        self);

  g_clear_object (&self->settings);
  self->buffer = NULL;
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->load = gbp_line_spacing_buffer_addin_load;
  iface->unload = gbp_line_spacing_buffer_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpLineSpacingBufferAddin, gbp_line_spacing_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_line_spacing_buffer_addin_class_init (GbpLineSpacingBufferAddinClass *klass)
{
}

static void
gbp_line_spacing_buffer_addin_init (GbpLineSpacingBufferAddin *self)
{
}
