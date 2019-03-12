/* gbp-editor-workbench-addin.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-editor-workbench-addin"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libide-code.h>
#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-io.h>
#include <libide-threading.h>
#include <string.h>

#include "gbp-editor-workbench-addin.h"

struct _GbpEditorWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

typedef struct
{
  GFile              *file;
  IdeBufferOpenFlags  flags;
  gint                at_line;
  gint                at_line_offset;
} OpenFileTaskData;

static void ide_workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpEditorWorkbenchAddin, gbp_editor_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                               ide_workbench_addin_iface_init))

static void
open_file_task_data_free (gpointer data)
{
  OpenFileTaskData *td = data;

  g_clear_object (&td->file);
  g_slice_free (OpenFileTaskData, td);
}

static void
gbp_editor_workbench_addin_class_init (GbpEditorWorkbenchAddinClass *klass)
{
}

static void
gbp_editor_workbench_addin_init (GbpEditorWorkbenchAddin *self)
{
}

static void
gbp_editor_workbench_addin_load (IdeWorkbenchAddin *addin,
                                 IdeWorkbench      *workbench)
{
  GbpEditorWorkbenchAddin *self = (GbpEditorWorkbenchAddin *)addin;

  g_assert (GBP_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (self->workbench == NULL);

  self->workbench = workbench;
}

static void
gbp_editor_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GbpEditorWorkbenchAddin *self = (GbpEditorWorkbenchAddin *)addin;

  g_assert (GBP_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static gboolean
gbp_editor_workbench_addin_can_open (IdeWorkbenchAddin *addin,
                                     GFile             *file,
                                     const gchar       *content_type,
                                     gint              *priority)
{
  const gchar *path;

  g_assert (GBP_IS_EDITOR_WORKBENCH_ADDIN (addin));
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

  if (content_type != NULL)
    {
      g_autofree gchar *text_type = NULL;

      text_type = g_content_type_from_mime_type ("text/plain");
      return g_content_type_is_a (content_type, text_type);
    }

  return FALSE;
}

static void
find_workspace_surface_cb (GtkWidget *widget,
                           gpointer   user_data)
{
  IdeSurface **surface = user_data;

  g_assert (IDE_IS_WORKSPACE (widget));
  g_assert (surface != NULL);
  g_assert (*surface == NULL || IDE_IS_SURFACE (*surface));

  if (*surface == NULL)
    {
      *surface = ide_workspace_get_surface_by_name (IDE_WORKSPACE (widget), "editor");
      if (!IDE_IS_EDITOR_SURFACE (*surface))
        *surface = NULL;
    }
}

static void
gbp_editor_workbench_addin_open_at_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  GbpEditorWorkbenchAddin *self;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  OpenFileTaskData *state;
  IdeEditorSurface *surface = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_EDITOR_WORKBENCH_ADDIN (self));

  buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error);

  if (buffer == NULL)
    {
      IDE_TRACE_MSG ("%s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (self->workbench == NULL)
    goto failure;

  ide_workbench_foreach_workspace (self->workbench,
                                   find_workspace_surface_cb,
                                   &surface);

  if (!IDE_IS_EDITOR_SURFACE (surface))
    goto failure;

  state = ide_task_get_task_data (task);

  if (state->at_line > -1)
    {
      g_autoptr(IdeLocation) location = NULL;

      location = ide_location_new (state->file,
                                   state->at_line,
                                   state->at_line_offset);
      ide_editor_surface_focus_location (surface, location);
    }

  if (surface != NULL &&
      !(state->flags & IDE_BUFFER_OPEN_FLAGS_NO_VIEW) &&
      !(state->flags & IDE_BUFFER_OPEN_FLAGS_BACKGROUND))
    ide_editor_surface_focus_buffer_in_current_stack (surface, buffer);

failure:
  ide_task_return_boolean (task, TRUE);
}

static void
gbp_editor_workbench_addin_open_at_async (IdeWorkbenchAddin   *addin,
                                          GFile               *file,
                                          const gchar         *content_type,
                                          gint                 at_line,
                                          gint                 at_line_offset,
                                          IdeBufferOpenFlags   flags,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GbpEditorWorkbenchAddin *self = (GbpEditorWorkbenchAddin *)addin;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  OpenFileTaskData *state;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  task = ide_task_new (self, cancellable, callback, user_data);
  state = g_slice_new0 (OpenFileTaskData);
  state->flags = flags;
  state->file = g_object_ref (file);
  state->at_line = at_line;
  state->at_line_offset = at_line_offset;
  ide_task_set_task_data (task, state, open_file_task_data_free);

  context = ide_workbench_get_context (self->workbench);
  buffer_manager = ide_buffer_manager_from_context (context);

  ide_buffer_manager_load_file_async (buffer_manager,
                                      file,
                                      state->flags,
                                      NULL,
                                      cancellable,
                                      gbp_editor_workbench_addin_open_at_cb,
                                      g_steal_pointer (&task));
}

static void
gbp_editor_workbench_addin_open_async (IdeWorkbenchAddin   *addin,
                                       GFile               *file,
                                       const gchar         *content_type,
                                       IdeBufferOpenFlags   flags,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  gbp_editor_workbench_addin_open_at_async (addin, file, content_type, -1, -1, flags, cancellable, callback, user_data);
}

static gboolean
gbp_editor_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  g_assert (GBP_IS_EDITOR_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
new_editor_workspace_cb (GSimpleAction *action,
                         GVariant      *param,
                         gpointer       user_data)
{
  GbpEditorWorkbenchAddin *self = user_data;
  IdeWorkspace *workspace;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_EDITOR_WORKBENCH_ADDIN (self));

  workspace = g_object_new (IDE_TYPE_EDITOR_WORKSPACE,
                            "application", IDE_APPLICATION_DEFAULT,
                            NULL);
  ide_workbench_add_workspace (self->workbench, workspace);
  ide_gtk_window_present (GTK_WINDOW (workspace));
}

static GActionEntry actions[] = {
  { "new-editor-workspace", new_editor_workspace_cb },
};

static void
gbp_editor_workbench_addin_workspace_added (IdeWorkbenchAddin *addin,
                                            IdeWorkspace      *workspace)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (workspace));

  g_action_map_add_action_entries (G_ACTION_MAP (workspace),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   addin);
}

static void
gbp_editor_workbench_addin_workspace_removed (IdeWorkbenchAddin *addin,
                                              IdeWorkspace      *workspace)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (workspace));

  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (workspace), actions[i].name);
}

static void
ide_workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->can_open = gbp_editor_workbench_addin_can_open;
  iface->load = gbp_editor_workbench_addin_load;
  iface->open_at_async = gbp_editor_workbench_addin_open_at_async;
  iface->open_async = gbp_editor_workbench_addin_open_async;
  iface->open_finish = gbp_editor_workbench_addin_open_finish;
  iface->unload = gbp_editor_workbench_addin_unload;
  iface->workspace_added = gbp_editor_workbench_addin_workspace_added;
  iface->workspace_removed = gbp_editor_workbench_addin_workspace_removed;
}
