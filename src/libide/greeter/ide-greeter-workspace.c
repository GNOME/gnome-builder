/* ide-greeter-workspace.c
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

#define G_LOG_DOMAIN "ide-greeter-workspace"

#include "config.h"

#include <glib/gi18n.h>

#include <libpeas.h>

#include "ide-marshal.h"

#include "ide-greeter-buttons-section.h"
#include "ide-greeter-resources.h"
#include "ide-greeter-row.h"
#include "ide-greeter-workspace.h"

/**
 * SECTION:ide-greeter-workspace
 * @title: IdeGreeterWorkspace
 * @short_description: The greeter upon starting Builder
 *
 * Use the #IdeWorkspace APIs to add pages for user guides such
 * as the git workflow or project creation wizard.
 *
 * You can add buttons to the headerbar and use actions to change
 * pages such as "win.page::'page-name'".
 */

struct _IdeGreeterWorkspace
{
  IdeWorkspace              parent_instance;

  PeasExtensionSet         *addins;
  IdePatternSpec           *pattern_spec;
  GSimpleAction            *delete_action;
  GSimpleAction            *purge_action;

  /* Template Widgets */
  IdeHeaderBar             *header_bar;
  GtkBox                   *sections;
  AdwNavigationView        *navigation_view;
  GtkSearchEntry           *search_entry;
  GtkActionBar             *action_bar;
  GtkActionBar             *projects_action_bar;
  IdeGreeterButtonsSection *buttons_section;
  AdwStatusPage            *empty_state;

  guint                     selection_mode : 1;
  guint                     busy : 1;
};

G_DEFINE_FINAL_TYPE (IdeGreeterWorkspace, ide_greeter_workspace, IDE_TYPE_WORKSPACE)

enum {
  PROP_0,
  PROP_SELECTION_MODE,
  N_PROPS
};

enum {
  OPEN_PROJECT,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

#define GET_PRIORITY(w)   GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"PRIORITY"))
#define SET_PRIORITY(w,i) g_object_set_data(G_OBJECT(w),"PRIORITY",GINT_TO_POINTER(i))

static void
add_with_priority (GtkWidget *parent,
                   GtkWidget *widget,
                   int        priority)
{
  GtkWidget *sibling = NULL;

  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  SET_PRIORITY (widget, priority);

  for (GtkWidget *child = gtk_widget_get_first_child (parent);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (priority < GET_PRIORITY (child))
        break;
      sibling = child;
    }

  gtk_widget_insert_after (widget, parent, sibling);
}

static gboolean
ide_greeter_workspace_has_match (IdeGreeterWorkspace *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->sections));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_SECTION (child) && gtk_widget_get_visible (child))
        return TRUE;
    }

  return FALSE;
}

static void
ide_greeter_workspace_filter_sections (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       GObject    *exten,
                                       gpointer          user_data)
{
  IdeGreeterWorkspace *self = user_data;
  IdeGreeterSection *section = (IdeGreeterSection *)exten;
  gboolean has_child;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GREETER_SECTION (section));
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  has_child = ide_greeter_section_filter (section, self->pattern_spec);

  gtk_widget_set_visible (GTK_WIDGET (section), has_child);
}

static void
ide_greeter_workspace_apply_filter_all (IdeGreeterWorkspace *self)
{
  const gchar *text;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  g_clear_pointer (&self->pattern_spec, ide_pattern_spec_unref);

  if (NULL != (text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry))))
    self->pattern_spec = ide_pattern_spec_new (text);

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins,
                                ide_greeter_workspace_filter_sections,
                                self);

  gtk_widget_set_visible (GTK_WIDGET (self->sections),
                          ide_greeter_workspace_has_match (self));
  gtk_widget_set_visible (GTK_WIDGET (self->empty_state),
                          !ide_greeter_workspace_has_match (self));
}

static void
ide_greeter_workspace_search_entry_activate (IdeGreeterWorkspace *self,
                                             GtkSearchEntry      *search_entry)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->sections));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_SECTION (child))
        {
          if (ide_greeter_section_activate_first (IDE_GREETER_SECTION (child)))
            return;
        }
    }

  gtk_widget_error_bell (GTK_WIDGET (search_entry));
}

static void
ide_greeter_workspace_search_entry_changed (IdeGreeterWorkspace *self,
                                            GtkSearchEntry      *search_entry)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  ide_greeter_workspace_apply_filter_all (self);
}

static void
navigation_view_notify_visible_page_cb (IdeGreeterWorkspace *self)
{
  g_autofree gchar *title = NULL;
  g_autofree gchar *full_title = NULL;
  AdwNavigationPage *page;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  page = adw_navigation_view_get_visible_page (self->navigation_view);

  if (page != NULL)
    {
      if ((title = g_strdup (adw_navigation_page_get_title (page))))
        full_title = g_strdup_printf (_("Builder — %s"), title);
    }

  gtk_window_set_title (GTK_WINDOW (self), full_title);
}

static void
ide_greeter_workspace_addin_added_cb (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject    *exten,
                                      gpointer          user_data)
{
  IdeGreeterSection *section = (IdeGreeterSection *)exten;
  IdeGreeterWorkspace *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GREETER_SECTION (section));
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  /* Don't allow floating, to work with extension set*/
  if (g_object_is_floating (G_OBJECT (section)))
    g_object_ref_sink (section);

  gtk_widget_show (GTK_WIDGET (section));

  ide_greeter_workspace_add_section (self, section);
}

static void
ide_greeter_workspace_addin_removed_cb (PeasExtensionSet *set,
                                        PeasPluginInfo   *plugin_info,
                                        GObject    *exten,
                                        gpointer          user_data)
{
  IdeGreeterSection *section = (IdeGreeterSection *)exten;
  GtkBox *box;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GREETER_SECTION (section));
  g_assert (IDE_IS_GREETER_WORKSPACE (user_data));

  box = GTK_BOX (gtk_widget_get_parent (GTK_WIDGET (section)));
  gtk_box_remove (box, GTK_WIDGET (section));
}

static void
ide_greeter_workspace_constructed (GObject *object)
{
  IdeGreeterWorkspace *self = (IdeGreeterWorkspace *)object;

  G_OBJECT_CLASS (ide_greeter_workspace_parent_class)->constructed (object);

  gtk_widget_add_css_class (GTK_WIDGET (self), "greeter");

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_GREETER_SECTION,
                                         NULL);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_greeter_workspace_addin_added_cb),
                    self);

  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_greeter_workspace_addin_removed_cb),
                    self);

  peas_extension_set_foreach (self->addins,
                              ide_greeter_workspace_addin_added_cb,
                              self);

  /* Ensure that no plugin changed our page */
  adw_navigation_view_pop_to_tag (self->navigation_view, "overview");

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static void
tear_workbench_down (IdeWorkbench *workbench)
{
  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (workbench));

  ide_workbench_unload_async (workbench, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_greeter_workspace_open_project_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *)object;
  g_autoptr(IdeGreeterWorkspace) self = (IdeGreeterWorkspace *)user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  if (!ide_workbench_load_project_finish (workbench, result, &error))
    {
      AdwDialog *dialog;

      dialog = adw_alert_dialog_new (_("Failed to load the project"),
                                     error->message);
      adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("_Close"));

      g_signal_connect_object (dialog,
                               "response",
                               G_CALLBACK (tear_workbench_down),
                               workbench,
                               G_CONNECT_SWAPPED);

      adw_dialog_present (dialog, GTK_WIDGET (workbench));

      ide_greeter_workspace_end (self);
    }

  ide_workbench_remove_workspace (workbench, IDE_WORKSPACE (self));
  gtk_window_destroy (GTK_WINDOW (self));

  IDE_EXIT;
}

static gboolean
ide_greeter_workspace_real_open_project (IdeGreeterWorkspace *self,
                                         IdeProjectInfo      *project_info)
{
  const gchar *vcs_uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  /* If there is a VCS Uri and no project file/directory, then we might
   * be able to guess the directory based on the clone vcs uri directory
   * name. Use that to see if we can skip cloning again.
   */
  if (!ide_project_info_get_file (project_info) &&
      !ide_project_info_get_directory (project_info) &&
      (vcs_uri = ide_project_info_get_vcs_uri (project_info)))
    {
      g_autoptr(IdeVcsUri) uri = ide_vcs_uri_new (vcs_uri);
      g_autofree gchar *suggested = NULL;
      g_autofree gchar *checkout = NULL;

      if (uri != NULL &&
          (suggested = ide_vcs_uri_get_clone_name (uri)) &&
          (checkout = g_build_filename (ide_get_projects_dir (), suggested, NULL)) &&
          g_file_test (checkout, G_FILE_TEST_IS_DIR))
        {
          g_autoptr(GFile) directory = g_file_new_for_path (checkout);
          ide_project_info_set_directory (project_info, directory);
          IDE_RETURN (FALSE);
        }
    }

  IDE_RETURN (FALSE);
}

/**
 * ide_greeter_workspace_open_project:
 * @self: an #IdeGreeterWorkspace
 * @project_info: an #IdeProjectInfo
 *
 * Opens the project described by @project_info.
 *
 * This is useful by greeter workspace extensions that add new pages
 * which may not have other means to activate a project.
 */
void
ide_greeter_workspace_open_project (IdeGreeterWorkspace *self,
                                    IdeProjectInfo      *project_info)
{
  IdeWorkbench *workbench;
  gboolean ret = FALSE;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));

  g_signal_emit (self, signals [OPEN_PROJECT], 0, project_info, &ret);

  if (ret)
    IDE_GOTO (not_ready);

  /* If this project is already open, then just switch to that project instead
   * of trying to load the project a second time.
   */
  if ((workbench = ide_application_find_project_workbench (IDE_APPLICATION_DEFAULT, project_info)))
    {
      ide_workbench_activate (workbench);
      gtk_window_destroy (GTK_WINDOW (self));
      IDE_EXIT;
    }

  workbench = ide_workspace_get_workbench (IDE_WORKSPACE (self));

  ide_greeter_workspace_begin (self);

  if (ide_project_info_get_directory (project_info) == NULL)
    {
      GFile *file;

      if ((file = ide_project_info_get_file (project_info)))
        {
          g_autoptr(GFile) parent = g_file_get_parent (file);

          /* If it's a directory, set that too, otherwise use the parent */
          if (g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
            ide_project_info_set_directory (project_info, file);
          else
            ide_project_info_set_directory (project_info, parent);
        }
    }

  ide_workbench_load_project_async (workbench,
                                    project_info,
                                    IDE_TYPE_PRIMARY_WORKSPACE,
                                    ide_workspace_get_cancellable (IDE_WORKSPACE (self)),
                                    ide_greeter_workspace_open_project_cb,
                                    g_object_ref (self));

not_ready:
  IDE_EXIT;
}

static void
ide_greeter_workspace_project_activated_cb (IdeGreeterWorkspace *self,
                                            IdeProjectInfo      *project_info,
                                            IdeGreeterSection   *section)
{
  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (IDE_IS_GREETER_SECTION (section));

  ide_greeter_workspace_open_project (self, project_info);
}

static void
ide_greeter_workspace_delete_selected_rows (GSimpleAction *action,
                                            GVariant      *param,
                                            gpointer       user_data)
{
  IdeGreeterWorkspace *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param == NULL);
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->sections));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_SECTION (child))
        ide_greeter_section_delete_selected (IDE_GREETER_SECTION (child));
    }

  ide_greeter_workspace_set_selection_mode (self, FALSE);
}

static void
purge_selected_rows_response (IdeGreeterWorkspace *self)
{
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->sections));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (IDE_IS_GREETER_SECTION (child))
        ide_greeter_section_purge_selected (IDE_GREETER_SECTION (child));
    }

  ide_greeter_workspace_set_selection_mode (self, FALSE);
}

static void
ide_greeter_workspace_purge_selected_rows (GSimpleAction *action,
                                           GVariant      *param,
                                           gpointer       user_data)
{
  IdeGreeterWorkspace *self = user_data;
  AdwDialog *dialog;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param == NULL);
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  dialog = adw_alert_dialog_new (_("Delete Project Sources?"),
                                 _("Deleting the project source code from your computer cannot be undone."));
  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "delete", _("_Delete Project Sources"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                            "delete",
                                            ADW_RESPONSE_DESTRUCTIVE);
  g_signal_connect_object (dialog,
                           "response::delete",
                           G_CALLBACK (purge_selected_rows_response),
                           self,
                           G_CONNECT_SWAPPED);
  adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
ide_greeter_workspace_page_action (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  IdeGreeterWorkspace *self = (IdeGreeterWorkspace *)widget;

  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  ide_greeter_workspace_push_page_by_tag (self, g_variant_get_string (param, NULL));

  IDE_EXIT;
}

static void
ide_greeter_workspace_dialog_response (IdeGreeterWorkspace  *self,
                                       gint                  response_id,
                                       GtkFileChooserDialog *dialog)
{
  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_OK)
    {
      g_autoptr(IdeProjectInfo) project_info = NULL;
      g_autoptr(GFile) project_file = NULL;
      GtkFileFilter *filter;

      project_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      project_info = ide_project_info_new ();
      ide_project_info_set_file (project_info, project_file);

      if ((filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog))))
        {
          const gchar *module_name = g_object_get_data (G_OBJECT (filter), "MODULE_NAME");

          if (module_name != NULL)
            ide_project_info_set_build_system_hint (project_info, module_name);

          /* If this is a directory selection, then make sure we set the
           * directory on the project-info too. That way we don't rely on
           * it being set elsewhere (which could be a translated symlink path).
           */
          if (g_object_get_data (G_OBJECT (filter), "IS_DIRECTORY"))
            ide_project_info_set_directory (project_info, project_file);
        }

      ide_greeter_workspace_open_project (self, project_info);
    }

  gtk_window_destroy (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static void
ide_greeter_workspace_dialog_notify_filter (IdeGreeterWorkspace  *self,
                                            GParamSpec           *pspec,
                                            GtkFileChooserDialog *dialog)
{
  GtkFileFilter *filter;
  GtkFileChooserAction action;
  const gchar *title;

  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (pspec != NULL);
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog));

  if (filter && g_object_get_data (G_OBJECT (filter), "IS_DIRECTORY"))
    {
      action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      title = _("Select Project Folder");
    }
  else
    {
      action = GTK_FILE_CHOOSER_ACTION_OPEN;
      title = _("Select Project File");
    }

  gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), action);
  gtk_window_set_title (GTK_WINDOW (dialog), title);

  IDE_EXIT;
}

static void
ide_greeter_workspace_open_action (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  IdeGreeterWorkspace *self = (IdeGreeterWorkspace *)widget;
  g_autoptr(GFile) projects_dir = NULL;
  GtkFileChooserDialog *dialog;
  GtkFileFilter *all_filter;
  PeasEngine *engine;
  gint64 last_priority = G_MAXINT64;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (param == NULL);

  engine = peas_engine_get_default ();
  n_items = g_list_model_get_n_items (G_LIST_MODEL (engine));

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                         "transient-for", self,
                         "modal", TRUE,
                         "title", _("Select Project Folder"),
                         "visible", TRUE,
                         NULL);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("_Open"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  g_signal_connect_object (dialog,
                           "notify::filter",
                           G_CALLBACK (ide_greeter_workspace_dialog_notify_filter),
                           self,
                           G_CONNECT_SWAPPED);

  all_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (all_filter, _("All Project Types"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), all_filter);

  /* For testing with no plugins */
  if (n_items == 0)
    gtk_file_filter_add_pattern (all_filter, "*");

  for (guint j = 0; j < n_items; j++)
    {
      g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), j);
      const char *module_name = peas_plugin_info_get_module_name (plugin_info);
      GtkFileFilter *filter;
      const char *pattern;
      const char *content_type;
      const char *name;
      const char *priority;
      char **patterns;
      char **content_types;
      int i;

      if (!peas_plugin_info_is_loaded (plugin_info))
        continue;

      name = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Name");
      if (name == NULL)
        continue;

      pattern = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Pattern");
      content_type = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Content-Type");
      priority = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Priority");

      if (pattern == NULL && content_type == NULL)
        continue;

      patterns = g_strsplit (pattern ?: "", ",", 0);
      content_types = g_strsplit (content_type ?: "", ",", 0);

      filter = gtk_file_filter_new ();

      gtk_file_filter_set_name (filter, name);

      if (!ide_str_equal0 (module_name, "greeter"))
        g_object_set_data_full (G_OBJECT (filter), "MODULE_NAME", g_strdup (module_name), g_free);

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

      /* Look at the priority to set the default file filter. */
      if (priority != NULL)
        {
          gint64 pval = g_ascii_strtoll (priority, NULL, 10);

          if (pval < last_priority)
            {
              gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
              last_priority = pval;
            }
        }

      g_strfreev (patterns);
      g_strfreev (content_types);
    }

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (ide_greeter_workspace_dialog_response),
                           self,
                           G_CONNECT_SWAPPED);

  /* If unset, set the default filter */
  if (last_priority == G_MAXINT64)
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), all_filter);

  projects_dir = g_file_new_for_path (ide_get_projects_dir ());
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), projects_dir, NULL);

  gtk_window_present (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static void
ide_greeter_workspace_reset_action (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  ide_greeter_workspace_set_selection_mode (IDE_GREETER_WORKSPACE (widget), FALSE);
}

static void
ide_greeter_workspace_dispose (GObject *object)
{
  IdeGreeterWorkspace *self = (IdeGreeterWorkspace *)object;

  g_clear_object (&self->addins);
  g_clear_object (&self->delete_action);
  g_clear_object (&self->purge_action);
  g_clear_pointer (&self->pattern_spec, ide_pattern_spec_unref);

  G_OBJECT_CLASS (ide_greeter_workspace_parent_class)->dispose (object);
}

static void
ide_greeter_workspace_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeGreeterWorkspace *self = IDE_GREETER_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_SELECTION_MODE:
      g_value_set_boolean (value, ide_greeter_workspace_get_selection_mode (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_greeter_workspace_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeGreeterWorkspace *self = IDE_GREETER_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_SELECTION_MODE:
      ide_greeter_workspace_set_selection_mode (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_greeter_workspace_class_init (IdeGreeterWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  object_class->constructed = ide_greeter_workspace_constructed;
  object_class->dispose = ide_greeter_workspace_dispose;
  object_class->get_property = ide_greeter_workspace_get_property;
  object_class->set_property = ide_greeter_workspace_set_property;

  workspace_class->restore_size = NULL;
  workspace_class->save_size = NULL;

  /**
   * IdeGreeterWorkspace:selection-mode:
   *
   * The "selection-mode" property indicates if the workspace allows
   * selecting existing projects and removing them, including source files
   * and cached data.
   *
   * This is usually used by the checkmark button to toggle selections.
   */
  properties [PROP_SELECTION_MODE] =
    g_param_spec_boolean ("selection-mode",
                          "Selection Mode",
                          "If the workspace is in selection mode",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [OPEN_PROJECT] =
    g_signal_new_class_handler ("open-project",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_greeter_workspace_real_open_project),
                                g_signal_accumulator_true_handled, NULL,
                                ide_marshal_BOOLEAN__OBJECT,
                                G_TYPE_BOOLEAN,
                                1,
                                IDE_TYPE_PROJECT_INFO | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [OPEN_PROJECT],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_BOOLEAN__OBJECTv);

  ide_workspace_class_set_kind (workspace_class, "greeter");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-greeter/ide-greeter-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, action_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, buttons_section);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, empty_state);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, navigation_view);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, projects_action_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, sections);
  gtk_widget_class_bind_template_callback (widget_class, navigation_view_notify_visible_page_cb);

  gtk_widget_class_install_action (widget_class, "greeter.open", NULL, ide_greeter_workspace_open_action);
  gtk_widget_class_install_action (widget_class, "greeter.page", "s", ide_greeter_workspace_page_action);
  gtk_widget_class_install_action (widget_class, "greeter.reset", NULL, ide_greeter_workspace_reset_action);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Left, GDK_ALT_MASK, "win.page", "s", "overview");
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_w, GDK_CONTROL_MASK, "window.close", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "greeter.reset", NULL);

  g_type_ensure (IDE_TYPE_GREETER_BUTTONS_SECTION);
  g_type_ensure (IDE_TYPE_GREETER_ROW);

  g_resources_register (ide_greeter_get_resource ());
}

static void
ide_greeter_workspace_init (IdeGreeterWorkspace *self)
{
  static const GActionEntry actions[] = {
    { "purge-selected-rows", ide_greeter_workspace_purge_selected_rows },
    { "delete-selected-rows", ide_greeter_workspace_delete_selected_rows },
  };

  g_autoptr(GPropertyAction) selection_action = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  selection_action = g_property_action_new ("selection-mode", G_OBJECT (self), "selection-mode");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (selection_action));
  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "greeter.reset", FALSE);

  g_signal_connect_object (self->search_entry,
                           "activate",
                           G_CALLBACK (ide_greeter_workspace_search_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (ide_greeter_workspace_search_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect (self->search_entry,
                    "stop-search",
                    G_CALLBACK (gtk_editable_set_text),
                    (gpointer) "");

  navigation_view_notify_visible_page_cb (self);
}

IdeGreeterWorkspace *
ide_greeter_workspace_new (IdeApplication *app)
{
  return g_object_new (IDE_TYPE_GREETER_WORKSPACE,
                       "application", app,
                       "default-width", 1000,
                       "default-height", 800,
                       NULL);
}

/**
 * ide_greeter_workspace_add_section:
 * @self: a #IdeGreeterWorkspace
 * @section: an #IdeGreeterSection based #GtkWidget
 *
 * Adds the #IdeGreeterSection to the display.
 */
void
ide_greeter_workspace_add_section (IdeGreeterWorkspace *self,
                                   IdeGreeterSection   *section)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (IDE_IS_GREETER_SECTION (section));

  g_signal_connect_object (section,
                           "project-activated",
                           G_CALLBACK (ide_greeter_workspace_project_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  add_with_priority (GTK_WIDGET (self->sections),
                     GTK_WIDGET (section),
                     ide_greeter_section_get_priority (section));

  gtk_widget_set_visible (GTK_WIDGET (section),
                          ide_greeter_section_filter (section, NULL));

  gtk_widget_set_visible (GTK_WIDGET (self->empty_state),
                          !ide_greeter_workspace_has_match (self));
}

/**
 * ide_greeter_workspace_remove_section:
 * @self: a #IdeGreeterWorkspace
 * @section: an #IdeGreeterSection based #GtkWidget
 *
 * Remvoes the #IdeGreeterSection from the display. This should be a section
 * that was previously added with ide_greeter_workspace_add_section().
 *
 * Plugins should clean up after themselves when they are unloaded, which may
 * include calling this function.
 */
void
ide_greeter_workspace_remove_section (IdeGreeterWorkspace *self,
                                      IdeGreeterSection   *section)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (IDE_IS_GREETER_SECTION (section));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (section)) == GTK_WIDGET (self->sections));

  gtk_box_remove (self->sections, GTK_WIDGET (section));
}

void
ide_greeter_workspace_add_button (IdeGreeterWorkspace *self,
                                  GtkWidget           *button,
                                  gint                 priority)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (GTK_IS_WIDGET (button));

  ide_greeter_buttons_section_add_button (self->buttons_section, priority, button);
}

/**
 * ide_greeter_workspace_begin:
 * @self: a #IdeGreeterWorkspace
 *
 * This function will disable various actions and should be called before
 * an #IdeGreeterAddin begins doing work that cannot be undone except to
 * cancel the operation.
 *
 * Actions such as switching guides will be disabled during this process.
 *
 * See ide_greeter_workspace_end() to restore actions.
 */
void
ide_greeter_workspace_begin (IdeGreeterWorkspace *self)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));

  self->busy = TRUE;

  gtk_widget_set_sensitive (GTK_WIDGET (self->sections), FALSE);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "greeter.page", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "greeter.open", FALSE);
}

/**
 * ide_greeter_workspace_end:
 * @self: a #IdeGreeterWorkspace
 *
 * Restores actions after a call to ide_greeter_workspace_begin().
 */
void
ide_greeter_workspace_end (IdeGreeterWorkspace *self)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));

  self->busy = FALSE;

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "greeter.page", TRUE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "greeter.open", TRUE);

  gtk_widget_set_sensitive (GTK_WIDGET (self->sections), TRUE);
}

/**
 * ide_greeter_workspace_get_selection_mode:
 * @self: a #IdeGreeterWorkspace
 *
 * Gets if the greeter is in selection mode, which means that the workspace
 * allows selecting projects for removal.
 *
 * Returns: %TRUE if in selection mode, otherwise %FALSE
 */
gboolean
ide_greeter_workspace_get_selection_mode (IdeGreeterWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_WORKSPACE (self), FALSE);

  return self->selection_mode;
}

/**
 * ide_greeter_workspace_set_selection_mode:
 * @self: a #IdeGreeterWorkspace
 * @selection_mode: if the workspace should be in selection mode
 *
 * Sets the workspace in selection mode.
 */
void
ide_greeter_workspace_set_selection_mode (IdeGreeterWorkspace *self,
                                          gboolean             selection_mode)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));

  selection_mode = !!selection_mode;

  if (selection_mode != self->selection_mode)
    {
      self->selection_mode = selection_mode;

      for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->sections));
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          if (IDE_IS_GREETER_SECTION (child))
            ide_greeter_section_set_selection_mode (IDE_GREETER_SECTION (child), selection_mode);
        }

      gtk_widget_action_set_enabled (GTK_WIDGET (self), "greeter.reset", selection_mode);

      gtk_widget_set_visible (GTK_WIDGET (self->action_bar), selection_mode);
      gtk_widget_set_visible (GTK_WIDGET (self->projects_action_bar), !selection_mode);

      ide_greeter_workspace_apply_filter_all (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTION_MODE]);
    }
}

/**
 * ide_greeter_workspace_get_visible_page:
 *
 * Returns: (transfer none) (nullable): the current page, or %NULL if not
 *   page has been added yet.
 */
AdwNavigationPage *
ide_greeter_workspace_get_visible_page (IdeGreeterWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_WORKSPACE (self), NULL);

  return adw_navigation_view_get_visible_page (self->navigation_view);
}

void
ide_greeter_workspace_push_page (IdeGreeterWorkspace *self,
                                 AdwNavigationPage   *page)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (ADW_IS_NAVIGATION_PAGE (page));

  adw_navigation_view_push (self->navigation_view, page);
}

void
ide_greeter_workspace_push_page_by_tag (IdeGreeterWorkspace *self,
                                        const char          *tag)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (tag != NULL);

  adw_navigation_view_push_by_tag (self->navigation_view, tag);
}

void
ide_greeter_workspace_add_page (IdeGreeterWorkspace *self,
                                AdwNavigationPage   *page)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (ADW_IS_NAVIGATION_PAGE (page));

  adw_navigation_view_add (self->navigation_view, page);
}

void
ide_greeter_workspace_remove_page (IdeGreeterWorkspace *self,
                                   AdwNavigationPage   *page)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (ADW_IS_NAVIGATION_PAGE (page));

  adw_navigation_view_remove (self->navigation_view, page);
}

/**
 * ide_greeter_workspace_find_page:
 * @self: a #IdeGreeterWorkspace
 *
 * Finds a page that was added, by its tag.
 *
 * Returns: (transfer none) (nullable): a #AdwNavigationPage or %NULL
 */
AdwNavigationPage *
ide_greeter_workspace_find_page (IdeGreeterWorkspace *self,
                                 const char          *tag)
{
  g_return_val_if_fail (IDE_IS_GREETER_WORKSPACE (self), NULL);
  g_return_val_if_fail (tag != NULL, NULL);

  return adw_navigation_view_find_page (self->navigation_view, tag);
}

gboolean
ide_greeter_workspace_is_busy (IdeGreeterWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_WORKSPACE (self), FALSE);

  return self->busy;
}
