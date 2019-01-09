/* gbp-recent-section.c
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

#define G_LOG_DOMAIN "gbp-recent-section"

#include <glib/gi18n.h>
#include <libide-greeter.h>

#include "gbp-recent-project-row.h"
#include "gbp-recent-section.h"

struct _GbpRecentSection
{
  GtkBin      parent_instance;

  GtkListBox *listbox;

  guint       selection_mode : 1;
};

enum {
  PROP_0,
  PROP_HAS_SELECTION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
clear_settings_with_path (const gchar *schema_id,
                          const gchar *path)
{
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_auto(GStrv) keys = NULL;

  g_assert (schema_id != NULL);
  g_assert (path != NULL);

  settings = g_settings_new_with_path (schema_id, path);
  g_object_get (settings, "settings-schema", &schema, NULL);
  keys = g_settings_schema_list_keys (schema);

  for (guint i = 0; keys[i] != NULL; i++)
    g_settings_reset (settings, keys[i]);
}

static void
gbp_recent_section_get_has_selection_cb (GtkWidget *widget,
                                         gpointer   user_data)
{
  gboolean *has_selection = user_data;
  gboolean selected = FALSE;

  g_object_get (widget, "selected", &selected, NULL);
  *has_selection |= selected;
}

static gboolean
gbp_recent_section_get_has_selection (GbpRecentSection *self)
{
  gboolean has_selection = FALSE;

  g_assert (GBP_IS_RECENT_SECTION (self));

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         gbp_recent_section_get_has_selection_cb,
                         &has_selection);

  return has_selection;
}

static gint
gbp_recent_section_get_priority (IdeGreeterSection *section)
{
  return 0;
}

static void
gbp_recent_section_filter_cb (GtkListBoxRow *row,
                              gpointer       user_data)
{
  struct {
    DzlPatternSpec *spec;
    gboolean found;
  } *filter = user_data;
  gboolean match = TRUE;

  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));
  g_assert (filter != NULL);

  if (filter->spec != NULL)
    {
      const gchar *search_text;

      search_text = gbp_recent_project_row_get_search_text (GBP_RECENT_PROJECT_ROW (row));
      match = dzl_pattern_spec_match (filter->spec, search_text);
    }

  gtk_widget_set_visible (GTK_WIDGET (row), match);

  filter->found |= match;
}

static gboolean
gbp_recent_section_filter (IdeGreeterSection *section,
                           DzlPatternSpec    *spec)
{
  GbpRecentSection *self = (GbpRecentSection *)section;
  struct {
    DzlPatternSpec *spec;
    gboolean found;
  } filter = { spec, FALSE };

  g_assert (GBP_IS_RECENT_SECTION (self));

  /* We don't use filter func here so that we know if any
   * rows matched. We have to hide our widget if there are
   * no visible matches.
   */

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         (GtkCallback) gbp_recent_section_filter_cb,
                         &filter);

  return filter.found;
}

static void
gbp_recent_section_activate_first_cb (GtkWidget *widget,
                                      gpointer   user_data)
{
  IdeProjectInfo *project_info;
  struct {
    GbpRecentSection *self;
    gboolean handled;
  } *activate = user_data;

  g_assert (GBP_IS_RECENT_PROJECT_ROW (widget));
  g_assert (activate != NULL);

  if (activate->handled || !gtk_widget_get_visible (widget))
    return;

  project_info = gbp_recent_project_row_get_project_info (GBP_RECENT_PROJECT_ROW (widget));
  ide_greeter_section_emit_project_activated (IDE_GREETER_SECTION (activate->self), project_info);

  activate->handled = TRUE;
}

static gboolean
gbp_recent_section_activate_first (IdeGreeterSection *section)
{
  GbpRecentSection *self = (GbpRecentSection *)section;
  struct {
    GbpRecentSection *self;
    gboolean handled;
  } activate;

  g_return_val_if_fail (GBP_IS_RECENT_SECTION (self), FALSE);

  activate.self = self;
  activate.handled = FALSE;

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         gbp_recent_section_activate_first_cb,
                         &activate);

  return activate.handled;
}

static void
gbp_recent_section_set_selection_mode_cb (GtkWidget *widget,
                                          gpointer   user_data)
{
  GbpRecentProjectRow *row = (GbpRecentProjectRow *)widget;
  gboolean *selection_mode = user_data;

  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));
  g_assert (selection_mode != NULL);

  gbp_recent_project_row_set_selection_mode (row, *selection_mode);
  g_object_set (row, "selected", FALSE, NULL);
}

static void
gbp_recent_section_set_selection_mode (IdeGreeterSection *section,
                                       gboolean           selection_mode)
{
  GbpRecentSection *self = (GbpRecentSection *)section;

  g_assert (GBP_IS_RECENT_SECTION (self));

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         gbp_recent_section_set_selection_mode_cb,
                         &selection_mode);

  self->selection_mode = selection_mode;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_SELECTION]);
}

static void
gbp_recent_section_collect_selected_cb (GtkWidget *widget,
                                        gpointer   user_data)
{
  GbpRecentProjectRow *row = (GbpRecentProjectRow *)widget;
  GList **list = user_data;
  gboolean selected = FALSE;

  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));
  g_assert (list != NULL);

  g_object_get (row, "selected", &selected, NULL);

  if (selected)
    {
      IdeProjectInfo *project_info;

      project_info = gbp_recent_project_row_get_project_info (row);
      *list = g_list_prepend (*list, g_object_ref (project_info));
      gtk_widget_destroy (GTK_WIDGET (row));
    }
}

static gboolean
can_purge_project_directory (GFile *directory)
{
  g_autoptr(GFile) projects_dir = NULL;
  g_autoptr(GFile) home_dir = NULL;
  g_autoptr(GFile) downloads_dir = NULL;
  g_autofree gchar *uri = NULL;
  GFileType file_type;

  g_assert (G_IS_FILE (directory));

  uri = g_file_get_uri (directory);
  file_type = g_file_query_file_type (directory, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);

  if (file_type != G_FILE_TYPE_DIRECTORY)
    {
      g_critical ("Refusing to purge non-directory \"%s\"", uri);
      return FALSE;
    }

  projects_dir = g_file_new_for_path (ide_get_projects_dir ());
  home_dir = g_file_new_for_path (g_get_home_dir ());
  downloads_dir = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD));

  /* Refuse to delete anything outside of projects dir to be paranoid */
  if (!g_file_has_prefix (directory, projects_dir))
    {
      g_critical ("Refusing to purge \"%s\" as it is outside of projects directory", uri);
      return FALSE;
    }

  if (g_file_equal (directory, projects_dir) ||
      g_file_equal (directory, home_dir) ||
      g_file_equal (directory, downloads_dir))
    {
      g_critical ("Refusing to purge the project's directory");
      return FALSE;
    }

  return TRUE;
}

static void
gbp_recent_section_reap_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  DzlDirectoryReaper *reaper = (DzlDirectoryReaper *)object;
  g_autoptr(GPtrArray) directories = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (DZL_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (directories != NULL);

  if (!dzl_directory_reaper_execute_finish (reaper, result, &error))
    {
      g_warning ("Failed to purge directories: %s", error->message);
      return;
    }

  for (guint i = 0; i < directories->len; i++)
    {
      GFile *directory = g_ptr_array_index (directories, i);

      g_file_delete_async (directory, G_PRIORITY_LOW, NULL, NULL, NULL);
    }

  IDE_EXIT;
}

static void
gbp_recent_section_remove_file (DzlDirectoryReaper *reaper,
                                GFile              *file,
                                GtkTextBuffer      *buffer)
{
  GtkTextIter iter;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DZL_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_FILE (file));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_get_end_iter (buffer, &iter);

  if (g_file_is_native (file))
    {
      /* translators: %s is replaced with the path of the file to be deleted and \n for a new line */
      g_autofree gchar *formatted = g_strdup_printf (_("Removing %s\n"), g_file_peek_path (file));
      gtk_text_buffer_insert (buffer, &iter, formatted, -1);
    }
  else
    {
      /* translators: %s is replaced with the path of the file to be deleted and \n for a new line */
      g_autofree gchar *uri = g_file_get_uri (file);
      g_autofree gchar *formatted = g_strdup_printf (_("Removing %s\n"), uri);
      gtk_text_buffer_insert (buffer, &iter, formatted, -1);
    }
}

static void
gbp_recent_section_purge_selected_full (IdeGreeterSection *section,
                                        gboolean           purge_sources)
{
  GbpRecentSection *self = (GbpRecentSection *)section;
  g_autoptr(DzlDirectoryReaper) reaper = NULL;
  g_autoptr(GPtrArray) directories = NULL;
  IdeRecentProjects *projects;
  GtkWidget *workspace;
  GList *infos = NULL;

  g_assert (GBP_IS_RECENT_SECTION (self));

  workspace = gtk_widget_get_toplevel (GTK_WIDGET (section));

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         gbp_recent_section_collect_selected_cb,
                         &infos);

  /* Remove the projects from the list of recent projects */
  projects = ide_recent_projects_get_default ();
  ide_recent_projects_remove (projects, infos);

  /* Now asynchronously remove all the project files */
  reaper = dzl_directory_reaper_new ();
  directories = g_ptr_array_new_with_free_func (g_object_unref);

  for (const GList *iter = infos; iter != NULL; iter = iter->next)
    {
      g_autoptr(IdeProjectInfo) info = iter->data;
      const gchar *name = ide_project_info_get_name (info);
      GFile *directory = ide_project_info_get_directory (info);
      GFile *file = ide_project_info_get_file (info);
      g_autoptr(GFile) parent = NULL;
      g_autofree gchar *id = NULL;
      g_autofree gchar *path = NULL;

      g_assert (G_IS_FILE (directory) || G_IS_FILE (file));

      /* If the IdeProjectInfo:file is a directory, refuse to delete the
       * pre-stated directory as it might be a parent which is really Home, or
       * something like that. This just helps ensure we're a bit safer when
       * dealing with user data.
       */
      if (file != NULL &&
          g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
        {
          if (directory == NULL || g_file_has_prefix (file, directory))
            directory = file;
        }

      if (directory == NULL)
        {
          if (g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
            directory = g_object_ref (file);
          else
            directory = parent = g_file_get_parent (file);
        }

      g_assert (G_IS_FILE (directory));

      if (purge_sources)
        {
          if (can_purge_project_directory (directory))
            {
              dzl_directory_reaper_add_directory (reaper, directory, 0);
              g_ptr_array_add (directories, g_object_ref (directory));
            }
        }

      /*
       * Also add various cache directories we know are used by
       * Builder so that we can cleanup extra state that the user
       * might expect to be reomved.
       */

      id = ide_create_project_id (name);

      if (name != NULL)
        {
          g_autoptr(GFile) cache = NULL;

          cache = g_file_new_build_filename (g_get_user_cache_dir (),
                                             ide_get_program_name (),
                                             "projects",
                                             id,
                                             NULL);
          dzl_directory_reaper_add_directory (reaper, cache, 0);
          g_ptr_array_add (directories, g_steal_pointer (&cache));
        }

      /*
       * Unset any project settings so if the project is openned again there
       * is a better chance it get's fresh state.
       */
      path = g_strdup_printf ("/org/gnome/builder/projects/%s/", id);
      clear_settings_with_path ("org.gnome.builder.project", path);
    }

  if (purge_sources)
    {
      GtkDialog *dialog;
      GtkWidget *scroller;
      GtkWidget *view;
      GtkWidget *content_area;
      GtkTextBuffer *buffer;

      dialog = GTK_DIALOG (gtk_dialog_new ());
      gtk_window_set_title (GTK_WINDOW (dialog), _("Removing Files…"));
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (workspace));
      gtk_dialog_add_button (dialog, _("Close"), GTK_RESPONSE_CLOSE);
      gtk_window_set_default_size (GTK_WINDOW (dialog), 700, 500);
      content_area = gtk_dialog_get_content_area (dialog);
      gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);
      gtk_box_set_spacing (GTK_BOX (content_area), 12);

      scroller = gtk_scrolled_window_new (NULL, NULL);
      gtk_widget_set_vexpand (scroller, TRUE);
      gtk_container_add (GTK_CONTAINER (content_area), scroller);
      gtk_widget_show (scroller);

      view = gtk_text_view_new ();
      gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_container_add (GTK_CONTAINER (scroller), view);
      gtk_widget_show (view);

      g_signal_connect_object (reaper,
                               "remove-file",
                               G_CALLBACK (gbp_recent_section_remove_file),
                               buffer,
                               0);

      g_signal_connect (dialog,
                        "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      gtk_window_present (GTK_WINDOW (dialog));
    }

  dzl_directory_reaper_execute_async (reaper,
                                      NULL,
                                      gbp_recent_section_reap_cb,
                                      g_steal_pointer (&directories));

  g_list_free (infos);
}

static void
gbp_recent_section_purge_selected (IdeGreeterSection *section)
{
  gbp_recent_section_purge_selected_full (section, TRUE);
}

static void
gbp_recent_section_delete_selected (IdeGreeterSection *section)
{
  gbp_recent_section_purge_selected_full (section, FALSE);
}

static void
greeter_section_iface_init (IdeGreeterSectionInterface *iface)
{
  iface->get_priority = gbp_recent_section_get_priority;
  iface->filter = gbp_recent_section_filter;
  iface->activate_first = gbp_recent_section_activate_first;
  iface->set_selection_mode = gbp_recent_section_set_selection_mode;
  iface->delete_selected = gbp_recent_section_delete_selected;
  iface->purge_selected = gbp_recent_section_purge_selected;
}

G_DEFINE_TYPE_WITH_CODE (GbpRecentSection, gbp_recent_section, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_GREETER_SECTION,
                                                greeter_section_iface_init))

static void
gbp_recent_section_notify_row_selected (GbpRecentSection    *self,
                                        GParamSpec          *pspec,
                                        GbpRecentProjectRow *row)
{
  g_assert (GBP_IS_RECENT_SECTION (self));
  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_SELECTION]);
}

static void
gbp_recent_section_row_activated (GbpRecentSection    *self,
                                  GbpRecentProjectRow *row,
                                  GtkListBox          *list_box)
{
  g_assert (GBP_IS_RECENT_SECTION (self));
  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (self->selection_mode)
    {
      gboolean selected = FALSE;

      g_object_get (row, "selected", &selected, NULL);
      g_object_set (row, "selected", !selected, NULL);
    }
  else
    {
      IdeProjectInfo *project_info;

      project_info = gbp_recent_project_row_get_project_info (row);
      ide_greeter_section_emit_project_activated (IDE_GREETER_SECTION (self), project_info);
    }
}

static GtkWidget *
create_widget_func (gpointer item,
                    gpointer user_data)
{
  IdeProjectInfo *project_info = item;
  GbpRecentProjectRow *row;
  GbpRecentSection *self = user_data;

  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (GBP_IS_RECENT_SECTION (self));

  row = g_object_new (GBP_TYPE_RECENT_PROJECT_ROW,
                      "project-info", project_info,
                      "visible", TRUE,
                      NULL);

  g_signal_connect_object (row,
                           "notify::selected",
                           G_CALLBACK (gbp_recent_section_notify_row_selected),
                           self,
                           G_CONNECT_SWAPPED);

  return GTK_WIDGET (row);
}

static void
gbp_recent_section_constructed (GObject *object)
{
  GbpRecentSection *self = (GbpRecentSection *)object;
  IdeRecentProjects *projects;

  G_OBJECT_CLASS (gbp_recent_section_parent_class)->constructed (object);

  projects = ide_recent_projects_get_default ();

  gtk_list_box_bind_model (self->listbox,
                           G_LIST_MODEL (projects),
                           create_widget_func, self, NULL);
}

static void
gbp_recent_section_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpRecentSection *self = GBP_RECENT_SECTION (object);

  switch (prop_id)
    {
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, gbp_recent_section_get_has_selection (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_recent_section_class_init (GbpRecentSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_recent_section_constructed;
  object_class->get_property = gbp_recent_section_get_property;

  properties [PROP_HAS_SELECTION] =
    g_param_spec_boolean ("has-selection", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "recent");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/recent/gbp-recent-section.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpRecentSection, listbox);
  gtk_widget_class_bind_template_callback (widget_class, gbp_recent_section_row_activated);

  g_type_ensure (GBP_TYPE_RECENT_PROJECT_ROW);
}

static gboolean
on_button_press_event_cb (GtkListBox       *listbox,
                          GdkEventButton   *ev,
                          GbpRecentSection *self)
{
  GtkListBoxRow *row;

  g_assert (GTK_IS_LIST_BOX (listbox));
  g_assert (GBP_IS_RECENT_SECTION (self));

  if (ev->button == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *workspace;

      workspace = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GREETER_WORKSPACE);
      ide_greeter_workspace_set_selection_mode (IDE_GREETER_WORKSPACE (workspace), TRUE);

      if ((row = gtk_list_box_get_row_at_y (listbox, ev->y)))
        {
          g_object_set (row, "selected", TRUE, NULL);
          return GDK_EVENT_STOP;
        }
    }

  return GDK_EVENT_PROPAGATE;
}


static void
gbp_recent_section_init (GbpRecentSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->listbox,
                    "button-press-event",
                    G_CALLBACK (on_button_press_event_cb),
                    self);
}
