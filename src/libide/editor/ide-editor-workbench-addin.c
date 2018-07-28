/* ide-editor-workbench-addin.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-editor-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <string.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer.h"
#include "diagnostics/ide-source-location.h"
#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-workbench-addin.h"
#include "util/ide-gtk.h"
#include "util/ide-gtk.h"
#include "workbench/ide-workbench.h"
#include "workbench/ide-workbench-header-bar.h"
#include "threading/ide-task.h"

struct _IdeEditorWorkbenchAddin
{
  GObject               parent_instance;

  /* Owned references */
  DzlSignalGroup       *buffer_manager_signals;
  DzlDockManager       *manager;

  /* Borrowed references */
  IdeWorkbench         *workbench;
  IdeEditorPerspective *perspective;
  GtkBox               *panels_box;
  DzlMenuButton        *new_button;
};

typedef struct
{
  IdeWorkbenchOpenFlags flags;
  IdeUri               *uri;
} OpenFileTaskData;

static void ide_workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeEditorWorkbenchAddin, ide_editor_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                               ide_workbench_addin_iface_init))

static void
open_file_task_data_free (gpointer data)
{
  OpenFileTaskData *td = data;

  ide_uri_unref (td->uri);
  g_slice_free (OpenFileTaskData, td);
}

static void
ide_editor_workbench_addin_on_load_buffer (IdeEditorWorkbenchAddin *self,
                                           IdeBuffer               *buffer,
                                           gboolean                 create_new_view,
                                           IdeBufferManager        *buffer_manager)
{
  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  /*
   * We only want to create a new view when the buffer is originally
   * created, not when it's reloaded.
   */
  if (!create_new_view)
    {
      ide_buffer_manager_set_focus_buffer (buffer_manager, buffer);
      return;
    }

  IDE_TRACE_MSG ("Loading %s", ide_buffer_get_title (buffer));

  ide_editor_perspective_focus_buffer (self->perspective, buffer);
}

static void
bind_buffer_manager (IdeEditorWorkbenchAddin *self,
                     IdeBufferManager        *buffer_manager,
                     DzlSignalGroup          *signal_group)
{
  guint n_items;

  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (DZL_IS_SIGNAL_GROUP (signal_group));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (buffer_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = NULL;

      buffer = g_list_model_get_item (G_LIST_MODEL (buffer_manager), i);
      ide_editor_perspective_focus_buffer (self->perspective, buffer);
    }
}

static void
ide_editor_workbench_addin_finalize (GObject *object)
{
  IdeEditorWorkbenchAddin *self = (IdeEditorWorkbenchAddin *)object;

  g_clear_object (&self->buffer_manager_signals);

  G_OBJECT_CLASS (ide_editor_workbench_addin_parent_class)->finalize (object);
}

static void
ide_editor_workbench_addin_class_init (IdeEditorWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_editor_workbench_addin_finalize;
}

static void
ide_editor_workbench_addin_init (IdeEditorWorkbenchAddin *self)
{
  self->buffer_manager_signals = dzl_signal_group_new (IDE_TYPE_BUFFER_MANAGER);

  g_signal_connect_swapped (self->buffer_manager_signals,
                            "bind",
                            G_CALLBACK (bind_buffer_manager),
                            self);

  dzl_signal_group_connect_swapped (self->buffer_manager_signals,
                                    "load-buffer",
                                    G_CALLBACK (ide_editor_workbench_addin_on_load_buffer),
                                    self);
}

static void
ide_editor_workbench_addin_add_buttons (IdeEditorWorkbenchAddin *self,
                                        IdeWorkbenchHeaderBar   *header)
{
  GtkWidget *button;

  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH_HEADER_BAR (header));

  self->panels_box = g_object_new (GTK_TYPE_BOX,
                                   "visible", TRUE,
                                   NULL);
  g_signal_connect (self->panels_box,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->panels_box);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->panels_box), "linked");
  ide_workbench_header_bar_insert_left (header, GTK_WIDGET (self->panels_box), GTK_PACK_START, 10);

  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "action-name", "dockbin.left-visible",
                         "focus-on-click", FALSE,
                         "tooltip-text", _("Toggle navigation panel"),
                         "child", g_object_new (GTK_TYPE_IMAGE,
                                                "icon-name", "builder-view-left-pane-symbolic",
                                                "margin-start", 12,
                                                "margin-end", 12,
                                                "visible", TRUE,
                                                NULL),
                         "visible", TRUE,
                         NULL);
  gtk_container_add (GTK_CONTAINER (self->panels_box), button);

  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "action-name", "dockbin.bottom-visible",
                         "focus-on-click", FALSE,
                         "tooltip-text", _("Toggle utilities panel"),
                         "child", g_object_new (GTK_TYPE_IMAGE,
                                                "icon-name", "builder-view-bottom-pane-symbolic",
                                                "margin-start", 12,
                                                "margin-end", 12,
                                                "visible", TRUE,
                                                NULL),
                         "visible", TRUE,
                         NULL);
  gtk_container_add (GTK_CONTAINER (self->panels_box), button);

  self->new_button = g_object_new (DZL_TYPE_MENU_BUTTON,
                                   "icon-name", "document-open-symbolic",
                                   "focus-on-click", FALSE,
                                   "show-arrow", TRUE,
                                   "show-icons", FALSE,
                                   "show-accels", FALSE,
                                   "menu-id", "new-document-menu",
                                   "visible", TRUE,
                                   NULL);
  g_signal_connect (self->new_button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->new_button);
  ide_workbench_header_bar_add_primary (header, GTK_WIDGET (self->new_button));
}

static void
ide_editor_workbench_addin_load (IdeWorkbenchAddin *addin,
                                 IdeWorkbench      *workbench)
{
  IdeEditorWorkbenchAddin *self = (IdeEditorWorkbenchAddin *)addin;
  IdeWorkbenchHeaderBar *header;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (self->manager == NULL);
  g_assert (self->workbench == NULL);

  self->workbench = workbench;
  self->manager = dzl_dock_manager_new ();

  context = ide_workbench_get_context (workbench);
  buffer_manager = ide_context_get_buffer_manager (context);

  header = ide_workbench_get_headerbar (workbench);

  ide_editor_workbench_addin_add_buttons (self, header);

  self->perspective = g_object_new (IDE_TYPE_EDITOR_PERSPECTIVE,
                                    "manager", self->manager,
                                    "visible", TRUE,
                                    NULL);
  g_signal_connect (self->perspective,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->perspective);
  ide_workbench_add_perspective (workbench, IDE_PERSPECTIVE (self->perspective));

  dzl_signal_group_set_target (self->buffer_manager_signals, buffer_manager);
}

static void
ide_editor_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  IdeEditorWorkbenchAddin *self = (IdeEditorWorkbenchAddin *)addin;

  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  dzl_signal_group_set_target (self->buffer_manager_signals, NULL);

  gtk_widget_destroy (GTK_WIDGET (self->perspective));
  gtk_widget_destroy (GTK_WIDGET (self->panels_box));

  g_clear_object (&self->manager);

  self->workbench = NULL;
}

static gboolean
ide_editor_workbench_addin_can_open (IdeWorkbenchAddin *addin,
                                     IdeUri            *uri,
                                     const gchar       *content_type,
                                     gint              *priority)
{
  const gchar *path;

  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (addin));
  g_assert (uri != NULL);
  g_assert (priority != NULL);

  *priority = 0;

  path = ide_uri_get_path (uri);

  if ((path != NULL) || (content_type != NULL))
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
      gchar *text_type;
      gboolean ret;

      text_type = g_content_type_from_mime_type ("text/plain");
      ret = g_content_type_is_a (content_type, text_type);
      g_free (text_type);

      return ret;
    }

  return FALSE;
}

static void
ide_editor_workbench_addin_open_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  IdeEditorWorkbenchAddin *self;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  const gchar *fragment;
  OpenFileTaskData *open_file_task_data;
  IdeUri *uri;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (self));

  open_file_task_data = ide_task_get_task_data (task);

  buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error);

  if (buffer == NULL)
    {
      IDE_TRACE_MSG ("%s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  uri = open_file_task_data->uri;
  fragment = ide_uri_get_fragment (uri);

  if (fragment != NULL)
    {
      guint line = 0;
      guint column = 0;

      if (sscanf (fragment, "L%u_%u", &line, &column) >= 1)
        {
          g_autoptr(IdeSourceLocation) location = NULL;

          location = ide_source_location_new (ide_buffer_get_file (buffer), line, column, 0);
          ide_editor_perspective_focus_location (self->perspective, location);
        }
    }

  if (self->perspective != NULL &&
      !(open_file_task_data->flags & IDE_WORKBENCH_OPEN_FLAGS_NO_VIEW) &&
      !(open_file_task_data->flags & IDE_WORKBENCH_OPEN_FLAGS_BACKGROUND))
    ide_editor_perspective_focus_buffer_in_current_stack (self->perspective, buffer);

  ide_task_return_boolean (task, TRUE);
}

static void
ide_editor_workbench_addin_open_async (IdeWorkbenchAddin    *addin,
                                       IdeUri               *uri,
                                       const gchar          *content_type,
                                       IdeWorkbenchOpenFlags flags,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data)
{
  IdeEditorWorkbenchAddin *self = (IdeEditorWorkbenchAddin *)addin;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  OpenFileTaskData *open_file_task_data;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeFile) ifile = NULL;
  g_autoptr(GFile) gfile = NULL;

  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (self));
  g_assert (uri != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  task = ide_task_new (self, cancellable, callback, user_data);
  open_file_task_data = g_slice_new0 (OpenFileTaskData);
  open_file_task_data->flags = flags;
  open_file_task_data->uri = ide_uri_ref(uri);
  ide_task_set_task_data (task, open_file_task_data, open_file_task_data_free);

  context = ide_workbench_get_context (self->workbench);
  buffer_manager = ide_context_get_buffer_manager (context);

  gfile = ide_uri_to_file (uri);

  if (gfile == NULL)
    {
      g_autofree gchar *uristr = NULL;

      uristr = ide_uri_to_string (uri, IDE_URI_HIDE_AUTH_PARAMS);
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "Failed to create resource for \"%s\"",
                                 uristr);
      return;
    }

  ifile = ide_file_new (context, gfile);

  ide_buffer_manager_load_file_async (buffer_manager,
                                      ifile,
                                      FALSE,
                                      flags,
                                      NULL,
                                      cancellable,
                                      ide_editor_workbench_addin_open_cb,
                                      g_object_ref (task));
}

static gboolean
ide_editor_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gchar *
ide_editor_workbench_addin_get_id (IdeWorkbenchAddin *addin)
{
  return g_strdup ("editor");
}

static void
ide_editor_workbench_addin_perspective_set (IdeWorkbenchAddin *addin,
                                            IdePerspective    *perspective)
{
  IdeEditorWorkbenchAddin *self = (IdeEditorWorkbenchAddin *)addin;

  g_assert (IDE_IS_EDITOR_WORKBENCH_ADDIN (self));

  gtk_widget_set_visible (GTK_WIDGET (self->panels_box),
                          IDE_IS_EDITOR_PERSPECTIVE (perspective));
  gtk_widget_set_visible (GTK_WIDGET (self->new_button),
                          IDE_IS_EDITOR_PERSPECTIVE (perspective));
}

static void
ide_workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->can_open = ide_editor_workbench_addin_can_open;
  iface->get_id = ide_editor_workbench_addin_get_id;
  iface->load = ide_editor_workbench_addin_load;
  iface->open_async = ide_editor_workbench_addin_open_async;
  iface->open_finish = ide_editor_workbench_addin_open_finish;
  iface->unload = ide_editor_workbench_addin_unload;
  iface->perspective_set = ide_editor_workbench_addin_perspective_set;
}
