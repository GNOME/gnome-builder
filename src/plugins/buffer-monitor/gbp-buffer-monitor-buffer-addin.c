/* gbp-buffer-monitor-buffer-addin.c
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

#define G_LOG_DOMAIN "gbp-buffer-monitor-buffer-addin"

#include "config.h"

#include <libide-code.h>
#include <libide-projects.h>
#include <string.h>

#include "ide-buffer-private.h"

#include "gbp-buffer-monitor-buffer-addin.h"

struct _GbpBufferMonitorBufferAddin
{
  GObject       parent_instance;

  IdeBuffer    *buffer;
  GFileMonitor *monitor;
  IdeProject   *project;

  GDateTime    *mtime;
  guint         mtime_set : 1;
};

static void
gbp_buffer_monitor_buffer_addin_check_for_change (GbpBufferMonitorBufferAddin *self,
                                                  GFile                       *file)
{
  g_autoptr(GFileInfo) info = NULL;

  g_assert (GBP_IS_BUFFER_MONITOR_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (self->buffer));
  g_assert (G_IS_FILE (file));

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_TIME_MODIFIED","
                            G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            NULL);

  if (info == NULL)
    {
      GtkTextIter iter;

      /* If we get here, the file likely does not exist on disk. We might have
       * a situation where the file was moved out from under the user. If so,
       * then we should mark the buffer as modified so that the user can save
       * it going forward.
       */

      gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (self->buffer), &iter);
      if (gtk_text_iter_get_offset (&iter) != 0)
        gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (self->buffer), TRUE);
    }

  if (!self->mtime_set)
    return;

  if (info != NULL && g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
    {
      g_autoptr(GDateTime) mtime = g_file_info_get_modification_date_time (info);

      if (self->mtime != NULL && mtime != NULL && !g_date_time_equal (self->mtime, mtime))
        {
          self->mtime_set = FALSE;

          /* Cancel any further requests from being delivered, we don't care
           * until the file has been re-loaded or saved again.
           */
          if (self->monitor != NULL)
            {
              g_file_monitor_cancel (self->monitor);
              g_clear_object (&self->monitor);
            }

          /* Let the buffer propagate the status to the UI */
          _ide_buffer_set_changed_on_volume (self->buffer, TRUE);
        }
    }
}

static void
gbp_buffer_monitor_buffer_addin_file_changed_cb (GbpBufferMonitorBufferAddin *self,
                                                 GFile                       *file,
                                                 GFile                       *other_file,
                                                 GFileMonitorEvent            event,
                                                 GFileMonitor                *monitor)
{
  GFile *expected;

  IDE_ENTRY;

  g_assert (GBP_IS_BUFFER_MONITOR_BUFFER_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (G_IS_FILE_MONITOR (monitor));
  g_assert (IDE_IS_BUFFER (self->buffer));

  if (g_file_monitor_is_cancelled (monitor))
    IDE_EXIT;

  expected = ide_buffer_get_file (self->buffer);
  if (!g_file_equal (expected, file))
    IDE_EXIT;

  IDE_TRACE_MSG ("%s event=%d", g_file_peek_path (file), event);

  if (event == G_FILE_MONITOR_EVENT_CHANGED ||
      event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT ||
      event == G_FILE_MONITOR_EVENT_DELETED ||
      event == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED)
    {
      gbp_buffer_monitor_buffer_addin_check_for_change (self, file);
      IDE_EXIT;
    }

  IDE_EXIT;
}

static void
gbp_buffer_monitor_buffer_addin_setup_monitor (GbpBufferMonitorBufferAddin *self,
                                               GFile                       *file)
{
  IDE_ENTRY;

  g_assert (GBP_IS_BUFFER_MONITOR_BUFFER_ADDIN (self));
  g_assert (!file || G_IS_FILE (file));

  if (self->monitor != NULL)
    {
      g_file_monitor_cancel (self->monitor);
      g_clear_object (&self->monitor);
    }

  if (file != NULL)
    {
      g_autoptr(GFileInfo) info = NULL;

      info = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_TIME_MODIFIED","
                                G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC","
                                G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                NULL);

      self->mtime_set = FALSE;

      if (info != NULL)
        {
          if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
            _ide_buffer_set_read_only (self->buffer,
                                       !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE));

          if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
            {
              g_clear_pointer (&self->mtime, g_date_time_unref);
              self->mtime = g_file_info_get_modification_date_time (info);
              self->mtime_set = TRUE;
            }
        }

      self->monitor = g_file_monitor_file (file,
                                           G_FILE_MONITOR_NONE,
                                           NULL,
                                           NULL);

      if (self->monitor != NULL)
        {
          g_file_monitor_set_rate_limit (self->monitor, 500);
          g_signal_connect_object (self->monitor,
                                   "changed",
                                   G_CALLBACK (gbp_buffer_monitor_buffer_addin_file_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
    }

  IDE_EXIT;
}

static void
file_renamed_cb (GbpBufferMonitorBufferAddin *self,
                 GFile                       *file,
                 GFile                       *other,
                 IdeProject                  *project)
{
  GFile *buffer_file;

  IDE_ENTRY;

  g_assert (GBP_IS_BUFFER_MONITOR_BUFFER_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_FILE (other));
  g_assert (IDE_IS_PROJECT (project));

  buffer_file = ide_buffer_get_file (self->buffer);

  if (g_file_equal (buffer_file, file))
    {
      _ide_buffer_set_file (self->buffer, other);
    }
  else if (g_file_has_prefix (buffer_file, file))
    {
      g_autofree gchar *suffix = g_file_get_relative_path (file, buffer_file);
      g_autoptr(GFile) new_file = g_file_get_child (other, suffix);

      _ide_buffer_set_file (self->buffer, new_file);
    }

  IDE_EXIT;
}

static void
gbp_buffer_monitor_buffer_addin_load (IdeBufferAddin *addin,
                                      IdeBuffer      *buffer)
{
  GbpBufferMonitorBufferAddin *self = (GbpBufferMonitorBufferAddin *)addin;
  g_autoptr(IdeContext) context = NULL;
  IdeProject *project;

  g_assert (GBP_IS_BUFFER_MONITOR_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->buffer = buffer;

  context = ide_buffer_ref_context (buffer);
  project = ide_project_from_context (context);

  self->project = g_object_ref (project);
  g_signal_connect_object (self->project,
                           "file-renamed",
                           G_CALLBACK (file_renamed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_buffer_monitor_buffer_addin_unload (IdeBufferAddin *addin,
                                        IdeBuffer      *buffer)
{
  GbpBufferMonitorBufferAddin *self = (GbpBufferMonitorBufferAddin *)addin;

  g_assert (GBP_IS_BUFFER_MONITOR_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gbp_buffer_monitor_buffer_addin_setup_monitor (self, NULL);

  g_signal_handlers_disconnect_by_func (self->project,
                                        G_CALLBACK (file_renamed_cb),
                                        self);

  g_clear_pointer (&self->mtime, g_date_time_unref);
  g_clear_object (&self->project);
  self->buffer = NULL;
}

static void
gbp_buffer_monitor_buffer_addin_save_file (IdeBufferAddin *addin,
                                           IdeBuffer      *buffer,
                                           GFile          *file)
{
  GbpBufferMonitorBufferAddin *self = (GbpBufferMonitorBufferAddin *)addin;
  GFile *current;

  g_assert (IDE_IS_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  current = ide_buffer_get_file (buffer);

  if (!g_file_equal (file, current))
    return;

  /* Disable monitors while saving */
  gbp_buffer_monitor_buffer_addin_setup_monitor (self, NULL);
}

static void
gbp_buffer_monitor_buffer_addin_file_saved (IdeBufferAddin *addin,
                                            IdeBuffer      *buffer,
                                            GFile          *file)
{
  GbpBufferMonitorBufferAddin *self = (GbpBufferMonitorBufferAddin *)addin;
  GFile *current;

  g_assert (IDE_IS_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  current = ide_buffer_get_file (buffer);

  if (!g_file_equal (file, current))
    return;

  /* Restore any file monitors */
  gbp_buffer_monitor_buffer_addin_setup_monitor (self, current);
}

static void
gbp_buffer_monitor_buffer_addin_file_loaded (IdeBufferAddin *addin,
                                             IdeBuffer      *buffer,
                                             GFile          *file)
{
  GbpBufferMonitorBufferAddin *self = (GbpBufferMonitorBufferAddin *)addin;

  g_assert (IDE_IS_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  gbp_buffer_monitor_buffer_addin_setup_monitor (self, file);
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->load = gbp_buffer_monitor_buffer_addin_load;
  iface->unload = gbp_buffer_monitor_buffer_addin_unload;
  iface->save_file = gbp_buffer_monitor_buffer_addin_save_file;
  iface->file_saved = gbp_buffer_monitor_buffer_addin_file_saved;
  iface->file_loaded = gbp_buffer_monitor_buffer_addin_file_loaded;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBufferMonitorBufferAddin, gbp_buffer_monitor_buffer_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_buffer_monitor_buffer_addin_class_init (GbpBufferMonitorBufferAddinClass *klass)
{
}

static void
gbp_buffer_monitor_buffer_addin_init (GbpBufferMonitorBufferAddin *self)
{
}
