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

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gtk.h>
#include <libide-greeter.h>
#include <libide-projects.h>

#include "ide-project-info-private.h"

#include "gbp-recent-section.h"

struct _GbpRecentSection
{
  GtkWidget            parent_instance;

  IdeTruncateModel    *truncate;

  AdwPreferencesGroup *group;
  GtkListBox          *listbox;

  guint                selection_mode : 1;
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

static gboolean
gbp_recent_section_get_has_selection (GbpRecentSection *self)
{
  g_assert (GBP_IS_RECENT_SECTION (self));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_ROW (child))
        {
          gboolean selected;

          g_object_get (child, "selected", &selected, NULL);

          if (selected)
            return TRUE;
        }
    }

  return FALSE;
}

static gint
gbp_recent_section_get_priority (IdeGreeterSection *section)
{
  return -100;
}

static gboolean
gbp_recent_section_filter (IdeGreeterSection *section,
                           IdePatternSpec    *spec)
{
  GbpRecentSection *self = (GbpRecentSection *)section;
  gboolean found = FALSE;

  g_assert (GBP_IS_RECENT_SECTION (self));

  /* Expand the truncation model if necessary */
  if (spec != NULL)
    ide_truncate_model_set_expanded (self->truncate, TRUE);

  /* We don't use filter func here so that we know if any
   * rows matched. We have to hide our widget if there are
   * no visible matches.
   */
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_ROW (child))
        {
          gboolean match = spec == NULL;

          if (spec != NULL)
            {
              g_autofree gchar *search_text = NULL;

              if ((search_text = ide_greeter_row_get_search_text (IDE_GREETER_ROW (child))))
                match = ide_pattern_spec_match (spec, search_text);
            }

          gtk_widget_set_visible (child, match);

          found |= match;
        }
    }

  return found;
}

static gboolean
gbp_recent_section_activate_first (IdeGreeterSection *section)
{
  GbpRecentSection *self = (GbpRecentSection *)section;

  g_return_val_if_fail (GBP_IS_RECENT_SECTION (self), FALSE);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_ROW (child) &&
          gtk_widget_get_visible (child))
        {
          IdeProjectInfo *project_info;

          project_info = ide_greeter_row_get_project_info (IDE_GREETER_ROW (child));
          ide_greeter_section_emit_project_activated (IDE_GREETER_SECTION (self), project_info);

          return TRUE;
        }
    }

  return FALSE;
}

static void
gbp_recent_section_set_selection_mode (IdeGreeterSection *section,
                                       gboolean           selection_mode)
{
  GbpRecentSection *self = (GbpRecentSection *)section;

  g_assert (GBP_IS_RECENT_SECTION (self));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_ROW (child))
        {
          ide_greeter_row_set_selection_mode (IDE_GREETER_ROW (child), selection_mode);
          g_object_set (child, "selected", FALSE, NULL);
        }
    }

  self->selection_mode = selection_mode;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_SELECTION]);
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
  IdeDirectoryReaper *reaper = (IdeDirectoryReaper *)object;
  g_autoptr(GPtrArray) directories = user_data;
  g_autoptr(GError) error = NULL;
  GtkDialog *dialog;

  IDE_ENTRY;

  g_assert (IDE_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (directories != NULL);

  if ((dialog = g_object_get_data (G_OBJECT (reaper), "DIALOG")))
    gtk_window_set_title (GTK_WINDOW (dialog), _("Removed Files"));

  if (!ide_directory_reaper_execute_finish (reaper, result, &error))
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
gbp_recent_section_remove_file (IdeDirectoryReaper *reaper,
                                GFile              *file,
                                GtkTextBuffer      *buffer)
{
  GtkTextIter iter;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_DIRECTORY_REAPER (reaper));
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
  g_autoptr(IdeDirectoryReaper) reaper = NULL;
  g_autoptr(GPtrArray) directories = NULL;
  IdeRecentProjects *projects;
  GtkWidget *workspace;
  GList *infos = NULL;

  g_assert (GBP_IS_RECENT_SECTION (self));

  workspace = GTK_WIDGET (gtk_widget_get_native (GTK_WIDGET (section)));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_ROW (child))
        {
          gboolean selected;

          g_object_get (child, "selected", &selected, NULL);

          if (selected)
            {
              IdeProjectInfo *info;

              if ((info = ide_greeter_row_get_project_info (IDE_GREETER_ROW (child))))
                infos = g_list_prepend (infos, g_object_ref (info));
            }
        }
    }

  /* Remove the projects from the list of recent projects */
  projects = ide_recent_projects_get_default ();

  /* Now asynchronously remove all the project files */
  reaper = ide_directory_reaper_new ();
  directories = g_ptr_array_new_with_free_func (g_object_unref);

  for (const GList *iter = infos; iter != NULL; iter = iter->next)
    {
      IdeProjectInfo *info = iter->data;
      const gchar *name = ide_project_info_get_name (info);
      GFile *directory = _ide_project_info_get_real_directory (info);
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
              ide_directory_reaper_add_directory (reaper, directory, 0);
              g_ptr_array_add (directories, g_object_ref (directory));
            }
        }

      /*
       * Also add various cache directories we know are used by
       * Builder so that we can cleanup extra state that the user
       * might expect to be reomved.
       */

      id = g_strdelimit (ide_create_project_id (name), "@:/", '-');

      if (name != NULL)
        {
          g_autofree char *default_cache_root = ide_dup_default_cache_dir ();
          g_autoptr(GFile) cache = NULL;

          cache = g_file_new_build_filename (default_cache_root,
                                             "projects",
                                             id,
                                             NULL);
          ide_directory_reaper_add_directory (reaper, cache, 0);
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

      dialog = g_object_new (GTK_TYPE_DIALOG,
                             "title", _("Removing Files…"),
                             "transient-for", workspace,
                             "default-width", 700,
                             "default-height", 500,
                             "use-header-bar", TRUE,
                             NULL);
#ifdef DEVELOPMENT_BUILD
      gtk_widget_add_css_class (GTK_WIDGET (dialog), "devel");
#endif
      gtk_dialog_add_button (dialog, _("_Close"), GTK_RESPONSE_CLOSE);
      content_area = gtk_dialog_get_content_area (dialog);
      g_object_set (content_area,
                    "margin-top", 0,
                    "margin-bottom", 0,
                    "margin-start", 0,
                    "margin-end", 0,
                    NULL);
      gtk_box_set_spacing (GTK_BOX (content_area), 0);

      scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW, NULL);
      gtk_widget_set_vexpand (scroller, TRUE);
      gtk_box_append (GTK_BOX (content_area), scroller);
      gtk_widget_show (scroller);

      view = g_object_new (GTK_TYPE_TEXT_VIEW,
                           "editable", FALSE,
                           "cursor-visible", FALSE,
                           "left-margin", 12,
                           "right-margin", 12,
                           "top-margin", 12,
                           "bottom-margin", 12,
                           NULL);
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller), view);
      gtk_widget_show (view);

      g_signal_connect_object (reaper,
                               "remove-file",
                               G_CALLBACK (gbp_recent_section_remove_file),
                               buffer,
                               0);

      g_signal_connect (dialog,
                        "response",
                        G_CALLBACK (gtk_window_destroy),
                        NULL);

      g_object_set_data_full (G_OBJECT (reaper),
                              "DIALOG",
                              g_object_ref (dialog),
                              g_object_unref);

      ide_gtk_window_present (GTK_WINDOW (dialog));
    }

  ide_directory_reaper_execute_async (reaper,
                                      NULL,
                                      gbp_recent_section_reap_cb,
                                      g_steal_pointer (&directories));

  ide_recent_projects_remove (projects, infos);
  g_list_free_full (infos, g_object_unref);
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

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpRecentSection, gbp_recent_section, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_GREETER_SECTION, greeter_section_iface_init))

static void
gbp_recent_section_notify_row_selected (GbpRecentSection *self,
                                        GParamSpec       *pspec,
                                        IdeGreeterRow    *row)
{
  g_assert (GBP_IS_RECENT_SECTION (self));
  g_assert (IDE_IS_GREETER_ROW (row));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_SELECTION]);
}

static void
gbp_recent_section_row_activated (GbpRecentSection *self,
                                  IdeGreeterRow    *row,
                                  GtkListBox       *listbox)
{
  g_assert (GBP_IS_RECENT_SECTION (self));
  g_assert (GTK_IS_LIST_BOX (listbox));

  if (!IDE_IS_GREETER_ROW (row))
    {
      ide_truncate_model_set_expanded (self->truncate, TRUE);
      return;
    }

  if (self->selection_mode)
    {
      gboolean selected = FALSE;

      g_object_get (row, "selected", &selected, NULL);
      g_object_set (row, "selected", !selected, NULL);
    }
  else
    {
      IdeProjectInfo *project_info;

      project_info = ide_greeter_row_get_project_info (row);
      ide_greeter_section_emit_project_activated (IDE_GREETER_SECTION (self), project_info);
    }
}

static GtkWidget *
create_widget_func (gpointer item,
                    gpointer user_data)
{
  IdeProjectInfo *project_info = item;
  GbpRecentSection *self = user_data;
  IdeGreeterRow *row;

  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (GBP_IS_RECENT_SECTION (self));

  row = g_object_new (IDE_TYPE_GREETER_ROW,
                      "project-info", project_info,
                      "visible", TRUE,
                      NULL);

  g_signal_connect_object (row,
                           "notify::selected",
                           G_CALLBACK (gbp_recent_section_notify_row_selected),
                           self,
                           G_CONNECT_SWAPPED);

  if (self->selection_mode)
    ide_greeter_row_set_selection_mode (row, TRUE);

  return GTK_WIDGET (row);
}

static void
gbp_recent_section_dispose (GObject *object)
{
  GbpRecentSection *self = (GbpRecentSection *)object;

  g_clear_object (&self->truncate);
  g_clear_pointer ((GtkWidget **)&self->group, gtk_widget_unparent);

  G_OBJECT_CLASS (gbp_recent_section_parent_class)->dispose (object);
}

static void
gbp_recent_section_constructed (GObject *object)
{
  GbpRecentSection *self = (GbpRecentSection *)object;
  IdeRecentProjects *projects;
  GtkWidget *row;

  G_OBJECT_CLASS (gbp_recent_section_parent_class)->constructed (object);

  projects = ide_recent_projects_get_default ();

  self->truncate = ide_truncate_model_new (G_LIST_MODEL (projects));

  gtk_list_box_bind_model (self->listbox,
                           G_LIST_MODEL (self->truncate),
                           create_widget_func, self, NULL);

  row = g_object_new (ADW_TYPE_BUTTON_ROW,
                      "title", _("Show More"),
                      "start-icon-name", "view-more-symbolic",
                      NULL);
  gtk_list_box_append (self->listbox, GTK_WIDGET (row));

  g_object_bind_property (self->truncate, "can-expand", row, "visible",
                          G_BINDING_SYNC_CREATE);
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
  object_class->dispose = gbp_recent_section_dispose;
  object_class->get_property = gbp_recent_section_get_property;

  properties [PROP_HAS_SELECTION] =
    g_param_spec_boolean ("has-selection", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "recent");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/recent/gbp-recent-section.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpRecentSection, group);
  gtk_widget_class_bind_template_child (widget_class, GbpRecentSection, listbox);
  gtk_widget_class_bind_template_callback (widget_class, gbp_recent_section_row_activated);
}

static void
gbp_recent_section_click_pressed_cb (GbpRecentSection *self,
                                     int               n_presses,
                                     double            x,
                                     double            y,
                                     GtkGestureClick  *gesture)
{
  IdeWorkspace *workspace;
  GtkWidget *pick;

  g_assert (GBP_IS_RECENT_SECTION (self));
  g_assert (GTK_IS_GESTURE_CLICK (gesture));

  pick = gtk_widget_pick (GTK_WIDGET (self), x, y, GTK_PICK_NON_TARGETABLE);
  if (!GTK_IS_LIST_BOX_ROW (pick))
    pick = gtk_widget_get_ancestor (pick, GTK_TYPE_LIST_BOX_ROW);

  if (pick == NULL)
    return;

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  ide_greeter_workspace_set_selection_mode (IDE_GREETER_WORKSPACE (workspace), TRUE);
  g_object_set (pick, "selected", TRUE, NULL);
}

static void
gbp_recent_section_init (GbpRecentSection *self)
{
  GtkGesture *gesture;

  gtk_widget_init_template (GTK_WIDGET (self));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 3);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
  g_signal_connect_object (gesture,
                           "pressed",
                           G_CALLBACK (gbp_recent_section_click_pressed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}
