/* ide-greeter-perspective.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-greeter-perspective"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-macros.h"

#include "application/ide-application.h"
#include "genesis/ide-genesis-addin.h"
#include "greeter/ide-greeter-perspective.h"
#include "greeter/ide-greeter-project-row.h"
#include "util/ide-gtk.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench-private.h"
#include "workbench/ide-workbench.h"

struct _IdeGreeterPerspective
{
  GtkBin                parent_instance;

  DzlSignalGroup       *signal_group;
  IdeRecentProjects    *recent_projects;
  DzlPatternSpec       *pattern_spec;
  PeasExtensionSet     *genesis_set;

  GBinding             *ready_binding;
  GCancellable         *cancellable;

  GtkStack             *stack;
  GtkStack             *top_stack;
  GtkButton            *genesis_continue_button;
  GtkButton            *genesis_cancel_button;
  GtkLabel             *genesis_title;
  GtkStack             *genesis_stack;
  GtkInfoBar           *info_bar;
  GtkLabel             *info_bar_label;
  GtkRevealer          *info_bar_revealer;
  GtkViewport          *viewport;
  GtkWidget            *titlebar;
  GtkBox               *my_projects_container;
  GtkListBox           *my_projects_list_box;
  GtkButton            *open_button;
  GtkButton            *cancel_button;
  GtkBox               *other_projects_container;
  GtkListBox           *other_projects_list_box;
  GtkButton            *remove_button;
  GtkSearchEntry       *search_entry;
  DzlStateMachine      *state_machine;
  GtkScrolledWindow    *scrolled_window;
  DzlPriorityBox       *genesis_buttons;
  DzlEmptyState        *no_projects_found;

  gint                  selected_count;
};

static void ide_perspective_iface_init (IdePerspectiveInterface *iface);
static void ide_greeter_perspective_genesis_continue (IdeGreeterPerspective *self);

G_DEFINE_TYPE_EXTENDED (IdeGreeterPerspective, ide_greeter_perspective, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE,
                                               ide_perspective_iface_init))

enum {
  PROP_0,
  PROP_RECENT_PROJECTS,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static GtkWidget *
ide_greeter_perspective_get_titlebar (IdePerspective *perspective)
{
  return IDE_GREETER_PERSPECTIVE (perspective)->titlebar;
}

static gchar *
ide_greeter_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("greeter");
}

static gboolean
ide_greeter_perspective_is_early (IdePerspective *perspective)
{
  return TRUE;
};

static void
ide_perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_id = ide_greeter_perspective_get_id;
  iface->get_titlebar = ide_greeter_perspective_get_titlebar;
  iface->is_early = ide_greeter_perspective_is_early;
}

static void
ide_greeter_perspective_first_visible_cb (GtkWidget *widget,
                                          gpointer   user_data)
{
  GtkWidget **row = user_data;

  if ((*row == NULL) && gtk_widget_get_child_visible (widget))
    *row = widget;
}

static void
ide_greeter_perspective__search_entry_activate (IdeGreeterPerspective *self,
                                                GtkSearchEntry        *search_entry)
{
  GtkWidget *row = NULL;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  gtk_container_foreach (GTK_CONTAINER (self->my_projects_list_box),
                         ide_greeter_perspective_first_visible_cb,
                         &row);
  if (row == NULL)
    gtk_container_foreach (GTK_CONTAINER (self->other_projects_list_box),
                           ide_greeter_perspective_first_visible_cb,
                           &row);

  if (row != NULL)
    g_signal_emit_by_name (row, "activate");
}

IdeRecentProjects *
ide_greeter_perspective_get_recent_projects (IdeGreeterPerspective *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_PERSPECTIVE (self), NULL);

  return self->recent_projects;
}

static void
ide_greeter_perspective_apply_filter_cb (GtkWidget *widget,
                                   gpointer   user_data)
{
  gboolean *visible = user_data;

  g_assert (IDE_IS_GREETER_PROJECT_ROW (widget));

  if (gtk_widget_get_child_visible (widget))
    *visible = TRUE;
}

static void
ide_greeter_perspective_apply_filter (IdeGreeterPerspective *self,
                                      GtkListBox            *list_box,
                                      GtkWidget             *container)
{
  gboolean visible = FALSE;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_CONTAINER (container));

  gtk_list_box_invalidate_filter (list_box);
  gtk_container_foreach (GTK_CONTAINER (list_box), ide_greeter_perspective_apply_filter_cb, &visible);
  gtk_widget_set_visible (GTK_WIDGET (container), visible);
}

static void
ide_greeter_perspective_apply_filter_all (IdeGreeterPerspective *self)
{
  const gchar *text;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  g_clear_pointer (&self->pattern_spec, dzl_pattern_spec_unref);
  if ((text = gtk_entry_get_text (GTK_ENTRY (self->search_entry))))
    self->pattern_spec = dzl_pattern_spec_new (text);

  ide_greeter_perspective_apply_filter (self,
                                  self->my_projects_list_box,
                                  GTK_WIDGET (self->my_projects_container));
  ide_greeter_perspective_apply_filter (self,
                                  self->other_projects_list_box,
                                  GTK_WIDGET (self->other_projects_container));

  if (gtk_widget_get_visible (GTK_WIDGET (self->my_projects_container)) == FALSE &&
      gtk_widget_get_visible (GTK_WIDGET (self->other_projects_container)) == FALSE)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->no_projects_found), TRUE);
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (self->no_projects_found), FALSE);
    }
}

static void
ide_greeter_perspective__search_entry_changed (IdeGreeterPerspective *self,
                                               GtkSearchEntry        *search_entry)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  ide_greeter_perspective_apply_filter_all (self);
}

static gboolean
row_focus_in_event (IdeGreeterPerspective *self,
                    GdkEventFocus         *focus,
                    IdeGreeterProjectRow  *row)
{
  GtkAllocation alloc;
  GtkAllocation row_alloc;
  gint dest_x;
  gint dest_y;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  gtk_widget_get_allocation (GTK_WIDGET (self->viewport), &alloc);
  gtk_widget_get_allocation (GTK_WIDGET (row), &row_alloc);

  /*
   * If we are smaller than the visible area, don't do anything for now.
   * This can happen during creation of the window and resize process.
   */
  if (row_alloc.height > alloc.height)
    return GDK_EVENT_PROPAGATE;

  if (gtk_widget_translate_coordinates (GTK_WIDGET (row), GTK_WIDGET (self->viewport), 0, 0, &dest_x, &dest_y))
    {
      gint distance = 0;

      if (dest_y < 0)
        {
          distance = dest_y;
        }
      else if ((dest_y + row_alloc.height) > alloc.height)
        {
          distance = dest_y + row_alloc.height - alloc.height;
        }

      if (distance != 0)
        {
          GtkAdjustment *vadj;
          gdouble value;

          vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->viewport));
          value = gtk_adjustment_get_value (vadj);
          gtk_adjustment_set_value (vadj, value + distance);
        }
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
selection_to_true (GBinding     *binding,
                   const GValue *from_value,
                   GValue       *to_value,
                   gpointer      user_data)
{
  if (G_VALUE_HOLDS_STRING (from_value) && G_VALUE_HOLDS_BOOLEAN (to_value))
    {
      const gchar *str;

      str = g_value_get_string (from_value);
      g_value_set_boolean (to_value, ide_str_equal0 (str, "selection"));

      return TRUE;
    }

  return FALSE;
}

static void
ide_greeter_perspective__row_notify_selected (IdeGreeterPerspective *self,
                                              GParamSpec            *pspec,
                                              IdeGreeterProjectRow  *row)
{
  gboolean selected = FALSE;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_GREETER_PROJECT_ROW (row));

  g_object_get (row, "selected", &selected, NULL);
  self->selected_count += selected ? 1 : -1;

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "greeter", "delete-selected-rows",
                             "enabled", self->selected_count > 0,
                             NULL);
}

static void
recent_projects_items_changed (IdeGreeterPerspective *self,
                               guint                  position,
                               guint                  removed,
                               guint                  added,
                               GListModel            *list_model)
{
  IdeGreeterProjectRow *row;
  gsize i;

  /*
   * TODO: We ignore removed out of simplicity for now.
   *       But IdeRecentProjects doesn't currently remove anything anyway.
   */

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (G_IS_LIST_MODEL (list_model));
  g_assert (IDE_IS_RECENT_PROJECTS (list_model));

  if (g_list_model_get_n_items (list_model) > 0)
    {
      if (ide_str_equal0 ("empty-state", gtk_stack_get_visible_child_name (self->stack)))
        gtk_stack_set_visible_child_name (self->stack, "projects");
    }

  for (i = 0; i < added; i++)
    {
      IdeProjectInfo *project_info;
      GtkListBox *list_box;

      project_info = g_list_model_get_item (list_model, position + i);

      row = g_object_new (IDE_TYPE_GREETER_PROJECT_ROW,
                          "visible", TRUE,
                          "project-info", project_info,
                          NULL);
      g_signal_connect_object (row,
                               "focus-in-event",
                               G_CALLBACK (row_focus_in_event),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (row,
                               "notify::selected",
                               G_CALLBACK (ide_greeter_perspective__row_notify_selected),
                               self,
                               G_CONNECT_SWAPPED);

      if (ide_project_info_get_is_recent (project_info))
        {
          list_box = self->my_projects_list_box;
          g_object_bind_property_full (self->state_machine, "state",
                                       row, "selection-mode",
                                       G_BINDING_SYNC_CREATE,
                                       selection_to_true, NULL,
                                       NULL, NULL);
        }
      else
        {
          list_box = self->other_projects_list_box;
        }

      gtk_container_add (GTK_CONTAINER (list_box), GTK_WIDGET (row));
    }

  ide_greeter_perspective_apply_filter_all (self);
}

static gint
ide_greeter_perspective_sort_rows (GtkListBoxRow *row1,
                                   GtkListBoxRow *row2,
                                   gpointer       user_data)
{
  IdeProjectInfo *info1;
  IdeProjectInfo *info2;

  info1 = ide_greeter_project_row_get_project_info (IDE_GREETER_PROJECT_ROW (row1));
  info2 = ide_greeter_project_row_get_project_info (IDE_GREETER_PROJECT_ROW (row2));

  return ide_project_info_compare (info1, info2);
}

static void
ide_greeter_perspective_set_recent_projects (IdeGreeterPerspective *self,
                                             IdeRecentProjects     *recent_projects)
{
  g_return_if_fail (IDE_IS_GREETER_PERSPECTIVE (self));
  g_return_if_fail (!recent_projects || IDE_IS_RECENT_PROJECTS (recent_projects));

  if (g_set_object (&self->recent_projects, recent_projects))
    {
      dzl_signal_group_set_target (self->signal_group, recent_projects);

      if (recent_projects != NULL)
        {
          GListModel *list_model;
          guint n_items;

          list_model = G_LIST_MODEL (recent_projects);
          n_items = g_list_model_get_n_items (list_model);
          recent_projects_items_changed (self, 0, 0, n_items, list_model);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RECENT_PROJECTS]);
    }
}

static gboolean
ide_greeter_perspective_filter_row (GtkListBoxRow *row,
                                    gpointer       user_data)
{
  IdeGreeterPerspective *self = user_data;
  IdeGreeterProjectRow *project_row = (IdeGreeterProjectRow *)row;
  const gchar *search_text;
  gboolean ret;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (IDE_IS_GREETER_PROJECT_ROW (project_row));

  if (self->pattern_spec == NULL)
    return TRUE;

  search_text = ide_greeter_project_row_get_search_text (project_row);
  ret = dzl_pattern_spec_match (self->pattern_spec, search_text);

  return ret;
}

static void
ide_greeter_perspective_open_project_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *)object;
  g_autoptr(IdeGreeterPerspective) self = (IdeGreeterPerspective *)user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  if (!ide_workbench_open_project_finish (workbench, result, &error))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (workbench),
                                       GTK_DIALOG_USE_HEADER_BAR,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Failed to load the project"));

      g_object_set (dialog,
                    "modal", TRUE,
                    "secondary-text", error->message,
                    NULL);

      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), workbench);

      gtk_window_present (GTK_WINDOW (dialog));

      gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->titlebar), TRUE);
    }
}

static void
ide_greeter_perspective__row_activated (IdeGreeterPerspective *self,
                                        IdeGreeterProjectRow  *row,
                                        GtkListBox            *list_box)
{
  IdeProjectInfo *project_info;
  IdeWorkbench *workbench = NULL;
  GFile *project_file;
  GList *list;
  GtkWindow *window;
  IdeContext *context;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (IDE_IS_GREETER_PROJECT_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (ide_str_equal0 (dzl_state_machine_get_state (self->state_machine), "selection"))
    {
      gboolean selected = FALSE;

      g_object_get (row, "selected", &selected, NULL);
      g_object_set (row, "selected", !selected, NULL);
      return;
    }

  project_info = ide_greeter_project_row_get_project_info (row);
  project_file = ide_project_info_get_file (project_info);

  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->titlebar), FALSE);

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));

  list = gtk_application_get_windows (gtk_window_get_application (GTK_WINDOW (workbench)));

  for (; list != NULL; list = list->next)
    {
      window = list->data;
      context = ide_workbench_get_context (IDE_WORKBENCH (window));

      if (context != NULL)
        {
          if (g_file_equal (ide_context_get_project_file (context), project_file))
            {
              gtk_window_present (window);
              gtk_window_close (GTK_WINDOW (workbench));
              workbench = NULL;
              break;
            }
        }
    }

  if(workbench != NULL)
    {
      ide_workbench_open_project_async (workbench,
                                        project_file,
                                        NULL,
                                        ide_greeter_perspective_open_project_cb,
                                        g_object_ref (self));
    }

  ide_project_info_set_is_recent (project_info, TRUE);
}

static gboolean
ide_greeter_perspective__keynav_failed (IdeGreeterPerspective *self,
                                        GtkDirectionType       dir,
                                        GtkListBox            *list_box)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if ((list_box == self->my_projects_list_box) && (dir == GTK_DIR_DOWN))
    {
      gtk_widget_child_focus (GTK_WIDGET (self->other_projects_list_box), GTK_DIR_DOWN);
      return GDK_EVENT_STOP;
    }
  else if ((list_box == self->other_projects_list_box) && (dir == GTK_DIR_UP))
    {
      gtk_widget_child_focus (GTK_WIDGET (self->my_projects_list_box), GTK_DIR_UP);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
delete_selected_rows (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  IdeGreeterPerspective *self = user_data;
  GList *rows;
  GList *iter;
  GList *projects = NULL;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  rows = gtk_container_get_children (GTK_CONTAINER (self->my_projects_list_box));

  for (iter = rows; iter; iter = iter->next)
    {
      IdeGreeterProjectRow *row = iter->data;
      gboolean selected = FALSE;

      g_object_get (row, "selected", &selected, NULL);

      if (selected)
        {
          IdeProjectInfo *info;

          info = ide_greeter_project_row_get_project_info (row);
          projects = g_list_prepend (projects, g_object_ref (info));
          gtk_container_remove (GTK_CONTAINER (self->my_projects_list_box), iter->data);
        }
    }

  g_list_free (rows);

  ide_recent_projects_remove (self->recent_projects, projects);
  g_list_free_full (projects, g_object_unref);

  self->selected_count = 0;
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  dzl_state_machine_set_state (self->state_machine, "browse");

  ide_greeter_perspective_apply_filter_all (self);
}

static void
ide_greeter_perspective_dialog_response (IdeGreeterPerspective *self,
                                         gint                   response_id,
                                         GtkFileChooserDialog  *dialog)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_OK)
    {
      IdeWorkbench *workbench;

      if (NULL != (workbench = ide_widget_get_workbench (GTK_WIDGET (self))))
        {
          g_autoptr(GFile) project_file = NULL;

          gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
          gtk_widget_set_sensitive (GTK_WIDGET (self->titlebar), FALSE);

          project_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
          ide_workbench_open_project_async (workbench, project_file, NULL, NULL, NULL);
        }
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ide_greeter_perspective_dialog_notify_filter (IdeGreeterPerspective *self,
                                              GParamSpec            *pspec,
                                              GtkFileChooserDialog  *dialog)
{
  GtkFileFilter *filter;
  GtkFileChooserAction action;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (pspec != NULL);
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog));

  if (filter && g_object_get_data (G_OBJECT (filter), "IS_DIRECTORY"))
    action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
  else
    action = GTK_FILE_CHOOSER_ACTION_OPEN;

  gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), action);
}

static void
ide_greeter_perspective_open_clicked (IdeGreeterPerspective *self,
                                      GtkButton             *open_button)
{
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *projects_dir = NULL;
  GtkFileChooserDialog *dialog;
  GtkWidget *toplevel;
  PeasEngine *engine;
  const GList *list;
  GtkFileFilter *all_filter;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_BUTTON (open_button));

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!GTK_IS_WINDOW (toplevel))
    toplevel = NULL;

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                         "transient-for", toplevel,
                         "modal", TRUE,
                         "title", _("Open Project"),
                         "visible", TRUE,
                         NULL);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Open"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  g_signal_connect_object (dialog,
                           "notify::filter",
                           G_CALLBACK (ide_greeter_perspective_dialog_notify_filter),
                           self,
                           G_CONNECT_SWAPPED);

  all_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (all_filter, _("All Project Types"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), all_filter);

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      GtkFileFilter *filter;
      const gchar *pattern;
      const gchar *content_type;
      const gchar *name;
      gchar **patterns;
      gchar **content_types;
      gint i;

      if (!peas_plugin_info_is_loaded (plugin_info))
        continue;

      name = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Name");
      if (name == NULL)
        continue;

      pattern = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Pattern");
      content_type = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Content-Type");

      if (pattern == NULL && content_type == NULL)
        continue;

      patterns = g_strsplit (pattern ?: "", ",", 0);
      content_types = g_strsplit (content_type ?: "", ",", 0);

      filter = gtk_file_filter_new ();

      gtk_file_filter_set_name (filter, name);

      for (i = 0; patterns [i] != NULL; i++)
        {
          if (*patterns [i])
            {
              gtk_file_filter_add_pattern (filter, patterns [i]);
              gtk_file_filter_add_pattern (all_filter, patterns [i]);
            }
        }

      for (i = 0; content_types [i] != NULL; i++)
        {
          if (*content_types [i])
            {
              gtk_file_filter_add_mime_type (filter, content_types [i]);
              gtk_file_filter_add_mime_type (all_filter, content_types [i]);

              /* Helper so we can change the file chooser action to OPEN_DIRECTORY,
               * otherwise the user won't be able to choose a directory, it will
               * instead dive into the directory.
               */
              if (g_strcmp0 (content_types [i], "inode/directory") == 0)
                g_object_set_data (G_OBJECT (filter), "IS_DIRECTORY", GINT_TO_POINTER (1));
            }
        }

      gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

      g_strfreev (patterns);
      g_strfreev (content_types);
    }

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (ide_greeter_perspective_dialog_response),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), all_filter);

  settings = g_settings_new ("org.gnome.builder");
  projects_dir = g_settings_get_string (settings, "projects-directory");
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), projects_dir);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
ide_greeter_perspective_cancel_clicked (IdeGreeterPerspective *self,
                                        GtkButton             *cancel_button)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_BUTTON (cancel_button));

  dzl_state_machine_set_state (self->state_machine, "browse");
  ide_greeter_perspective_apply_filter_all (self);
}

void
ide_greeter_perspective_show_genesis_view (IdeGreeterPerspective *self,
                                           const gchar           *genesis_addin_name,
                                           const gchar           *manifest)
{
  GtkWidget *addin;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  addin = gtk_stack_get_child_by_name (self->genesis_stack, genesis_addin_name);
  gtk_stack_set_visible_child (self->genesis_stack, addin);
  dzl_state_machine_set_state (self->state_machine, "genesis");

  if (manifest != NULL)
    {
      g_object_set (addin, "manifest", manifest, NULL);

      gtk_widget_hide (GTK_WIDGET (self->genesis_continue_button));
      ide_greeter_perspective_genesis_continue (self);
    }
}

static void
genesis_button_clicked (IdeGreeterPerspective *self,
                        GtkButton             *button)
{
  const gchar *name;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_BUTTON (button));

  name = gtk_widget_get_name (GTK_WIDGET (button));
  ide_greeter_perspective_show_genesis_view (self, name, NULL);
}

static void
ide_greeter_perspective_genesis_cancel_clicked (IdeGreeterPerspective *self,
                                                GtkButton             *genesis_cancel_button)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_BUTTON (genesis_cancel_button));

  g_cancellable_cancel (self->cancellable);
  dzl_state_machine_set_state (self->state_machine, "browse");
  ide_greeter_perspective_apply_filter_all (self);
}

static void
ide_greeter_perspective_genesis_added (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeGreeterPerspective *self = user_data;
  IdeGenesisAddin *addin = (IdeGenesisAddin *)exten;
  g_autofree gchar *title = NULL;
  GtkWidget *button;
  GtkWidget *child;
  gint priority;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (addin));
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  title = ide_genesis_addin_get_label (addin);
  if (title != NULL)
    {
      priority = ide_genesis_addin_get_priority (addin);
      button = g_object_new (GTK_TYPE_BUTTON,
                             "name", G_OBJECT_TYPE_NAME (addin),
                             "label", title,
                             "visible", TRUE,
                             NULL);
      g_signal_connect_object (button,
                               "clicked",
                               G_CALLBACK (genesis_button_clicked),
                               self,
                               G_CONNECT_SWAPPED);
      gtk_container_add_with_properties (GTK_CONTAINER (self->genesis_buttons), GTK_WIDGET (button),
                                         "pack-type", GTK_PACK_START,
                                         "priority", priority,
                                         NULL);
    }

  child = ide_genesis_addin_get_widget (addin);
  gtk_container_add_with_properties (GTK_CONTAINER (self->genesis_stack), child,
                                     "name", G_OBJECT_TYPE_NAME (addin),
                                     NULL);
}

static void
ide_greeter_perspective_genesis_removed (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         PeasExtension    *exten,
                                         gpointer          user_data)
{
  IdeGreeterPerspective *self = user_data;
  IdeGenesisAddin *addin = (IdeGenesisAddin *)exten;
  const gchar *type_name;
  GList *list;
  GList *iter;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (addin));
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  type_name = G_OBJECT_TYPE_NAME (addin);
  list = gtk_container_get_children (GTK_CONTAINER (self->genesis_buttons));

  for (iter = list; iter != NULL; iter = iter->next)
    {
      GtkWidget *widget = iter->data;
      const gchar *name = gtk_widget_get_name (widget);

      if (g_strcmp0 (name, type_name) == 0)
        gtk_widget_destroy (widget);
    }

  g_list_free (list);
}

static void
ide_greeter_perspective_load_genesis_addins (IdeGreeterPerspective *self)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  self->genesis_set = peas_extension_set_new (peas_engine_get_default (),
                                              IDE_TYPE_GENESIS_ADDIN,
                                              NULL);

  g_signal_connect_object (self->genesis_set,
                           "extension-added",
                           G_CALLBACK (ide_greeter_perspective_genesis_added),
                           self,
                           0);

  g_signal_connect_object (self->genesis_set,
                           "extension-removed",
                           G_CALLBACK (ide_greeter_perspective_genesis_removed),
                           self,
                           0);

  peas_extension_set_foreach (self->genesis_set,
                              ide_greeter_perspective_genesis_added,
                              self);
}

static void
ide_greeter_perspective_run_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(IdeGreeterPerspective) self = user_data;
  IdeGenesisAddin *addin = (IdeGenesisAddin *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_GENESIS_ADDIN (addin));

  if (!ide_genesis_addin_run_finish (addin, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_strstrip (error->message);
          gtk_label_set_label (self->info_bar_label, error->message);
          gtk_revealer_set_reveal_child (self->info_bar_revealer, TRUE);
        }
    }

  /* Update continue button sensitivity */
  g_object_notify (G_OBJECT (addin), "is-ready");
}

static void
run_genesis_addin (PeasExtensionSet *set,
                   PeasPluginInfo   *plugin_info,
                   PeasExtension    *exten,
                   gpointer          user_data)
{
  IdeGenesisAddin *addin = (IdeGenesisAddin *)exten;
  struct {
    IdeGreeterPerspective *self;
    const gchar *name;
  } *state = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_GREETER_PERSPECTIVE (state->self));
  g_assert (state->name != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (addin));

  if (g_strcmp0 (state->name, G_OBJECT_TYPE_NAME (addin)) == 0)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (state->self->genesis_continue_button), FALSE);
      ide_genesis_addin_run_async (addin,
                                   state->self->cancellable,
                                   ide_greeter_perspective_run_cb,
                                   g_object_ref (state->self));
    }
}

static void
ide_greeter_perspective_genesis_continue (IdeGreeterPerspective *self)
{
  struct {
    IdeGreeterPerspective *self;
    const gchar *name;
  } state = { 0 };

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  state.self = self;
  state.name = gtk_stack_get_visible_child_name (self->genesis_stack);

  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  peas_extension_set_foreach (self->genesis_set, run_genesis_addin, &state);
}

static void
ide_greeter_perspective_genesis_continue_clicked (IdeGreeterPerspective *self,
                                                  GtkButton             *button)
{
  g_assert (GTK_IS_BUTTON (button));
  ide_greeter_perspective_genesis_continue (self);
}

static void
update_title_for_matching_addin (PeasExtensionSet *set,
                                 PeasPluginInfo   *plugin_info,
                                 PeasExtension    *exten,
                                 gpointer          user_data)
{
  struct {
    IdeGreeterPerspective *self;
    const gchar *name;
  } *state = user_data;
  IdeGenesisAddin *addin = (IdeGenesisAddin *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_GREETER_PERSPECTIVE (state->self));
  g_assert (state->name != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (addin));

  if (g_strcmp0 (state->name, G_OBJECT_TYPE_NAME (addin)) == 0)
    {
      g_autofree gchar *title = ide_genesis_addin_get_title (addin);
      g_autofree gchar *next = ide_genesis_addin_get_next_label (addin);
      GBinding *binding = state->self->ready_binding;

      if (binding != NULL)
        {
          ide_clear_weak_pointer (&state->self->ready_binding);
          g_binding_unbind (binding);
        }

      binding = g_object_bind_property (addin,
                                        "is-ready",
                                        state->self->genesis_continue_button,
                                        "sensitive",
                                        G_BINDING_SYNC_CREATE);
      ide_set_weak_pointer (&state->self->ready_binding, binding);

      gtk_label_set_label (state->self->genesis_title, title);
      gtk_button_set_label (state->self->genesis_continue_button, next);
    }
}

static void
ide_greeter_perspective_genesis_changed (IdeGreeterPerspective *self,
                                         GParamSpec            *pspec,
                                         GtkStack              *stack)
{
  struct {
    IdeGreeterPerspective *self;
    const gchar *name;
  } state = { 0 };

  g_assert (GTK_IS_STACK (stack));
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  gtk_widget_grab_default (GTK_WIDGET (self->genesis_continue_button));

  state.self = self;
  state.name = gtk_stack_get_visible_child_name (self->genesis_stack);

  peas_extension_set_foreach (self->genesis_set,
                              update_title_for_matching_addin,
                              &state);
}

static void
ide_greeter_perspective_info_bar_response (IdeGreeterPerspective *self,
                                           gint                   response_id,
                                           GtkInfoBar            *info_bar)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_INFO_BAR (info_bar));

  gtk_revealer_set_reveal_child (self->info_bar_revealer, FALSE);
}

static void
ide_greeter_perspective_constructed (GObject *object)
{
  IdeGreeterPerspective *self = (IdeGreeterPerspective *)object;
  IdeRecentProjects *recent_projects;

  G_OBJECT_CLASS (ide_greeter_perspective_parent_class)->constructed (object);

  recent_projects = ide_application_get_recent_projects (IDE_APPLICATION_DEFAULT);
  ide_greeter_perspective_set_recent_projects (self, recent_projects);

  ide_greeter_perspective_load_genesis_addins (self);
}

static void
ide_greeter_perspective_destroy (GtkWidget *widget)
{
  IdeGreeterPerspective *self = (IdeGreeterPerspective *)widget;

  if (self->titlebar != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->titlebar));

  GTK_WIDGET_CLASS (ide_greeter_perspective_parent_class)->destroy (widget);
}

static void
ide_greeter_perspective_finalize (GObject *object)
{
  IdeGreeterPerspective *self = (IdeGreeterPerspective *)object;

  ide_clear_weak_pointer (&self->ready_binding);
  g_clear_pointer (&self->pattern_spec, dzl_pattern_spec_unref);
  g_clear_object (&self->signal_group);
  g_clear_object (&self->recent_projects);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ide_greeter_perspective_parent_class)->finalize (object);
}

static void
ide_greeter_perspective_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeGreeterPerspective *self = IDE_GREETER_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_RECENT_PROJECTS:
      g_value_set_object (value, ide_greeter_perspective_get_recent_projects (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_greeter_perspective_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeGreeterPerspective *self = IDE_GREETER_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_RECENT_PROJECTS:
      ide_greeter_perspective_set_recent_projects (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_greeter_perspective_class_init (IdeGreeterPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_greeter_perspective_finalize;
  object_class->constructed = ide_greeter_perspective_constructed;
  object_class->get_property = ide_greeter_perspective_get_property;
  object_class->set_property = ide_greeter_perspective_set_property;

  widget_class->destroy = ide_greeter_perspective_destroy;

  properties [PROP_RECENT_PROJECTS] =
    g_param_spec_object ("recent-projects",
                         "Recent Projects",
                         "The recent projects that have been mined.",
                         IDE_TYPE_RECENT_PROJECTS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-greeter-perspective.ui");
  gtk_widget_class_set_css_name (widget_class, "greeter");
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_buttons);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_continue_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_title);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, info_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, info_bar_label);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, info_bar_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, my_projects_container);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, my_projects_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, open_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, other_projects_container);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, other_projects_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, remove_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, stack);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, state_machine);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, titlebar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, top_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, viewport);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, no_projects_found);
}

static const GActionEntry actions[] = {
  { "delete-selected-rows", delete_selected_rows },
};

static void
ide_greeter_perspective_init (IdeGreeterPerspective *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  g_autoptr(GAction) state = NULL;

  self->signal_group = dzl_signal_group_new (IDE_TYPE_RECENT_PROJECTS);
  dzl_signal_group_connect_object (self->signal_group,
                                   "items-changed",
                                   G_CALLBACK (recent_projects_items_changed),
                                   self,
                                   G_CONNECT_SWAPPED);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->titlebar,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->titlebar);

  g_signal_connect_object (self->search_entry,
                           "activate",
                           G_CALLBACK (ide_greeter_perspective__search_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (ide_greeter_perspective__search_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->my_projects_list_box,
                           "row-activated",
                           G_CALLBACK (ide_greeter_perspective__row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->my_projects_list_box,
                           "keynav-failed",
                           G_CALLBACK (ide_greeter_perspective__keynav_failed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->other_projects_list_box,
                           "row-activated",
                           G_CALLBACK (ide_greeter_perspective__row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->other_projects_list_box,
                           "keynav-failed",
                           G_CALLBACK (ide_greeter_perspective__keynav_failed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->top_stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_greeter_perspective_genesis_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->genesis_continue_button,
                           "clicked",
                           G_CALLBACK (ide_greeter_perspective_genesis_continue_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->genesis_cancel_button,
                           "clicked",
                           G_CALLBACK (ide_greeter_perspective_genesis_cancel_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->open_button,
                           "clicked",
                           G_CALLBACK (ide_greeter_perspective_open_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->cancel_button,
                           "clicked",
                           G_CALLBACK (ide_greeter_perspective_cancel_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->info_bar,
                           "response",
                           G_CALLBACK (ide_greeter_perspective_info_bar_response),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_sort_func (self->my_projects_list_box,
                              ide_greeter_perspective_sort_rows,
                              NULL, NULL);
  gtk_list_box_set_sort_func (self->other_projects_list_box,
                              ide_greeter_perspective_sort_rows,
                              NULL, NULL);

  gtk_list_box_set_filter_func (self->my_projects_list_box,
                                ide_greeter_perspective_filter_row,
                                self, NULL);
  gtk_list_box_set_filter_func (self->other_projects_list_box,
                                ide_greeter_perspective_filter_row,
                                self, NULL);

  group = g_simple_action_group_new ();
  state = dzl_state_machine_create_action (self->state_machine, "state");
  g_action_map_add_action (G_ACTION_MAP (group), state);
  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "greeter", G_ACTION_GROUP (group));

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "greeter", "delete-selected-rows",
                             "enabled", FALSE,
                             NULL);
}
