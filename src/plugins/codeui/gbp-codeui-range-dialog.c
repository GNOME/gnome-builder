/* gbp-codeui-range-dialog.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-codeui-range-dialog"

#include "config.h"

#include <glib/gi18n.h>

#include <libpanel.h>

#include <libide-editor.h>
#include <libide-code.h>

#include "gbp-codeui-range-dialog.h"

struct _GbpCodeuiRangeDialog
{
  AdwAlertDialog parent_instance;
  GtkListBox *list_box;
  AdwActionRow *loading;
  AdwPreferencesGroup *group;
  guint count;
};

G_DEFINE_FINAL_TYPE (GbpCodeuiRangeDialog, gbp_codeui_range_dialog, ADW_TYPE_ALERT_DIALOG)

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static GtkWidget *
create_widget_cb (gpointer item,
                  gpointer user_data)
{
  GbpCodeuiRangeDialog *self = user_data;
  IdeRange *range = item;
  IdeLocation *begin = ide_range_get_begin (range);
  GFile *file = ide_location_get_file (begin);
  g_autofree char *name = g_file_get_basename (file);
  g_autoptr(GFile) parent = g_file_get_parent (file);
  g_autofree char *dir = g_file_is_native (parent) ? g_file_get_path (parent) : g_file_get_uri (parent);
  guint line = ide_location_get_line (begin) + 1;
  guint line_offset = ide_location_get_line_offset (begin) + 1;
  g_autofree char *title = g_strdup_printf ("%s:%u:%u", name, line, line_offset);
  GtkWidget *image = gtk_image_new_from_icon_name ("go-next-symbolic");
  AdwActionRow *row;

  self->count++;

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      "title", title,
                      "subtitle", dir,
                      NULL);

  g_object_set_data_full (G_OBJECT (row),
                          "IDE_LOCATION",
                          g_object_ref (begin),
                          g_object_unref);

  adw_action_row_add_suffix (row, image);

  gtk_widget_show (GTK_WIDGET (self->list_box));

  return GTK_WIDGET (row);
}

static void
gbp_codeui_range_dialog_activate_row_cb (GbpCodeuiRangeDialog *self,
                                         GtkListBoxRow        *row,
                                         GtkListBox           *list_box)
{
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkspace *workspace;
  IdeLocation *location;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_RANGE_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  location = g_object_get_data (G_OBJECT (row), "IDE_LOCATION");
  position = panel_position_new ();

  ide_editor_focus_location (workspace, position, location);
}

static void
gbp_codeui_range_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbpCodeuiRangeDialog *self = GBP_CODEUI_RANGE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_list_box_bind_model (self->list_box,
                               g_value_get_object (value),
                               create_widget_cb, self, NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_codeui_range_dialog_class_init (GbpCodeuiRangeDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gbp_codeui_range_dialog_set_property;

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/codeui/gbp-codeui-range-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiRangeDialog, group);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiRangeDialog, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiRangeDialog, loading);
  gtk_widget_class_bind_template_callback (widget_class, gbp_codeui_range_dialog_activate_row_cb);
}

static void
gbp_codeui_range_dialog_init (GbpCodeuiRangeDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
gbp_codeui_range_dialog_done (GbpCodeuiRangeDialog *self)
{
  g_return_if_fail (GBP_IS_CODEUI_RANGE_DIALOG (self));

  if (self->count == 0)
    {
      g_object_set (self->loading,
                    "title", _("No references found"),
                    "subtitle", _("The programming language tooling may not support finding references"),
                    NULL);
      gtk_widget_hide (GTK_WIDGET (self->list_box));
      return;
    }

  adw_preferences_group_remove (self->group, GTK_WIDGET (self->loading));
  self->loading = NULL;
}
