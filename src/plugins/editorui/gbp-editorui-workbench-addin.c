/* gbp-editorui-workbench-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editorui-workbench-addin"

#include "config.h"

#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-sourceview.h>

#include "gbp-editorui-workbench-addin.h"

struct _GbpEditoruiWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

typedef struct
{
  PanelPosition      *position;
  GFile              *file;
  IdeBufferOpenFlags  flags;
  gint                at_line;
  gint                at_line_offset;
} OpenFileTaskData;

static GHashTable *overrides;

static void
open_file_task_data_free (gpointer data)
{
  OpenFileTaskData *td = data;

  g_clear_object (&td->file);
  g_clear_pointer (&td->position, g_object_unref);
  g_slice_free (OpenFileTaskData, td);
}

static void
gbp_editorui_workbench_addin_load (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GbpEditoruiWorkbenchAddin *self = (GbpEditoruiWorkbenchAddin *)addin;

  g_assert (GBP_IS_EDITORUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_editorui_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  GbpEditoruiWorkbenchAddin *self = (GbpEditoruiWorkbenchAddin *)addin;

  g_assert (GBP_IS_EDITORUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static gboolean
gbp_editorui_workbench_addin_can_open (IdeWorkbenchAddin *addin,
                                       GFile             *file,
                                       const gchar       *content_type,
                                       gint              *priority)
{
  const char *path;

  g_assert (GBP_IS_EDITORUI_WORKBENCH_ADDIN (addin));
  g_assert (G_IS_FILE (file));
  g_assert (priority != NULL);

  *priority = 0;

  path = g_file_peek_path (file);

  if (path != NULL || content_type != NULL)
    {
      GtkSourceLanguageManager *manager;
      GtkSourceLanguage *language;

      manager = gtk_source_language_manager_get_default ();
      language = gtk_source_language_manager_guess_language (manager, path, content_type);

      if (language != NULL)
        return TRUE;
    }

  /* Escape hatch in case shared-mime-info fails us */
  if (path != NULL)
    {
      const char *suffix = strrchr (path, '.');

      if (suffix && g_hash_table_contains (overrides, suffix))
        return TRUE;
    }

  if (content_type != NULL)
    {
      static char *text_plain_type;
      static char *x_zerosize_type;

      if G_UNLIKELY (text_plain_type == NULL)
        text_plain_type = g_content_type_from_mime_type ("text/plain");

      if G_UNLIKELY (x_zerosize_type == NULL)
        x_zerosize_type = g_content_type_from_mime_type ("application/x-zerosize");

      if (g_content_type_is_a (content_type, x_zerosize_type) ||
          g_content_type_is_a (content_type, text_plain_type))
        return TRUE;
    }

  return FALSE;
}

static void
find_preferred_workspace_cb (IdeWorkspace *workspace,
                             gpointer      user_data)
{
  IdeWorkspace **out_workspace = user_data;

  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (out_workspace != NULL);
  g_assert (*out_workspace == NULL || IDE_IS_WORKSPACE (*out_workspace));

  if (IDE_IS_PRIMARY_WORKSPACE (workspace))
    *out_workspace = workspace;
  else if (*out_workspace == NULL && IDE_IS_EDITOR_WORKSPACE (workspace))
    *out_workspace = workspace;
}

static void
gbp_editorui_workbench_addin_open_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  GbpEditoruiWorkbenchAddin *self;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  OpenFileTaskData *state;
  IdeWorkspace *workspace;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error);

  g_assert (GBP_IS_EDITORUI_WORKBENCH_ADDIN (self));
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  if (buffer == NULL)
    {
      IDE_TRACE_MSG ("Failed to load buffer: %s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (self->workbench == NULL)
    IDE_GOTO (failure);

  workspace = ide_workbench_get_current_workspace (self->workbench);

  if (!IDE_IS_PRIMARY_WORKSPACE (workspace) &&
      !IDE_IS_EDITOR_WORKSPACE (workspace))
    {
      workspace = NULL;
      ide_workbench_foreach_workspace (self->workbench,
                                       find_preferred_workspace_cb,
                                       &workspace);
    }

  if (workspace == NULL)
    IDE_GOTO (failure);

  state = ide_task_get_task_data (task);

  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));

  if (state->at_line > -1)
    {
      g_autoptr(IdeLocation) location = NULL;

      location = ide_location_new (state->file,
                                   state->at_line,
                                   state->at_line_offset);
      ide_editor_focus_location (workspace, state->position, location);
    }
  else
    {
      ide_editor_focus_buffer (workspace, state->position, buffer);
    }

failure:
  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_editorui_workbench_addin_open_async (IdeWorkbenchAddin   *addin,
                                         GFile               *file,
                                         const gchar         *content_type,
                                         gint                 at_line,
                                         gint                 at_line_offset,
                                         IdeBufferOpenFlags   flags,
                                         PanelPosition       *position,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  GbpEditoruiWorkbenchAddin *self = (GbpEditoruiWorkbenchAddin *)addin;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  OpenFileTaskData *state;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_EDITORUI_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_WORKBENCH (self->workbench));
  g_assert (position != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  state = g_slice_new0 (OpenFileTaskData);
  state->flags = flags;
  state->file = g_object_ref (file);
  state->at_line = at_line;
  state->at_line_offset = at_line_offset;
  state->position = g_object_ref (position);
  ide_task_set_task_data (task, state, open_file_task_data_free);

  context = ide_workbench_get_context (self->workbench);
  buffer_manager = ide_buffer_manager_from_context (context);

  ide_buffer_manager_load_file_async (buffer_manager,
                                      file,
                                      state->flags,
                                      NULL,
                                      cancellable,
                                      gbp_editorui_workbench_addin_open_cb,
                                      g_steal_pointer (&task));
}

static gboolean
gbp_editorui_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                          GAsyncResult       *result,
                                          GError            **error)
{
  g_assert (GBP_IS_EDITORUI_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_editorui_workbench_addin_load;
  iface->unload = gbp_editorui_workbench_addin_unload;
  iface->can_open = gbp_editorui_workbench_addin_can_open;
  iface->open_async = gbp_editorui_workbench_addin_open_async;
  iface->open_finish = gbp_editorui_workbench_addin_open_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditoruiWorkbenchAddin, gbp_editorui_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_editorui_workbench_addin_class_init (GbpEditoruiWorkbenchAddinClass *klass)
{
  overrides = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_add (overrides, (char *)".dts"); /* #1572 */
  g_hash_table_add (overrides, (char *)".mod"); /* #1863 */
}

static void
gbp_editorui_workbench_addin_init (GbpEditoruiWorkbenchAddin *self)
{
}
