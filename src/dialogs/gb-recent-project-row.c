/* gb-recent-project-row.c
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

#include <glib/gi18n.h>

#include "gb-glib.h"
#include "gb-recent-project-row.h"
#include "gb-widget.h"

struct _GbRecentProjectRow
{
  GtkListBoxRow   parent_instance;

  IdeProjectInfo *project_info;

  GtkCheckButton *check_button;
  GtkImage       *image;
  GtkLabel       *date_label;
  GtkLabel       *location_label;
  GtkLabel       *name_label;
  GtkRevealer    *revealer;
};

struct _GbRecentProjectRowClass
{
  GtkListBoxRowClass parent_class;
};

enum
{
  PROP_0,
  PROP_PROJECT_INFO,
  PROP_SELECTED,
  PROP_SELECTION_MODE,
  LAST_PROP
};

G_DEFINE_TYPE (GbRecentProjectRow, gb_recent_project_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_recent_project_row_set_selection_mode (GbRecentProjectRow *self,
                                          gboolean            selection_mode)
{
  g_return_if_fail (GB_IS_RECENT_PROJECT_ROW (self));

  if (selection_mode != gtk_revealer_get_reveal_child (self->revealer))
    gtk_revealer_set_reveal_child (self->revealer, selection_mode);
}

static gboolean
gb_recent_project_row_get_selection_mode (GbRecentProjectRow *self)
{
  g_return_val_if_fail (GB_IS_RECENT_PROJECT_ROW (self), FALSE);

  return gtk_revealer_get_reveal_child (self->revealer);
}

static void
gb_recent_project_row_set_project_info (GbRecentProjectRow *self,
                                        IdeProjectInfo     *project_info)
{
  g_assert (GB_IS_RECENT_PROJECT_ROW (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  if (g_set_object (&self->project_info, project_info))
    {
      if (project_info != NULL)
        {
          g_autofree gchar *relpath = NULL;
          g_autofree gchar *datestr = NULL;
          g_autoptr(GFile) home = NULL;
          GDateTime *last_modified_at;
          const gchar *name;
          GFile *directory;

          name = ide_project_info_get_name (project_info);
          directory = ide_project_info_get_directory (project_info);
          last_modified_at = ide_project_info_get_last_modified_at (project_info);

          home = g_file_new_for_path (g_get_home_dir ());
          relpath = g_file_get_relative_path (home, directory);
          if (relpath == NULL)
            relpath = g_file_get_path (directory);

          if (!g_file_is_native (directory))
            {
              gtk_image_set_from_icon_name (self->image, "folder-remote", GTK_ICON_SIZE_DIALOG);
              gtk_image_set_pixel_size (self->image, 64);
            }

          datestr = gb_date_time_format_for_display (last_modified_at);

          gtk_label_set_label (self->name_label, name);
          gtk_label_set_label (self->location_label, relpath);
          gtk_label_set_label (self->date_label, datestr);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_PROJECT_INFO]);
    }
}

static void
gb_recent_project_row__check_button_toggled (GbRecentProjectRow *self,
                                             GtkToggleButton    *toggle_button)
{
  g_assert (GB_IS_RECENT_PROJECT_ROW (self));
  g_assert (GTK_IS_TOGGLE_BUTTON (toggle_button));

  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SELECTED]);
}

static void
gb_recent_project_row_finalize (GObject *object)
{
  GbRecentProjectRow *self = (GbRecentProjectRow *)object;

  g_clear_object (&self->project_info);

  G_OBJECT_CLASS (gb_recent_project_row_parent_class)->finalize (object);
}

static void
gb_recent_project_row_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbRecentProjectRow *self = GB_RECENT_PROJECT_ROW (object);

  switch (prop_id)
    {
    case PROP_PROJECT_INFO:
      g_value_set_object (value, gb_recent_project_row_get_project_info (self));
      break;

    case PROP_SELECTED:
      g_value_set_boolean (value, gb_recent_project_row_get_selected (self));
      break;

    case PROP_SELECTION_MODE:
      g_value_set_boolean (value, gb_recent_project_row_get_selection_mode (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_recent_project_row_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbRecentProjectRow *self = GB_RECENT_PROJECT_ROW (object);

  switch (prop_id)
    {
    case PROP_PROJECT_INFO:
      gb_recent_project_row_set_project_info (self, g_value_get_object (value));
      break;

    case PROP_SELECTED:
      gb_recent_project_row_set_selected (self, g_value_get_boolean (value));
      break;

    case PROP_SELECTION_MODE:
      gb_recent_project_row_set_selection_mode (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_recent_project_row_class_init (GbRecentProjectRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_recent_project_row_finalize;
  object_class->get_property = gb_recent_project_row_get_property;
  object_class->set_property = gb_recent_project_row_set_property;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-recent-project-row.ui");
  GB_WIDGET_CLASS_BIND (klass, GbRecentProjectRow, check_button);
  GB_WIDGET_CLASS_BIND (klass, GbRecentProjectRow, date_label);
  GB_WIDGET_CLASS_BIND (klass, GbRecentProjectRow, image);
  GB_WIDGET_CLASS_BIND (klass, GbRecentProjectRow, location_label);
  GB_WIDGET_CLASS_BIND (klass, GbRecentProjectRow, name_label);
  GB_WIDGET_CLASS_BIND (klass, GbRecentProjectRow, revealer);

  gParamSpecs [PROP_PROJECT_INFO] =
    g_param_spec_object ("project-info",
                         _("Project Info"),
                         _("The project info for the row."),
                         IDE_TYPE_PROJECT_INFO,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PROJECT_INFO,
                                   gParamSpecs [PROP_PROJECT_INFO]);

  gParamSpecs [PROP_SELECTED] =
    g_param_spec_boolean ("selected",
                          _("Selected"),
                          _("Selected"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SELECTED, gParamSpecs [PROP_SELECTED]);

  gParamSpecs [PROP_SELECTION_MODE] =
    g_param_spec_boolean ("selection-mode",
                          _("Selection Mode"),
                          _("Selection Mode"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SELECTION_MODE,
                                   gParamSpecs [PROP_SELECTION_MODE]);
}

static void
gb_recent_project_row_init (GbRecentProjectRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->check_button,
                           "toggled",
                           G_CALLBACK (gb_recent_project_row__check_button_toggled),
                           self,
                           G_CONNECT_SWAPPED);
}

IdeProjectInfo *
gb_recent_project_row_get_project_info (GbRecentProjectRow *self)
{
  g_return_val_if_fail (GB_IS_RECENT_PROJECT_ROW (self), NULL);

  return self->project_info;
}

GtkWidget *
gb_recent_project_row_new (IdeProjectInfo *project_info)
{
  return g_object_new (IDE_TYPE_PROJECT_INFO,
                       "project-info", project_info,
                       NULL);
}

gboolean
gb_recent_project_row_get_selected (GbRecentProjectRow *self)
{
  g_return_val_if_fail (GB_IS_RECENT_PROJECT_ROW (self), FALSE);

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check_button));
}

void
gb_recent_project_row_set_selected (GbRecentProjectRow *self,
                                    gboolean            selected)
{
  g_return_if_fail (GB_IS_RECENT_PROJECT_ROW (self));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->check_button), selected);
}
