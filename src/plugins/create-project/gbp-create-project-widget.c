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
  GtkWidget         parent_instance;

  GtkWidget        *main;
  IdeTemplateInput *input;
  AdwEntryRow      *location;
  GtkMenuButton    *template_button;
};

enum {
  PROP_0,
  PROP_IS_READY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpCreateProjectWidget, gbp_create_project_widget, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static gboolean
gbp_create_project_widget_check_ready (GbpCreateProjectWidget *self)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  return TRUE;
}

static void
template_activated_cb (GbpCreateProjectWidget *self,
                       guint                   position,
                       GtkListView            *list_view)
{
  g_autoptr(IdeProjectTemplate) template = NULL;
  g_autofree char *id = NULL;
  GListModel *model;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  model = G_LIST_MODEL (gtk_list_view_get_model (list_view));
  template = g_list_model_get_item (model, position);

  gtk_menu_button_popdown (self->template_button);

  id = ide_project_template_get_id (template);
  ide_template_input_set_template (self->input, id);
}

static void
on_location_changed_cb (GbpCreateProjectWidget *self,
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
select_folder_response_cb (GbpCreateProjectWidget *self,
                           int                     response_id,
                           GtkFileChooserNative   *native)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (GTK_IS_FILE_CHOOSER_NATIVE (native));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));
      g_autofree char *path = ide_path_collapse (g_file_peek_path (file));

      gtk_editable_set_text (GTK_EDITABLE (self->location), path);
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
}

static void
select_folder_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  GbpCreateProjectWidget *self = (GbpCreateProjectWidget *)widget;
  GtkFileChooserNative *native;
  GtkRoot *root;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  root = gtk_widget_get_root (widget);
  native = gtk_file_chooser_native_new (_("Select Location"),
                                        GTK_WINDOW (root),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        _("Select"),
                                        _("Cancel"));
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (native),
                                       ide_template_input_get_directory (self->input),
                                       NULL);
  g_signal_connect_object (native,
                           "response",
                           G_CALLBACK (select_folder_response_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
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

  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, input);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, location);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, main);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, template_button);

  gtk_widget_class_bind_template_callback (widget_class, template_activated_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_install_action (widget_class, "create-project.select-folder", NULL, select_folder_action);

  g_type_ensure (IDE_TYPE_TEMPLATE_INPUT);
}

static void
gbp_create_project_widget_init (GbpCreateProjectWidget *self)
{
  g_autofree char *projects_dir = ide_path_collapse (ide_get_projects_dir ());

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->location,
                           "changed",
                           G_CALLBACK (on_location_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_editable_set_text (GTK_EDITABLE (self->location), projects_dir);
}
