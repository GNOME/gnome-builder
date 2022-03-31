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
#include <libpeas/peas.h>

#include "ide-greeter-buttons-section.h"
#include "ide-greeter-private.h"
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
  GtkBox                   *left_box;
  GtkStack                 *pages;
  GtkSearchEntry           *search_entry;
  GtkButton                *back_button;
  GtkButton                *select_button;
  GtkActionBar             *action_bar;
  GtkActionBar             *projects_action_bar;
  AdwWindowTitle           *title;
  IdeGreeterButtonsSection *buttons_section;
  AdwStatusPage            *empty_state;

  guint                     selection_mode : 1;
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
                                       PeasExtension    *exten,
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
stack_notify_visible_child_cb (IdeGreeterWorkspace *self,
                               GParamSpec          *pspec,
                               GtkStack            *stack)
{
  g_autofree gchar *title = NULL;
  g_autofree gchar *full_title = NULL;
  GtkStackPage *page;
  GtkWidget *visible_child;
  gboolean overview;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_STACK (stack));

  visible_child = gtk_stack_get_visible_child (stack);
  page = gtk_stack_get_page (stack, visible_child);

  if (page != NULL)
    {
      if ((title = g_strdup (gtk_stack_page_get_title (page))))
        full_title = g_strdup_printf (_("Builder — %s"), title);
    }

  adw_window_title_set_title (self->title, title);
  gtk_window_set_title (GTK_WINDOW (self), full_title);

  overview = ide_str_equal0 ("overview", gtk_stack_get_visible_child_name (stack));

  gtk_widget_set_visible (GTK_WIDGET (self->left_box), overview);
  gtk_widget_set_visible (GTK_WIDGET (self->back_button), !overview);
  gtk_widget_set_visible (GTK_WIDGET (self->select_button), overview);
}

static void
ide_greeter_workspace_addin_added_cb (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      PeasExtension    *exten,
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
                                        PeasExtension    *exten,
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
  ide_greeter_workspace_set_page_name (self, "overview");

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static void
tear_workbench_down (GtkDialog    *dialog,
                     int           response,
                     IdeWorkbench *workbench)
{
  IDE_ENTRY;

  g_assert (GTK_IS_DIALOG (dialog));
  g_assert (IDE_IS_WORKBENCH (workbench));

  gtk_window_destroy (GTK_WINDOW (dialog));
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
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                       GTK_DIALOG_USE_HEADER_BAR,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Failed to load the project"));

      g_object_set (dialog,
                    "modal", TRUE,
                    "secondary-text", error->message,
                    NULL);

      g_signal_connect_object (dialog,
                               "response",
                               G_CALLBACK (tear_workbench_down),
                               workbench,
                               0);

      gtk_window_present (GTK_WINDOW (dialog));

      ide_greeter_workspace_end (self);
    }

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
        }
    }

  return FALSE;

#if 0
  /*
   * to switch to the clone dialog. However, we can use the VCS Uri to
   * determine what the check-out directory would be, and if so, we can
   * just open that directory.
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
        }
      else
        {
          ide_clone_page_set_uri (self->clone_page, vcs_uri);
          ide_greeter_workspace_set_page_name (self, "clone");
          return;
        }
    }
#endif

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
ide_greeter_workspace_click_pressed_cb (IdeGreeterWorkspace *self,
                                        guint                n_press,
                                        double               x,
                                        double               y,
                                        GtkGestureClick     *gesture)
{
  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_GESTURE_CLICK (gesture));

  ide_greeter_workspace_set_page_name (self, "overview");
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

  ide_greeter_workspace_apply_filter_all (self);
  ide_greeter_workspace_set_selection_mode (self, FALSE);
}

static void
purge_selected_rows_response (IdeGreeterWorkspace *self,
                              gint                 response,
                              GtkDialog           *dialog)
{
  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->sections));
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          if (IDE_IS_GREETER_SECTION (child))
            ide_greeter_section_purge_selected (IDE_GREETER_SECTION (child));
        }

      ide_greeter_workspace_apply_filter_all (self);
      ide_greeter_workspace_set_selection_mode (self, FALSE);
    }

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
ide_greeter_workspace_purge_selected_rows (GSimpleAction *action,
                                           GVariant      *param,
                                           gpointer       user_data)
{
  IdeGreeterWorkspace *self = user_data;
  GtkWidget *parent;
  GtkWidget *button;
  GtkDialog *dialog;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param == NULL);
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  parent = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  dialog = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "modal", TRUE,
                         "transient-for", parent,
                         "text", _("Delete Project Sources?"),
                         "secondary-text", _("Deleting the project source code from your computer cannot be undone."),
                         NULL);
  gtk_dialog_add_buttons (dialog,
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Delete Project Sources"), GTK_RESPONSE_OK,
                          NULL);
  button = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_OK);
  gtk_widget_add_css_class (button, "destructive-action");
  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (purge_selected_rows_response),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_window_present (GTK_WINDOW (dialog));
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
                                NULL,
                                G_TYPE_BOOLEAN, 1, IDE_TYPE_PROJECT_INFO);

  ide_workspace_class_set_kind (workspace_class, "greeter");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-greeter/ide-greeter-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, action_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, back_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, buttons_section);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, empty_state);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, left_box);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, projects_action_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, sections);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, select_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, pages);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, title);
  gtk_widget_class_bind_template_callback (widget_class, stack_notify_visible_child_cb);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Left, GDK_ALT_MASK, "win.page", "s", "overview");
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_w, GDK_CONTROL_MASK, "window.close", NULL);

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
  GtkGesture *gesture;

  gtk_widget_init_template (GTK_WIDGET (self));

  selection_action = g_property_action_new ("selection-mode", G_OBJECT (self), "selection-mode");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (selection_action));
  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 8);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

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

  g_signal_connect_object (gesture,
                           "pressed",
                           G_CALLBACK (ide_greeter_workspace_click_pressed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  stack_notify_visible_child_cb (self, NULL, self->pages);

  _ide_greeter_workspace_init_actions (self);
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

      gtk_widget_set_visible (GTK_WIDGET (self->action_bar), selection_mode);
      gtk_widget_set_visible (GTK_WIDGET (self->projects_action_bar), !selection_mode);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTION_MODE]);
    }
}

/**
 * ide_greeter_workspace_get_page:
 *
 * Returns: (transfer none) (nullable): the current page, or %NULL if not
 *   page has been added yet.
 */
GtkWidget *
ide_greeter_workspace_get_page (IdeGreeterWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_WORKSPACE (self), NULL);

  return gtk_stack_get_visible_child (self->pages);
}

void
ide_greeter_workspace_set_page (IdeGreeterWorkspace *self,
                                GtkWidget           *page)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (!page || GTK_IS_WIDGET (page));

  if (page != NULL)
    gtk_stack_set_visible_child (self->pages, page);
  else
    gtk_stack_set_visible_child_name (self->pages, "overview");
}

const char *
ide_greeter_workspace_get_page_name (IdeGreeterWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_WORKSPACE (self), NULL);

  return gtk_stack_get_visible_child_name (self->pages);
}

void
ide_greeter_workspace_set_page_name (IdeGreeterWorkspace *self,
                                     const char          *name)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));

  if (name == NULL)
    name = "overview";

  gtk_stack_set_visible_child_name (self->pages, name);
}
