/* ide-greeter-perspective.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-debug.h"
#include "ide-macros.h"

#include "application/ide-application.h"
#include "genesis/ide-genesis-addin.h"
#include "greeter/ide-greeter-perspective.h"
#include "greeter/ide-greeter-project-row.h"
#include "greeter/ide-greeter-section.h"
#include "util/ide-gtk.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench-private.h"
#include "workbench/ide-workbench.h"

struct _IdeGreeterPerspective
{
  GtkBin                parent_instance;

  DzlPatternSpec       *pattern_spec;
  PeasExtensionSet     *genesis_set;

  GBinding             *ready_binding;
  GCancellable         *cancellable;

  PeasExtensionSet     *sections;

  GtkStack             *stack;
  GtkStack             *top_stack;
  GtkButton            *genesis_continue_button;
  GtkButton            *genesis_cancel_button;
  GtkLabel             *genesis_title;
  GtkStack             *genesis_stack;
  GtkInfoBar           *info_bar;
  GtkLabel             *info_bar_label;
  GtkRevealer          *info_bar_revealer;
  GtkToggleButton      *selection_button;
  GtkViewport          *viewport;
  GtkWidget            *titlebar;
  GtkButton            *open_button;
  GtkButton            *cancel_button;
  GtkButton            *remove_button;
  GtkSearchEntry       *search_entry;
  DzlStateMachine      *state_machine;
  GtkScrolledWindow    *scrolled_window;
  DzlPriorityBox       *genesis_buttons;
  DzlPriorityBox       *sections_container;

  gint                  selected_count;
};

typedef struct
{
  IdeGreeterPerspective *self;
  IdeVcsUri             *vcs_uri;
  gboolean               handled;
} LoadProject;

static void     ide_perspective_iface_init               (IdePerspectiveInterface *iface);
static void     ide_greeter_perspective_genesis_continue (IdeGreeterPerspective   *self);
static gboolean ide_greeter_perspective_load_project     (IdeGreeterPerspective   *self,
                                                          IdeProjectInfo          *project_info);

G_DEFINE_TYPE_EXTENDED (IdeGreeterPerspective, ide_greeter_perspective, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE,
                                               ide_perspective_iface_init))

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
ide_greeter_perspective_activate_cb (GtkWidget *widget,
                                     gpointer   user_data)
{
  gboolean *handled = user_data;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (handled != NULL);

  if (!IDE_IS_GREETER_SECTION (widget))
    return;

  if (!*handled)
    *handled = ide_greeter_section_activate_first (IDE_GREETER_SECTION (widget));
}

static void
ide_greeter_perspective__search_entry_activate (IdeGreeterPerspective *self,
                                                GtkSearchEntry        *search_entry)
{
  gboolean handled = FALSE;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  gtk_container_foreach (GTK_CONTAINER (self->sections_container),
                         ide_greeter_perspective_activate_cb,
                         &handled);

  if (!handled)
    gdk_window_beep (gtk_widget_get_window (GTK_WIDGET (search_entry)));
}

static void
ide_greeter_perspective_filter_sections (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         PeasExtension    *exten,
                                         gpointer          user_data)
{
  IdeGreeterPerspective *self = user_data;
  IdeGreeterSection *section = (IdeGreeterSection *)exten;
  gboolean has_child;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GREETER_SECTION (section));
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  has_child = ide_greeter_section_filter (section, self->pattern_spec);

  gtk_widget_set_visible (GTK_WIDGET (section), has_child);
}

static void
ide_greeter_perspective_apply_filter_all (IdeGreeterPerspective *self)
{
  const gchar *text;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  g_clear_pointer (&self->pattern_spec, dzl_pattern_spec_unref);

  if (NULL != (text = gtk_entry_get_text (GTK_ENTRY (self->search_entry))))
    self->pattern_spec = dzl_pattern_spec_new (text);

  if (self->sections != NULL)
    peas_extension_set_foreach (self->sections,
                                ide_greeter_perspective_filter_sections,
                                self);
}

static void
ide_greeter_perspective__search_entry_changed (IdeGreeterPerspective *self,
                                               GtkSearchEntry        *search_entry)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  ide_greeter_perspective_apply_filter_all (self);
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
ide_greeter_perspective_dialog_response (IdeGreeterPerspective *self,
                                         gint                   response_id,
                                         GtkFileChooserDialog  *dialog)
{
  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_OK)
    {
      IdeWorkbench *workbench;

      workbench = ide_widget_get_workbench (GTK_WIDGET (self));

      if (workbench != NULL)
        {
          g_autoptr(GFile) project_file = NULL;

          gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
          gtk_widget_set_sensitive (GTK_WIDGET (self->titlebar), FALSE);

          project_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

          ide_workbench_open_project_async (workbench,
                                            project_file,
                                            NULL,
                                            ide_greeter_perspective_open_project_cb,
                                            g_object_ref (self));
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
  ide_greeter_perspective_apply_filter_all (self);

  /* TODO: If there are items, we need to go back to empty */
  dzl_state_machine_set_state (self->state_machine, "browse");
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

static gchar *
get_project_directory (const gchar *name)
{
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *projects = NULL;

  settings = g_settings_new ("org.gnome.builder");
  projects = g_settings_get_string (settings, "projects-directory");

  if (!g_path_is_absolute (projects))
    return g_build_filename (g_get_home_dir (), projects, name, NULL);
  else
    return g_build_filename (projects, name, NULL);
}

static void
ide_greeter_perspective_load_project_cb (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         PeasExtension    *exten,
                                         gpointer          user_data)
{
  IdeGenesisAddin *addin = (IdeGenesisAddin *)exten;
  LoadProject *load = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GENESIS_ADDIN (addin));
  g_assert (load != NULL);
  g_assert (IDE_IS_GREETER_PERSPECTIVE (load->self));
  g_assert (load->vcs_uri != NULL);

  if (load->handled)
    return;

  load->handled = ide_genesis_addin_apply_uri (addin, load->vcs_uri);

  if (load->handled)
    {
      GtkWidget *child = ide_genesis_addin_get_widget (addin);

      if (child != NULL)
        {
          gtk_stack_set_visible_child (load->self->genesis_stack, child);
          dzl_state_machine_set_state (load->self->state_machine, "genesis");
          gtk_widget_hide (GTK_WIDGET (load->self->genesis_continue_button));
          ide_greeter_perspective_genesis_continue (load->self);
        }
    }
}

static gboolean
ide_greeter_perspective_load_project (IdeGreeterPerspective *self,
                                      IdeProjectInfo        *project_info)
{
  IdeWorkbench *workbench;
  IdeVcsUri *vcs_uri;
  GFile *project_file;

  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));

  /* Mark this project info as having been selected */
  ide_project_info_set_is_recent (project_info, TRUE);

  /* If the project info has a project file, open that. */
  if (NULL != (project_file = ide_project_info_get_file (project_info)))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->titlebar), FALSE);
      ide_workbench_open_project_async (workbench,
                                        project_file,
                                        NULL,
                                        ide_greeter_perspective_open_project_cb,
                                        g_object_ref (self));
      IDE_RETURN (TRUE);
    }

  /*
   * If this project info has a uri, we might be able to find it already
   * checked out on the system.
   */
  if (NULL != (vcs_uri = ide_project_info_get_vcs_uri (project_info)))
    {
      LoadProject load = { 0 };
      const gchar *path;

      if (NULL != (path = ide_vcs_uri_get_path (vcs_uri)))
        {
          IdeApplication *app = IDE_APPLICATION_DEFAULT;
          IdeRecentProjects *projects = ide_application_get_recent_projects (app);
          g_autofree gchar *dir = NULL;
          g_autofree gchar *maybe_project = NULL;
          g_autofree gchar *relocated = NULL;
          const gchar *previous = NULL;

          dir = g_path_get_basename (path);

          /* XXX: Would be nice if this could be abstracted */
          if (g_str_has_suffix (dir, ".git"))
            dir[strlen (dir) - 4] = '\0';

          maybe_project = get_project_directory (dir);

          /*
           * We might find the project already cloned (using our simple check
           * for the directory name), or possibly from our recent projects.
           */
          if (g_file_test (maybe_project, G_FILE_TEST_IS_DIR))
            previous = maybe_project;
          else
            previous = relocated = ide_recent_projects_find_by_directory (projects, maybe_project);

          if (previous != NULL)
            {
              g_autoptr(GFile) file = g_file_new_for_path (previous);

              gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
              gtk_widget_set_sensitive (GTK_WIDGET (self->titlebar), FALSE);
              ide_workbench_open_project_async (workbench,
                                                file,
                                                NULL,
                                                ide_greeter_perspective_open_project_cb,
                                                g_object_ref (self));
              IDE_RETURN (TRUE);
            }
        }

      /*
       * Okay, we didn't handle this, see if one of the genesis plugins
       * knows how to handle the given vcs uri.
       */

      load.self = self;
      load.vcs_uri = vcs_uri;
      load.handled = FALSE;

      peas_extension_set_foreach (self->genesis_set,
                                  ide_greeter_perspective_load_project_cb,
                                  &load);

      if (load.handled)
        IDE_RETURN (TRUE);
    }

  /*
   * TODO: Failed to locate something that could open this project.
   *       Notify the user of the error and continue.
   */

  IDE_RETURN (FALSE);
}

static void
ide_greeter_perspective_project_activated (IdeGreeterPerspective *self,
                                           IdeProjectInfo        *project_info,
                                           IdeGreeterSection     *section)
{
  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (IDE_IS_GREETER_SECTION (section));

  ide_greeter_perspective_load_project (self, project_info);

  IDE_EXIT;
}

static void
ide_greeter_perspective_notify_has_selection_cb (PeasExtensionSet *set,
                                                 PeasPluginInfo   *plugin_info,
                                                 PeasExtension    *exten,
                                                 gpointer          user_data)
{
  IdeGreeterSection *section = (IdeGreeterSection *)exten;
  gboolean *has_selection = user_data;

  g_return_if_fail (PEAS_IS_EXTENSION_SET (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (has_selection != NULL);
  g_return_if_fail (IDE_IS_GREETER_SECTION (section));

  if (*has_selection)
    return;

  g_object_get (section, "has-selection", has_selection, NULL);
}

static void
ide_greeter_perspective_notify_has_selection (IdeGreeterPerspective *self,
                                              GParamSpec            *pspec,
                                              IdeGreeterSection     *section)
{
  gboolean has_selection = FALSE;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (IDE_IS_GREETER_SECTION (section));

  peas_extension_set_foreach (self->sections,
                              ide_greeter_perspective_notify_has_selection_cb,
                              &has_selection);

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "greeter", "remove-selected-rows",
                             "enabled", has_selection,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "greeter", "purge-selected-rows",
                             "enabled", has_selection,
                             NULL);
}

static void
ide_greeter_perspective_section_added (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeGreeterPerspective *self = user_data;
  IdeGreeterSection *section = (IdeGreeterSection *)exten;
  gint priority;

  IDE_ENTRY;

  g_return_if_fail (PEAS_IS_EXTENSION_SET (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_GREETER_PERSPECTIVE (self));
  g_return_if_fail (IDE_IS_GREETER_SECTION (section));

  /* Take the floating GtkWidget reference */
  if (g_object_is_floating (section))
    g_object_ref_sink (section);

  g_signal_connect_object (section,
                           "notify::has-selection",
                           G_CALLBACK (ide_greeter_perspective_notify_has_selection),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (section,
                           "project-activated",
                           G_CALLBACK (ide_greeter_perspective_project_activated),
                           self,
                           G_CONNECT_SWAPPED);

  /* Add the section to our box with priority */
  priority = ide_greeter_section_get_priority (section);
  gtk_container_add_with_properties (GTK_CONTAINER (self->sections_container),
                                     GTK_WIDGET (section),
                                     "priority", priority,
                                     NULL);

  if (ide_greeter_section_filter (section, self->pattern_spec))
    {
      dzl_state_machine_set_state (self->state_machine, "browse");
      gtk_widget_show (GTK_WIDGET (section));
    }

  IDE_EXIT;
}

static void
ide_greeter_perspective_section_removed (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         PeasExtension    *exten,
                                         gpointer          user_data)
{
  IdeGreeterPerspective *self = user_data;
  IdeGreeterSection *section = (IdeGreeterSection *)exten;

  IDE_ENTRY;

  g_return_if_fail (PEAS_IS_EXTENSION_SET (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_GREETER_PERSPECTIVE (self));
  g_return_if_fail (IDE_IS_GREETER_SECTION (section));

  g_signal_handlers_disconnect_by_func (section,
                                        G_CALLBACK (ide_greeter_perspective_project_activated),
                                        self);

  gtk_container_remove (GTK_CONTAINER (self->sections_container),
                        GTK_WIDGET (section));

  /* TODO: Might have to switch to empty state */

  IDE_EXIT;
}

static void
ide_greeter_perspective_constructed (GObject *object)
{
  IdeGreeterPerspective *self = (IdeGreeterPerspective *)object;

  G_OBJECT_CLASS (ide_greeter_perspective_parent_class)->constructed (object);

  ide_greeter_perspective_load_genesis_addins (self);

  self->sections = peas_extension_set_new (peas_engine_get_default (),
                                           IDE_TYPE_GREETER_SECTION,
                                           NULL);
  g_signal_connect (self->sections,
                    "extension-added",
                    G_CALLBACK (ide_greeter_perspective_section_added),
                    self);
  g_signal_connect (self->sections,
                    "extension-removed",
                    G_CALLBACK (ide_greeter_perspective_section_removed),
                    self);
  peas_extension_set_foreach (self->sections,
                              ide_greeter_perspective_section_added,
                              self);
}

static void
remove_selected_rows_cb (PeasExtensionSet *set,
                         PeasPluginInfo   *plugin_info,
                         PeasExtension    *exten,
                         gpointer          user_data)
{
  IdeGreeterSection *section = (IdeGreeterSection *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GREETER_SECTION (section));

  ide_greeter_section_delete_selected (section);
}

static void
remove_selected_rows (GSimpleAction *simple,
                      GVariant      *param,
                      gpointer       user_data)
{
  IdeGreeterPerspective *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  peas_extension_set_foreach (self->sections, remove_selected_rows_cb, NULL);
  ide_greeter_perspective_apply_filter_all (self);
  dzl_state_machine_set_state (self->state_machine, "browse");

  IDE_EXIT;
}

static void
purge_selected_rows_cb (PeasExtensionSet *set,
                        PeasPluginInfo   *plugin_info,
                        PeasExtension    *exten,
                        gpointer          user_data)
{
  IdeGreeterSection *section = (IdeGreeterSection *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_GREETER_SECTION (section));

  ide_greeter_section_purge_selected (section);
}

static void
purge_selected_rows_response (IdeGreeterPerspective *self,
                              gint                   response,
                              GtkDialog             *dialog)
{
  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_MESSAGE_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      peas_extension_set_foreach (self->sections, purge_selected_rows_cb, NULL);
      ide_greeter_perspective_apply_filter_all (self);
      dzl_state_machine_set_state (self->state_machine, "browse");
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));

  IDE_EXIT;
}

static void
purge_selected_rows (GSimpleAction *simple,
                     GVariant      *param,
                     gpointer       user_data)
{
  IdeGreeterPerspective *self = user_data;
  GtkDialog *dialog;
  GtkWidget *parent;
  GtkWidget *button;

  IDE_ENTRY;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));

  parent = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  dialog = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "modal", TRUE,
                         "transient-for", parent,
                         "attached-to", parent,
                         "text", _("Removing project files will delete them from your computer and cannot be undone."),
                         NULL);
  gtk_dialog_add_buttons (dialog,
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Delete Project Files"), GTK_RESPONSE_OK,
                          NULL);
  button = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_OK);
  dzl_gtk_widget_add_style_class (button, "destructive-action");
  g_signal_connect_data (dialog,
                         "response",
                         G_CALLBACK (purge_selected_rows_response),
                         g_object_ref (self),
                         (GClosureNotify)g_object_unref,
                         G_CONNECT_SWAPPED);
  gtk_window_present (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static void
ide_greeter_perspective_notify_selection (PeasExtensionSet *set,
                                          PeasPluginInfo   *plugin_info,
                                          PeasExtension    *exten,
                                          gpointer          user_data)
{
  IdeGreeterSection *section = (IdeGreeterSection *)exten;
  gboolean *selection = user_data;

  g_assert (IDE_IS_GREETER_SECTION (section));
  g_assert (selection != NULL);

  ide_greeter_section_set_selection_mode (section, *selection);
}

static void
ide_greeter_perspective_selection_toggled (IdeGreeterPerspective *self,
                                           GtkToggleButton       *button)
{
  gboolean selection;

  g_assert (IDE_IS_GREETER_PERSPECTIVE (self));
  g_assert (GTK_IS_TOGGLE_BUTTON (button));

  selection = gtk_toggle_button_get_active (button);

  peas_extension_set_foreach (self->sections,
                              ide_greeter_perspective_notify_selection,
                              &selection);
}

static void
ide_greeter_perspective_destroy (GtkWidget *widget)
{
  IdeGreeterPerspective *self = (IdeGreeterPerspective *)widget;

  if (self->titlebar != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->titlebar));

  g_clear_object (&self->sections);

  GTK_WIDGET_CLASS (ide_greeter_perspective_parent_class)->destroy (widget);
}

static void
ide_greeter_perspective_finalize (GObject *object)
{
  IdeGreeterPerspective *self = (IdeGreeterPerspective *)object;

  ide_clear_weak_pointer (&self->ready_binding);
  g_clear_pointer (&self->pattern_spec, dzl_pattern_spec_unref);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ide_greeter_perspective_parent_class)->finalize (object);
}

static void
ide_greeter_perspective_class_init (IdeGreeterPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_greeter_perspective_finalize;
  object_class->constructed = ide_greeter_perspective_constructed;

  widget_class->destroy = ide_greeter_perspective_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-greeter-perspective.ui");
  gtk_widget_class_set_css_name (widget_class, "greeter");
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_buttons);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_continue_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, genesis_title);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, info_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, info_bar_label);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, info_bar_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, open_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, remove_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, sections_container);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, selection_button);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, stack);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, state_machine);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, titlebar);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, top_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterPerspective, viewport);
}

static const GActionEntry actions[] = {
  { "purge-selected-rows", purge_selected_rows },
  { "remove-selected-rows", remove_selected_rows },
};

static void
ide_greeter_perspective_init (IdeGreeterPerspective *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  g_autoptr(GAction) state = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->titlebar,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->titlebar);

  g_signal_connect_object (self->selection_button,
                           "toggled",
                           G_CALLBACK (ide_greeter_perspective_selection_toggled),
                           self,
                           G_CONNECT_SWAPPED);

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

  group = g_simple_action_group_new ();
  state = dzl_state_machine_create_action (self->state_machine, "state");
  g_action_map_add_action (G_ACTION_MAP (group), state);
  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "greeter", G_ACTION_GROUP (group));

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "greeter", "remove-selected-rows",
                             "enabled", FALSE,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "greeter", "purge-selected-rows",
                             "enabled", FALSE,
                             NULL);
}
