/* gb-greeter-project-row.c
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

#define G_LOG_DOMAIN "gb-greeter-project-row"

#include <glib/gi18n.h>
#include <ide.h>

#include "egg-binding-set.h"

#include "gb-glib.h"
#include "gb-greeter-project-row.h"
#include "gb-greeter-pill-box.h"

struct _GbGreeterProjectRow
{
  GtkListBoxRow   parent_instance;

  IdeProjectInfo *project_info;
  EggBindingSet  *bindings;
  GFile          *home_dir;

  GtkLabel       *date_label;
  GtkLabel       *description_label;
  GtkBox         *languages_box;
  GtkLabel       *location_label;
  GtkLabel       *title_label;
};

G_DEFINE_TYPE (GbGreeterProjectRow, gb_greeter_project_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_PROJECT_INFO,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];
static GFile      *gHomeDir;

IdeProjectInfo *
gb_greeter_project_row_get_project_info (GbGreeterProjectRow *self)
{
  g_return_val_if_fail (GB_IS_GREETER_PROJECT_ROW (self), NULL);

  return self->project_info;
}

static void
gb_greeter_project_row_add_languages (GbGreeterProjectRow *self,
                                      IdeProjectInfo      *project_info)
{
  IdeDoap *doap;
  gchar **languages;

  g_return_if_fail (GB_IS_GREETER_PROJECT_ROW (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));

  /*
   * TODO: What should we do about doap?
   */

  if ((doap = ide_project_info_get_doap (project_info)) &&
      (languages = ide_doap_get_languages (doap)))
    {
      gsize i;

      for (i = 0; languages [i]; i++)
        {
          GtkWidget *pill;

          pill = g_object_new (GB_TYPE_GREETER_PILL_BOX,
                               "visible", TRUE,
                               "label", languages [i],
                               NULL);
          gtk_container_add (GTK_CONTAINER (self->languages_box), pill);
        }
    }
}

static void
gb_greeter_project_row_set_project_info (GbGreeterProjectRow *self,
                                         IdeProjectInfo      *project_info)
{
  g_return_if_fail (GB_IS_GREETER_PROJECT_ROW (self));
  g_return_if_fail (!project_info || IDE_IS_PROJECT_INFO (project_info));

  if (g_set_object (&self->project_info, project_info))
    {
      egg_binding_set_set_source (self->bindings, project_info);
      if (project_info != NULL)
        gb_greeter_project_row_add_languages (self, project_info);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_PROJECT_INFO]);
    }
}

static gboolean
humanize_date_time (GBinding     *binding,
                    const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  GDateTime *dt;
  gchar *str;

  g_assert (G_VALUE_HOLDS (from_value, G_TYPE_DATE_TIME));
  g_assert (G_VALUE_HOLDS (to_value, G_TYPE_STRING));

  if (!(dt = g_value_get_boxed (from_value)))
    return FALSE;

  str = gb_date_time_format_for_display (dt);
  g_value_take_string (to_value, str);

  return TRUE;
}

static gboolean
truncate_location (GBinding     *binding,
                   const GValue *from_value,
                   GValue       *to_value,
                   gpointer      user_data)
{
  GFile *file;
  gchar *uri;

  g_assert (G_VALUE_HOLDS (from_value, G_TYPE_FILE));
  g_assert (G_VALUE_HOLDS (to_value, G_TYPE_STRING));

  if (!(file = g_value_get_object (from_value)))
    return FALSE;

  if (g_file_is_native (file))
    {
      gchar *relative_path;

      if ((relative_path = g_file_get_relative_path (gHomeDir, file)) ||
          (relative_path = g_file_get_path (file)))
        {
          g_value_set_string (to_value, relative_path);
          return TRUE;
        }
    }

  uri = g_file_get_uri (file);
  g_value_set_string (to_value, uri);

  return TRUE;
}

static void
gb_greeter_project_row_finalize (GObject *object)
{
  GbGreeterProjectRow *self = (GbGreeterProjectRow *)object;

  g_clear_object (&self->project_info);
  g_clear_object (&self->bindings);
  g_clear_object (&self->home_dir);

  G_OBJECT_CLASS (gb_greeter_project_row_parent_class)->finalize (object);
}

static void
gb_greeter_project_row_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbGreeterProjectRow *self = GB_GREETER_PROJECT_ROW (object);

  switch (prop_id)
    {
    case PROP_PROJECT_INFO:
      g_value_set_object (value, gb_greeter_project_row_get_project_info (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_greeter_project_row_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbGreeterProjectRow *self = GB_GREETER_PROJECT_ROW (object);

  switch (prop_id)
    {
    case PROP_PROJECT_INFO:
      gb_greeter_project_row_set_project_info (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_greeter_project_row_class_init (GbGreeterProjectRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_greeter_project_row_finalize;
  object_class->get_property = gb_greeter_project_row_get_property;
  object_class->set_property = gb_greeter_project_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-greeter-project-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbGreeterProjectRow, date_label);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterProjectRow, description_label);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterProjectRow, location_label);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterProjectRow, languages_box);
  gtk_widget_class_bind_template_child (widget_class, GbGreeterProjectRow, title_label);

  gParamSpecs [PROP_PROJECT_INFO] =
    g_param_spec_object ("project-info",
                         _("Project Info"),
                         _("The project info to render."),
                         IDE_TYPE_PROJECT_INFO,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PROJECT_INFO,
                                   gParamSpecs [PROP_PROJECT_INFO]);

  gHomeDir = g_file_new_for_path (g_get_home_dir ());
}

static void
gb_greeter_project_row_init (GbGreeterProjectRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->bindings = egg_binding_set_new ();

  egg_binding_set_bind (self->bindings, "name", self->title_label, "label", 0);
  egg_binding_set_bind_full (self->bindings, "last-modified-at", self->date_label, "label", 0,
                             humanize_date_time, NULL, NULL, NULL);
  egg_binding_set_bind_full (self->bindings, "directory", self->location_label, "label", 0,
                             truncate_location, NULL, NULL, NULL);
  egg_binding_set_bind (self->bindings, "description", self->description_label, "label", 0);
}
