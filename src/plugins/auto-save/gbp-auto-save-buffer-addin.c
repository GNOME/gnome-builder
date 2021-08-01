/* gbp-auto-save-buffer-addin.c
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

#define G_LOG_DOMAIN "gbp-auto-save-buffer-addin"

#include "config.h"

#include <libide-code.h>

#include "gbp-auto-save-buffer-addin.h"

struct _GbpAutoSaveBufferAddin
{
  GObject    parent_instance;
  IdeBuffer *buffer;
  GSettings *settings;
  guint      source_id;
  guint      auto_save_timeout;
  guint      auto_save : 1;
};

static gboolean
gbp_auto_save_buffer_addin_source_cb (gpointer user_data)
{
  GbpAutoSaveBufferAddin *self = user_data;

  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));

  self->source_id = 0;

  /* Only auto-save if there are no active changes and the file has not been
   * changed out from under us on the storage volume.
   */
  if (!ide_buffer_get_changed_on_volume (self->buffer) &&
      gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (self->buffer)))
    ide_buffer_save_file_async (self->buffer, NULL, NULL, NULL, NULL, NULL);

  return G_SOURCE_REMOVE;
}

static void
gbp_auto_save_buffer_addin_create_source (GbpAutoSaveBufferAddin *self)
{
  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));

  if (!self->auto_save)
    return;

  if (self->source_id == 0)
    self->source_id = g_timeout_add_seconds_full (G_PRIORITY_HIGH,
                                                  self->auto_save_timeout,
                                                  gbp_auto_save_buffer_addin_source_cb,
                                                  g_object_ref (self),
                                                  g_object_unref);
}

static void
gbp_auto_save_buffer_addin_change_settled_cb (GbpAutoSaveBufferAddin *self,
                                              IdeBuffer              *buffer)
{
  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_clear_handle_id (&self->source_id, g_source_remove);
  gbp_auto_save_buffer_addin_create_source (self);
}

static void
gbp_auto_save_buffer_addin_modified_changed_cb (GbpAutoSaveBufferAddin *self,
                                                IdeBuffer              *buffer)
{
  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (!gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)))
    g_clear_handle_id (&self->source_id, g_source_remove);
  else
    gbp_auto_save_buffer_addin_create_source (self);
}

static void
gbp_auto_save_buffer_addin_changed_cb (GbpAutoSaveBufferAddin *self,
                                       const gchar            *key,
                                       GSettings              *settings)
{
  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));
  g_assert (G_IS_SETTINGS (settings));

  self->auto_save = g_settings_get_boolean (settings, "auto-save");
  self->auto_save_timeout = g_settings_get_int (settings, "auto-save-timeout");

  if (self->auto_save_timeout == 0)
    self->auto_save_timeout = 60;

  g_clear_handle_id (&self->source_id, g_source_remove);
}

static void
gbp_auto_save_buffer_addin_load (IdeBufferAddin *addin,
                                 IdeBuffer      *buffer)
{
  GbpAutoSaveBufferAddin *self = (GbpAutoSaveBufferAddin *)addin;

  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->buffer = buffer;
  self->settings = g_settings_new ("org.gnome.builder.editor");

  self->auto_save = g_settings_get_boolean (self->settings, "auto-save");
  self->auto_save_timeout = g_settings_get_int (self->settings, "auto-save-timeout");

  g_signal_connect_object (self->settings,
                           "changed::auto-save",
                           G_CALLBACK (gbp_auto_save_buffer_addin_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->settings,
                           "changed::auto-save-timeout",
                           G_CALLBACK (gbp_auto_save_buffer_addin_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "change-settled",
                           G_CALLBACK (gbp_auto_save_buffer_addin_change_settled_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "modified-changed",
                           G_CALLBACK (gbp_auto_save_buffer_addin_modified_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

}

static void
gbp_auto_save_buffer_addin_unload (IdeBufferAddin *addin,
                                   IdeBuffer      *buffer)
{
  GbpAutoSaveBufferAddin *self = (GbpAutoSaveBufferAddin *)addin;

  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_clear_handle_id (&self->source_id, g_source_remove);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (gbp_auto_save_buffer_addin_change_settled_cb),
                                        self);
  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (gbp_auto_save_buffer_addin_modified_changed_cb),
                                        self);

  g_clear_object (&self->settings);

  self->buffer = NULL;
}

static void
gbp_auto_save_buffer_addin_save_file (IdeBufferAddin *addin,
                                      IdeBuffer      *buffer,
                                      GFile          *file)
{
  GbpAutoSaveBufferAddin *self = (GbpAutoSaveBufferAddin *)addin;
  GFile *orig_file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  orig_file = ide_buffer_get_file (buffer);

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_FILE (orig_file));

  /* If the user requests the buffer save its contents to the original
   * backing file, then we can drop our auto-save request.
   */
  if (g_file_equal (file, orig_file))
    g_clear_handle_id (&self->source_id, g_source_remove);
}

static void
gbp_auto_save_buffer_addin_file_loaded (IdeBufferAddin *addin,
                                        IdeBuffer      *buffer,
                                        GFile          *file)
{
  GbpAutoSaveBufferAddin *self = (GbpAutoSaveBufferAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_AUTO_SAVE_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  /* Contents just finished loading, clear any queued requests
   * that happened while loading.
   */
  g_clear_handle_id (&self->source_id, g_source_remove);
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->load = gbp_auto_save_buffer_addin_load;
  iface->unload = gbp_auto_save_buffer_addin_unload;
  iface->save_file = gbp_auto_save_buffer_addin_save_file;
  iface->file_loaded = gbp_auto_save_buffer_addin_file_loaded;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpAutoSaveBufferAddin, gbp_auto_save_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_auto_save_buffer_addin_class_init (GbpAutoSaveBufferAddinClass *klass)
{
}

static void
gbp_auto_save_buffer_addin_init (GbpAutoSaveBufferAddin *self)
{
}
