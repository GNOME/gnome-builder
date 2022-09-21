/* gbp-codeui-buffer-addin.c
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

#define G_LOG_DOMAIN "gbp-codeui-buffer-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-gui.h>

#include "ide-application-private.h"
#include "ide-diagnostics-manager-private.h"

#include "gbp-codeui-buffer-addin.h"

#define FORMAT_ON_SAVE_TIMEOUT_MSEC 2000

struct _GbpCodeuiBufferAddin
{
  GObject                parent_instance;
  IdeDiagnosticsManager *diagnostics_manager;
  IdeBuffer             *buffer;
  GFile                 *file;
};

static void
gbp_codeui_buffer_addin_queue_diagnose (GbpCodeuiBufferAddin *self,
                                        IdeBuffer            *buffer)
{
  g_autoptr(GBytes) contents = NULL;
  const gchar *lang_id;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self->diagnostics_manager));
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);
  lang_id = ide_buffer_get_language_id (buffer);
  contents = ide_buffer_dup_content (buffer);

  _ide_diagnostics_manager_file_changed (self->diagnostics_manager, file, contents, lang_id);
}

static void
gbp_codeui_buffer_addin_change_settled (IdeBufferAddin *addin,
                                        IdeBuffer      *buffer)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gbp_codeui_buffer_addin_queue_diagnose (self, buffer);
}

static void
gbp_codeui_buffer_addin_file_loaded (IdeBufferAddin *addin,
                                     IdeBuffer      *buffer,
                                     GFile          *file)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;
  const gchar *lang_id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  g_set_object (&self->file, file);

  lang_id = ide_buffer_get_language_id (buffer);

  _ide_diagnostics_manager_file_opened (self->diagnostics_manager, file, lang_id);
}

static void
gbp_codeui_buffer_addin_file_saved (IdeBufferAddin *addin,
                                    IdeBuffer      *buffer,
                                    GFile          *file)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  g_set_object (&self->file, file);

  gbp_codeui_buffer_addin_queue_diagnose (self, buffer);
}

static void
gbp_codeui_buffer_addin_changed_cb (GbpCodeuiBufferAddin  *self,
                                    IdeDiagnosticsManager *manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (manager));

  if (self->file != NULL)
    {
      g_autoptr(IdeDiagnostics) diagnostics = NULL;

      diagnostics = ide_diagnostics_manager_get_diagnostics_for_file (manager, self->file);
      ide_buffer_set_diagnostics (self->buffer, diagnostics);
    }
}

static void
gbp_codeui_buffer_addin_language_set (IdeBufferAddin *addin,
                                      IdeBuffer      *buffer,
                                      const gchar    *language_id)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;
  GFile *file;

  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_DIAGNOSTICS_MANAGER (self->diagnostics_manager));
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);

  g_assert (file != NULL);
  g_assert (G_IS_FILE (file));

  _ide_diagnostics_manager_language_changed (self->diagnostics_manager, file, language_id);
}

static void
gbp_codeui_buffer_addin_load (IdeBufferAddin *addin,
                              IdeBuffer      *buffer)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;
  g_autoptr(IdeContext) context = NULL;
  IdeDiagnosticsManager *manager;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  context = ide_buffer_ref_context (buffer);
  manager = ide_diagnostics_manager_from_context (context);

  self->buffer = g_object_ref (buffer);
  self->diagnostics_manager = g_object_ref (manager);

  g_signal_connect_object (self->diagnostics_manager,
                           "changed",
                           G_CALLBACK (gbp_codeui_buffer_addin_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_codeui_buffer_addin_unload (IdeBufferAddin *addin,
                                IdeBuffer      *buffer)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);

  g_signal_handlers_disconnect_by_func (self->diagnostics_manager,
                                        G_CALLBACK (gbp_codeui_buffer_addin_changed_cb),
                                        self);

  _ide_diagnostics_manager_file_closed (self->diagnostics_manager, file);

  g_clear_object (&self->file);
  g_clear_object (&self->diagnostics_manager);
}

static gboolean
gbp_codeui_buffer_addin_cancel_format_on_save (gpointer user_data)
{
  GCancellable *cancellable = user_data;
  g_assert (G_IS_CANCELLABLE (cancellable));
  g_cancellable_cancel (cancellable);
  return G_SOURCE_REMOVE;
}

static void
gbp_codeui_buffer_addin_format_on_save_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  /* First check for cancellation */
  if (ide_task_had_error (task) ||
      ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  /* If we fail get text edits for formatting the selection, just
   * bail and consider the buffer "settlted". Better to not touch
   * anything if the LSP and/or formatter fail for us.
   */
  if (!ide_buffer_format_selection_finish (buffer, result, &error))
    {
      IdeObjectBox *box;

      if ((box = ide_object_box_from_object (G_OBJECT (buffer))))
        ide_object_warning (IDE_OBJECT (box),
                            _("Failed to format while saving document: %s"),
                            error->message);
    }

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_codeui_buffer_addin_settle_async (IdeBufferAddin      *addin,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GbpCodeuiBufferAddin *self = (GbpCodeuiBufferAddin *)addin;
  g_autoptr(IdeFormatterOptions) options = NULL;
  g_autoptr(GCancellable) local_cancellable = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeFileSettings *file_settings;
  IdeApplication *app;
  GSource *cancel_format_source;
  gboolean insert_spaces;
  guint tab_width;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_BUFFER (self->buffer));

  app = IDE_APPLICATION_DEFAULT;

  /* We use our timeout cancellable instead of @cancellable so that
   * we are in control of cancellation of formatting without affecting
   * the cancellation of other save flows.
   */
  local_cancellable = g_cancellable_new ();

  task = ide_task_new (self, local_cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_codeui_buffer_addin_settle_async);
  ide_task_set_release_on_propagate (task, FALSE);
  ide_task_set_return_on_cancel (task, TRUE);

  /* Make sure the user enabled "format-on-save" */
  if (!g_settings_get_boolean (app->settings, "format-on-save"))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /* Make sure we even have a formatter to work with */
  if (ide_buffer_get_formatter (self->buffer) == NULL)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /* Can't do anything if there are no file settings to access, as
   * we don't have access to the values from the view. We could
   * eventually coordinate with a UI element on that, but probably
   * not worth the layer violations.
   */
  if (!(file_settings = ide_buffer_get_file_settings (self->buffer)))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  /* Setup options for the formatter, right now that only includes
   * a couple small things, tab size and if spaces should be used.
   */
  tab_width = ide_file_settings_get_tab_width (file_settings);
  insert_spaces = ide_file_settings_get_indent_style (file_settings) == IDE_INDENT_STYLE_SPACES;
  options = ide_formatter_options_new ();
  ide_formatter_options_set_tab_width (options, tab_width);
  ide_formatter_options_set_insert_spaces (options, insert_spaces);

  /* LSPs can be finicky, and take a really long time to serve requests.
   * If that happens, we don't want to block forever and be annoying.
   * Instead, bail after a short timeout.
   */
  cancel_format_source = g_timeout_source_new (FORMAT_ON_SAVE_TIMEOUT_MSEC);
  g_source_set_priority (cancel_format_source, G_PRIORITY_HIGH);
  g_source_set_name (cancel_format_source, "[ide-buffer-format-on-save]");
  g_source_set_callback (cancel_format_source,
                         gbp_codeui_buffer_addin_cancel_format_on_save,
                         g_object_ref (local_cancellable),
                         g_object_unref);
  g_source_attach (cancel_format_source, NULL);
  g_source_unref (cancel_format_source);

  /* Request the text edits to format. Changes will be applyed during
   * the callback, after which point we can consider our plugin settled.
   */
  ide_buffer_format_selection_async (self->buffer,
                                     options,
                                     local_cancellable,
                                     gbp_codeui_buffer_addin_format_on_save_cb,
                                     g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_codeui_buffer_addin_settle_finish (IdeBufferAddin  *addin,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_autoptr(GError) local_error = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_CODEUI_BUFFER_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  if (!(ret = ide_task_propagate_boolean (IDE_TASK (result), &local_error)))
    {
      if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        IDE_RETURN (TRUE);
      g_propagate_error (error, g_steal_pointer (&local_error));
    }

  IDE_RETURN (ret);
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->change_settled = gbp_codeui_buffer_addin_change_settled;
  iface->file_saved = gbp_codeui_buffer_addin_file_saved;
  iface->file_loaded = gbp_codeui_buffer_addin_file_loaded;
  iface->language_set = gbp_codeui_buffer_addin_language_set;
  iface->load = gbp_codeui_buffer_addin_load;
  iface->unload = gbp_codeui_buffer_addin_unload;
  iface->settle_async = gbp_codeui_buffer_addin_settle_async;
  iface->settle_finish = gbp_codeui_buffer_addin_settle_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodeuiBufferAddin, gbp_codeui_buffer_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_codeui_buffer_addin_class_init (GbpCodeuiBufferAddinClass *klass)
{
}

static void
gbp_codeui_buffer_addin_init (GbpCodeuiBufferAddin *self)
{
}
