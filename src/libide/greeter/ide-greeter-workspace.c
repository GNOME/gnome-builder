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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-clone-surface.h"
#include "ide-greeter-buttons-section.h"
#include "ide-greeter-private.h"
#include "ide-greeter-workspace.h"

/**
 * SECTION:ide-greeter-workspace
 * @title: IdeGreeterWorkspace
 * @short_description: The greeter upon starting Builder
 *
 * Use the #IdeWorkspace APIs to add surfaces for user guides such
 * as the git workflow or project creation wizard.
 *
 * You can add buttons to the headerbar and use actions to change
 * surfaces such as "win.surface::'surface-name'".
 *
 * Since: 3.32
 */

struct _IdeGreeterWorkspace
{
  IdeWorkspace              parent_instance;

  PeasExtensionSet         *addins;
  DzlPatternSpec           *pattern_spec;
  GSimpleAction            *delete_action;
  GSimpleAction            *purge_action;

  /* Template Widgets */
  IdeCloneSurface          *clone_surface;
  IdeHeaderBar             *header_bar;
  DzlPriorityBox           *sections;
  DzlPriorityBox           *left_box;
  GtkStack                 *surfaces;
  IdeSurface               *sections_surface;
  GtkSearchEntry           *search_entry;
  GtkButton                *back_button;
  GtkButton                *select_button;
  GtkActionBar             *action_bar;
  GtkActionBar             *projects_action_bar;
  GtkLabel                 *title;
  IdeGreeterButtonsSection *buttons_section;
  DzlEmptyState            *empty_state;
  GtkGestureMultiPress     *multipress_gesture;

  guint                     selection_mode : 1;
};

G_DEFINE_TYPE (IdeGreeterWorkspace, ide_greeter_workspace, IDE_TYPE_WORKSPACE)

enum {
  PROP_0,
  PROP_SELECTION_MODE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_greeter_workspace_has_match_cb (GtkWidget *widget,
                                    gpointer   user_data)
{
  gboolean *match = user_data;

  if (IDE_IS_GREETER_SECTION (widget))
    *match |= gtk_widget_get_visible (widget);
}

static gboolean
ide_greeter_workspace_has_match (IdeGreeterWorkspace *self)
{
  gboolean match = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  gtk_container_foreach (GTK_CONTAINER (self->sections),
                         ide_greeter_workspace_has_match_cb,
                         &match);

  return match;
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

  g_clear_pointer (&self->pattern_spec, dzl_pattern_spec_unref);

  if (NULL != (text = gtk_entry_get_text (GTK_ENTRY (self->search_entry))))
    self->pattern_spec = dzl_pattern_spec_new (text);

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins,
                                ide_greeter_workspace_filter_sections,
                                self);

  gtk_widget_set_visible (GTK_WIDGET (self->empty_state),
                          !ide_greeter_workspace_has_match (self));
}

static void
ide_greeter_workspace_activate_cb (GtkWidget *widget,
                                   gpointer   user_data)
{
  gboolean *handled = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (handled != NULL);

  if (!IDE_IS_GREETER_SECTION (widget))
    return;

  if (!*handled)
    *handled = ide_greeter_section_activate_first (IDE_GREETER_SECTION (widget));
}

static void
ide_greeter_workspace_search_entry_activate (IdeGreeterWorkspace *self,
                                             GtkSearchEntry      *search_entry)
{
  gboolean handled = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  gtk_container_foreach (GTK_CONTAINER (self->sections),
                         ide_greeter_workspace_activate_cb,
                         &handled);

  if (!handled)
    gdk_window_beep (gtk_widget_get_window (GTK_WIDGET (search_entry)));
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
  GtkWidget *visible_child;
  gboolean sections;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_STACK (stack));

  visible_child = gtk_stack_get_visible_child (stack);

  if (DZL_IS_DOCK_ITEM (visible_child))
    {
      if ((title = dzl_dock_item_get_title (DZL_DOCK_ITEM (visible_child))))
        full_title = g_strdup_printf (_("Builder — %s"), title);
    }

  gtk_label_set_label (self->title, title);
  gtk_window_set_title (GTK_WINDOW (self), full_title);

  sections = ide_str_equal0 ("sections", gtk_stack_get_visible_child_name (stack));

  gtk_widget_set_visible (GTK_WIDGET (self->left_box), sections);
  gtk_widget_set_visible (GTK_WIDGET (self->back_button), !sections);
  gtk_widget_set_visible (GTK_WIDGET (self->select_button), sections);
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

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GREETER_SECTION (section));
  g_assert (IDE_IS_GREETER_WORKSPACE (user_data));

  gtk_widget_destroy (GTK_WIDGET (section));
}

static void
ide_greeter_workspace_constructed (GObject *object)
{
  IdeGreeterWorkspace *self = (IdeGreeterWorkspace *)object;

  G_OBJECT_CLASS (ide_greeter_workspace_parent_class)->constructed (object);

  dzl_gtk_widget_add_style_class (GTK_WIDGET (self), "greeter");

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
  ide_workspace_set_visible_surface_name (IDE_WORKSPACE (self), "sections");

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
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

      g_signal_connect (dialog,
                        "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      g_signal_connect_swapped (dialog,
                                "response",
                                G_CALLBACK (gtk_widget_destroy),
                                workbench);

      ide_gtk_window_present (GTK_WINDOW (dialog));

      ide_greeter_workspace_end (self);
    }

  gtk_widget_destroy (GTK_WIDGET (self));

  IDE_EXIT;
}

/**
 * ide_greeter_workspace_open_project:
 * @self: an #IdeGreeterWorkspace
 * @project_info: an #IdeProjectInfo
 *
 * Opens the project described by @project_info.
 *
 * This is useful by greeter workspace extensions that add new surfaces
 * which may not have other means to activate a project.
 *
 * Since: 3.32
 */
void
ide_greeter_workspace_open_project (IdeGreeterWorkspace *self,
                                    IdeProjectInfo      *project_info)
{
  IdeWorkbench *workbench;
  const gchar *vcs_uri = NULL;
  GFile *file;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));

  /* If there is a VCS Uri and no project file/directory, then we want
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
          ide_clone_surface_set_uri (self->clone_surface, vcs_uri);
          ide_workspace_set_visible_surface_name (IDE_WORKSPACE (self), "clone");
          return;
        }
    }

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));

  ide_greeter_workspace_begin (self);

  if (ide_project_info_get_directory (project_info) == NULL)
    {
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

  IDE_EXIT;
}

static void
ide_greeter_workspace_multipress_gesture_pressed_cb (GtkGestureMultiPress *gesture,
                                                     guint                 n_press,
                                                     gdouble               x,
                                                     gdouble               y,
                                                     IdeGreeterWorkspace  *self)
{
  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_GESTURE_MULTI_PRESS (gesture));

  ide_workspace_set_visible_surface_name (IDE_WORKSPACE (self), "sections");
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
ide_greeter_workspace_delete_selected_rows_cb (GtkWidget *widget,
                                               gpointer   user_data)
{
  if (IDE_IS_GREETER_SECTION (widget))
    ide_greeter_section_delete_selected (IDE_GREETER_SECTION (widget));
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

  gtk_container_foreach (GTK_CONTAINER (self->sections),
                         ide_greeter_workspace_delete_selected_rows_cb,
                         NULL);
  ide_greeter_workspace_apply_filter_all (self);
  ide_greeter_workspace_set_selection_mode (self, FALSE);
}

static void
ide_greeter_workspace_purge_selected_rows_cb (GtkWidget *widget,
                                              gpointer   user_data)
{
  if (IDE_IS_GREETER_SECTION (widget))
    ide_greeter_section_purge_selected (IDE_GREETER_SECTION (widget));
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
      gtk_container_foreach (GTK_CONTAINER (self->sections),
                             ide_greeter_workspace_purge_selected_rows_cb,
                             NULL);
      ide_greeter_workspace_apply_filter_all (self);
      ide_greeter_workspace_set_selection_mode (self, FALSE);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
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
                         "attached-to", parent,
                         "text", _("Removing project sources will delete them from your computer and cannot be undone."),
                         NULL);
  gtk_dialog_add_buttons (dialog,
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Delete Project Sources"), GTK_RESPONSE_OK,
                          NULL);
  button = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_OK);
  dzl_gtk_widget_add_style_class (button, "destructive-action");
  g_signal_connect_data (dialog,
                         "response",
                         G_CALLBACK (purge_selected_rows_response),
                         g_object_ref (self),
                         (GClosureNotify)g_object_unref,
                         G_CONNECT_SWAPPED);
  ide_gtk_window_present (GTK_WINDOW (dialog));
}

static void
ide_greeter_workspace_destroy (GtkWidget *widget)
{
  IdeGreeterWorkspace *self = (IdeGreeterWorkspace *)widget;

  g_clear_object (&self->addins);
  g_clear_object (&self->delete_action);
  g_clear_object (&self->purge_action);
  g_clear_object (&self->multipress_gesture);
  g_clear_pointer (&self->pattern_spec, dzl_pattern_spec_unref);

  GTK_WIDGET_CLASS (ide_greeter_workspace_parent_class)->destroy (widget);
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
  object_class->get_property = ide_greeter_workspace_get_property;
  object_class->set_property = ide_greeter_workspace_set_property;

  widget_class->destroy = ide_greeter_workspace_destroy;

  /**
   * IdeGreeterWorkspace:selection-mode:
   *
   * The "selection-mode" property indicates if the workspace allows
   * selecting existing projects and removing them, including source files
   * and cached data.
   *
   * This is usually used by the checkmark button to toggle selections.
   *
   * Since: 3.32
   */
  properties [PROP_SELECTION_MODE] =
    g_param_spec_boolean ("selection-mode",
                          "Selection Mode",
                          "If the workspace is in selection mode",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  ide_workspace_class_set_kind (workspace_class, "greeter");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-greeter-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, action_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, back_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, buttons_section);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, clone_surface);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, empty_state);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, left_box);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, projects_action_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, sections);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, select_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, surfaces);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterWorkspace, title);
  gtk_widget_class_bind_template_callback (widget_class, stack_notify_visible_child_cb);

  g_type_ensure (IDE_TYPE_CLONE_SURFACE);
  g_type_ensure (IDE_TYPE_GREETER_BUTTONS_SECTION);
}

static void
ide_greeter_workspace_init (IdeGreeterWorkspace *self)
{
  g_autoptr(GPropertyAction) selection_action = NULL;
  static const GActionEntry actions[] = {
    { "purge-selected-rows", ide_greeter_workspace_purge_selected_rows },
    { "delete-selected-rows", ide_greeter_workspace_delete_selected_rows },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  selection_action = g_property_action_new ("selection-mode", G_OBJECT (self), "selection-mode");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (selection_action));
  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);
  self->multipress_gesture = GTK_GESTURE_MULTI_PRESS (gtk_gesture_multi_press_new (GTK_WIDGET (self)));
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->multipress_gesture), 8);

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
                    G_CALLBACK (gtk_entry_set_text),
                    (gpointer) "");

  g_signal_connect (self->multipress_gesture,
                    "pressed",
                    G_CALLBACK (ide_greeter_workspace_multipress_gesture_pressed_cb),
                    self);

  stack_notify_visible_child_cb (self, NULL, self->surfaces);

  _ide_greeter_workspace_init_actions (self);
  _ide_greeter_workspace_init_shortcuts (self);
}

IdeGreeterWorkspace *
ide_greeter_workspace_new (IdeApplication *app)
{
  return g_object_new (IDE_TYPE_GREETER_WORKSPACE,
                       "application", app,
                       "default-width", 1000,
                       "default-height", 600,
                       NULL);
}

/**
 * ide_greeter_workspace_add_section:
 * @self: a #IdeGreeterWorkspace
 * @section: an #IdeGreeterSection based #GtkWidget
 *
 * Adds the #IdeGreeterSection to the display.
 *
 * Since: 3.32
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

  gtk_container_add_with_properties (GTK_CONTAINER (self->sections), GTK_WIDGET (section),
                                     "priority", ide_greeter_section_get_priority (section),
                                     NULL);

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
 *
 * Since: 3.32
 */
void
ide_greeter_workspace_remove_section (IdeGreeterWorkspace *self,
                                      IdeGreeterSection   *section)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));
  g_return_if_fail (IDE_IS_GREETER_SECTION (section));

  gtk_container_remove (GTK_CONTAINER (self->sections), GTK_WIDGET (section));
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
 *
 * Since: 3.32
 */
void
ide_greeter_workspace_begin (IdeGreeterWorkspace *self)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));

  gtk_widget_set_sensitive (GTK_WIDGET (self->sections), FALSE);

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "win", "open",
                             "enabled", FALSE,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "win", "surface",
                             "enabled", FALSE,
                             NULL);
}

/**
 * ide_greeter_workspace_end:
 * @self: a #IdeGreeterWorkspace
 *
 * Restores actions after a call to ide_greeter_workspace_begin().
 *
 * Since: 3.32
 */
void
ide_greeter_workspace_end (IdeGreeterWorkspace *self)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "win", "open",
                             "enabled", TRUE,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "win", "surface",
                             "enabled", TRUE,
                             NULL);

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
 *
 * Since: 3.32
 */
gboolean
ide_greeter_workspace_get_selection_mode (IdeGreeterWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_WORKSPACE (self), FALSE);

  return self->selection_mode;
}

static void
ide_greeter_workspace_set_selection_mode_cb (GtkWidget *widget,
                                             gpointer   user_data)
{
  if (IDE_IS_GREETER_SECTION (widget))
    ide_greeter_section_set_selection_mode (IDE_GREETER_SECTION (widget),
                                            GPOINTER_TO_INT (user_data));
}

/**
 * ide_greeter_workspace_set_selection_mode:
 * @self: a #IdeGreeterWorkspace
 * @selection_mode: if the workspace should be in selection mode
 *
 * Sets the workspace in selection mode.
 *
 * Since: 3.32
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
      gtk_container_foreach (GTK_CONTAINER (self->sections),
                             ide_greeter_workspace_set_selection_mode_cb,
                             GINT_TO_POINTER (selection_mode));
      gtk_widget_set_visible (GTK_WIDGET (self->action_bar), selection_mode);
      gtk_widget_set_visible (GTK_WIDGET (self->projects_action_bar), !selection_mode);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTION_MODE]);
    }
}
