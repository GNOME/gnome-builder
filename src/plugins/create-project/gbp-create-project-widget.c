/* gbp-create-project-widget.c
 *
 * Copyright 2016-2022 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-create-project-widget"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#include <libide-greeter.h>
#include <libide-plugins.h>
#include <libide-projects.h>
#include <libide-vcs.h>

#include "gbp-create-project-widget.h"

struct _GbpCreateProjectWidget
{
  AdwNavigationPage parent_instance;

  GtkWidget        *main;
  IdeTemplateInput *input;
  GtkImage         *directory_clash;

  AdwEntryRow      *app_id_row;
  AdwEntryRow      *language_row;
  AdwComboRow      *license_row;
  AdwEntryRow      *location_row;
  AdwEntryRow      *name_row;
  AdwEntryRow      *template_row;

  guint             loaded : 1;
};

enum {
  PROP_0,
  PROP_IS_READY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpCreateProjectWidget, gbp_create_project_widget, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties [N_PROPS];

#define ADD_ERROR(widget) gtk_widget_add_css_class(GTK_WIDGET(widget),"error")
#define REMOVE_ERROR(widget) gtk_widget_remove_css_class(GTK_WIDGET(widget),"error")

static gboolean
gbp_create_project_widget_check_ready (GbpCreateProjectWidget *self)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  return TRUE;
}

static void
template_changed_cb (GbpCreateProjectWidget *self,
                     GParamSpec             *pspec,
                     AdwComboRow            *row)
{
  IdeProjectTemplate *template;
  const char *id;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (ADW_IS_COMBO_ROW (row));

  template = adw_combo_row_get_selected_item (row);

  id = ide_project_template_get_id (template);
  ide_template_input_set_template (self->input, id);
}

static void
language_changed_cb (GbpCreateProjectWidget *self,
                     GParamSpec             *pspec,
                     AdwComboRow            *row)
{
  GtkStringObject *string;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (ADW_IS_COMBO_ROW (row));

  string = adw_combo_row_get_selected_item (row);

  ide_template_input_set_language (self->input,
                                   gtk_string_object_get_string (string));
}

static void
license_changed_cb (GbpCreateProjectWidget *self,
                    GParamSpec             *pspec,
                    AdwComboRow            *row)
{
  GtkStringObject *string;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (ADW_IS_COMBO_ROW (row));

  string = adw_combo_row_get_selected_item (row);

  ide_template_input_set_license_name (self->input,
                                       gtk_string_object_get_string (string));
}

static void
location_row_changed_cb (GbpCreateProjectWidget *self,
                         GtkEditable            *editable)
{
  g_autofree char *expanded = NULL;
  g_autoptr(GFile) directory = NULL;
  const char *text;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (GTK_IS_EDITABLE (editable));

  text = gtk_editable_get_text (editable);
  expanded = ide_path_expand (text);
  directory = g_file_new_for_path (expanded);

  ide_template_input_set_directory (self->input, directory);
}

static void
input_notify_cb (GbpCreateProjectWidget *self,
                 GParamSpec             *pspec,
                 IdeTemplateInput       *input)
{
  IdeTemplateInputValidation flags;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (IDE_IS_TEMPLATE_INPUT (input));

  if (!self->loaded)
    return;

  flags = ide_template_input_validate (input);

#define CHECK_FLAG(FLAG,widget)                         \
  G_STMT_START {                                        \
    if ((flags & IDE_TEMPLATE_INPUT_INVAL_##FLAG) != 0) \
      ADD_ERROR(widget);                                \
    else                                                \
      REMOVE_ERROR(widget);                             \
  } G_STMT_END

  CHECK_FLAG (APP_ID, self->app_id_row);
  CHECK_FLAG (LANGUAGE, self->language_row);
  CHECK_FLAG (LOCATION, self->location_row);
  CHECK_FLAG (NAME, self->name_row);
  CHECK_FLAG (TEMPLATE, self->template_row);

#undef CHECK_FLAG

  if ((flags & IDE_TEMPLATE_INPUT_INVAL_LOCATION) &&
      !(flags & IDE_TEMPLATE_INPUT_INVAL_NAME))
    {
      ADD_ERROR (self->name_row);
      gtk_widget_show (GTK_WIDGET (self->directory_clash));
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self->directory_clash));
    }

  gtk_widget_action_set_enabled (GTK_WIDGET (self),
                                 "create-project.expand",
                                 flags == IDE_TEMPLATE_INPUT_VALID);
}

static void
select_folder_response_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GtkFileDialog *dialog = (GtkFileDialog *)object;
  g_autoptr(GbpCreateProjectWidget) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *path = NULL;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  if (!(file = gtk_file_dialog_select_folder_finish (dialog, result, &error)))
    return;

  path = ide_path_collapse (g_file_peek_path (file));
  gtk_editable_set_text (GTK_EDITABLE (self->location_row), path);
}

static void
select_folder_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  GbpCreateProjectWidget *self = (GbpCreateProjectWidget *)widget;
  g_autoptr(GtkFileDialog) dialog = NULL;
  GtkRoot *root;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  root = gtk_widget_get_root (widget);
  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Select Location"));
  gtk_file_dialog_set_accept_label (dialog, _("Select"));
  gtk_file_dialog_set_initial_folder (dialog, ide_template_input_get_directory (self->input));

  gtk_file_dialog_select_folder (dialog,
                                 GTK_WINDOW (root),
                                 NULL,
                                 select_folder_response_cb,
                                 g_object_ref (self));
}

static void
gbp_create_project_widget_expand_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeTemplateInput *input = (IdeTemplateInput *)object;
  g_autoptr(IdeGreeterWorkspace) greeter = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) directory = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TEMPLATE_INPUT (input));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_GREETER_WORKSPACE (greeter));

  if (!(directory = ide_template_input_expand_finish (input, result, &error)))
    {
      /* Make sure it wasn't closed/cancelled */
      if (gtk_widget_get_visible (GTK_WIDGET (greeter)))
        {
          AdwDialog *dialog;

          dialog = adw_alert_dialog_new (_("Failed to Create Project"),
                                         error->message);
          adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("_Close"));
          adw_dialog_present (dialog, GTK_WIDGET (greeter));
        }
    }
  else
    {
      g_autoptr(IdeProjectInfo) project_info = NULL;

      project_info = ide_project_info_new ();
      ide_project_info_set_file (project_info, directory);
      ide_project_info_set_directory (project_info, directory);

      ide_greeter_workspace_open_project (greeter, project_info);
    }

  ide_greeter_workspace_end (greeter);

  IDE_EXIT;
}

static void
expand_action (GtkWidget  *widget,
               const char *action_name,
               GVariant   *param)
{
  GbpCreateProjectWidget *self = (GbpCreateProjectWidget *)widget;
  IdeGreeterWorkspace *greeter;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  greeter = IDE_GREETER_WORKSPACE (ide_widget_get_workspace (widget));
  context = ide_workspace_get_context (IDE_WORKSPACE (greeter));

  ide_greeter_workspace_begin (greeter);

  gtk_widget_action_set_enabled (widget, "create-project.expand", FALSE);
  ide_template_input_expand_async (self->input,
                                   context,
                                   ide_workspace_get_cancellable (IDE_WORKSPACE (greeter)),
                                   gbp_create_project_widget_expand_cb,
                                   g_object_ref (greeter));

  IDE_EXIT;
}

static void
text_activated_cb (GbpCreateProjectWidget *self,
                   gpointer                userdata)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  gtk_widget_activate_action (GTK_WIDGET (self), "create-project.expand", NULL);
}

static guint
find_license (GbpCreateProjectWidget *self,
              const char             *license)
{
  GListModel *model;
  guint n_items;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (license != NULL);

  model = ide_template_input_get_licenses_model (self->input);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GtkStringObject) strobj = g_list_model_get_item (model, i);
      const char *str = gtk_string_object_get_string (strobj);

      if (ide_str_equal0 (str, license))
        return i;
    }

  return 0;
}

static void
gbp_create_project_widget_dispose (GObject *object)
{
  GbpCreateProjectWidget *self = (GbpCreateProjectWidget *)object;

  g_clear_pointer (&self->main, gtk_widget_unparent);

  G_OBJECT_CLASS (gbp_create_project_widget_parent_class)->dispose (object);
}

static void
gbp_create_project_widget_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpCreateProjectWidget *self = GBP_CREATE_PROJECT_WIDGET (object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      g_value_set_boolean (value, gbp_create_project_widget_check_ready (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_create_project_widget_class_init (GbpCreateProjectWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_create_project_widget_dispose;
  object_class->get_property = gbp_create_project_widget_get_property;

  properties [PROP_IS_READY] =
    g_param_spec_boolean ("is-ready", NULL, NULL, FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/create-project/gbp-create-project-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, app_id_row);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, directory_clash);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, input);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, language_row);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, license_row);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, location_row);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, main);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, name_row);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, template_row);

  gtk_widget_class_bind_template_callback (widget_class, template_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, language_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, text_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, license_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, location_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, input_notify_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_install_action (widget_class, "create-project.select-folder", NULL, select_folder_action);
  gtk_widget_class_install_action (widget_class, "create-project.expand", NULL, expand_action);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Return, GDK_CONTROL_MASK, "create-project.expand", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_ISO_Enter, GDK_CONTROL_MASK, "create-project.expand", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Enter, GDK_CONTROL_MASK, "create-project.expand", NULL);

  g_type_ensure (IDE_TYPE_TEMPLATE_INPUT);
}

static void
gbp_create_project_widget_init (GbpCreateProjectWidget *self)
{
  g_autofree char *projects_dir = ide_path_collapse (ide_get_projects_dir ());
  g_autoptr(GSettings) settings = g_settings_new ("org.gnome.builder");
  g_autofree char *default_license = g_settings_get_string (settings, "default-license");

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_editable_set_text (GTK_EDITABLE (self->location_row), projects_dir);
  adw_combo_row_set_selected (self->license_row, find_license (self, default_license));

  /* Always start disabled */
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "create-project.expand", FALSE);

  self->loaded = TRUE;
}
