/* ide-greeter-project-row.c
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

#define G_LOG_DOMAIN "ide-greeter-project-row"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "greeter/ide-greeter-project-row.h"
#include "ide-macros.h"

struct _IdeGreeterProjectRow
{
  GtkListBoxRow    parent_instance;

  IdeProjectInfo  *project_info;
  DzlBindingGroup *bindings;
  gchar           *search_text;

  GtkLabel        *date_label;
  GtkLabel        *description_label;
  GtkBox          *tags_box;
  GtkLabel        *location_label;
  GtkLabel        *title_label;
  GtkCheckButton  *checkbox;
};

G_DEFINE_TYPE (IdeGreeterProjectRow, ide_greeter_project_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_PROJECT_INFO,
  PROP_SELECTED,
  PROP_SELECTION_MODE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];
static GFile      *home_dir;

void
ide_greeter_project_row_set_selection_mode (IdeGreeterProjectRow *self,
                                            gboolean              selection_mode)
{
  g_return_if_fail (IDE_IS_GREETER_PROJECT_ROW (self));

  gtk_widget_set_visible (GTK_WIDGET (self->checkbox), selection_mode);
}

IdeProjectInfo *
ide_greeter_project_row_get_project_info (IdeGreeterProjectRow *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_PROJECT_ROW (self), NULL);

  return self->project_info;
}

static void
ide_greeter_project_row_create_search_text (IdeGreeterProjectRow *self,
                                            IdeProjectInfo       *project_info)
{
  const gchar *tmp;
  IdeDoap *doap;
  GString *str;
  GFile *file;

  g_assert (IDE_IS_GREETER_PROJECT_ROW (self));

  str = g_string_new (NULL);

  if ((tmp = ide_project_info_get_name (project_info)))
    {
      g_autofree gchar *downcase = g_utf8_strdown (tmp, -1);

      g_string_append (str, tmp);
      g_string_append (str, " ");
      g_string_append (str, downcase);
      g_string_append (str, " ");
    }

  if ((tmp = ide_project_info_get_description (project_info)))
    {
      g_string_append (str, tmp);
      g_string_append (str, " ");
    }

  doap = ide_project_info_get_doap (project_info);

  if (doap != NULL)
    {
      if ((tmp = ide_doap_get_description (doap)))
        {
          g_string_append (str, tmp);
          g_string_append (str, " ");
        }
    }

  file = ide_project_info_get_file (project_info);

  if (file != NULL)
    {
      g_autoptr(GFile) parent = g_file_get_parent (file);
      g_autofree gchar *dir = parent ? g_file_get_basename (parent) : NULL;
      g_autofree gchar *base = g_file_get_basename (file);

      if (dir != NULL)
        {
          g_string_append (str, dir);
          g_string_append (str, " ");
        }

      if (base != NULL)
        {
          g_string_append (str, base);
          g_string_append (str, " ");
        }
    }

  g_free (self->search_text);
  self->search_text = g_strdelimit (g_string_free (str, FALSE), "\n", ' ');
}

static void
ide_greeter_project_row_add_tags (IdeGreeterProjectRow *self,
                                  IdeProjectInfo       *project_info)
{
  const gchar * const *languages;
  const gchar *build_system_name;

  g_return_if_fail (IDE_IS_GREETER_PROJECT_ROW (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));

  languages = ide_project_info_get_languages (project_info);

  if (languages != NULL)
    {
      guint len = g_strv_length ((gchar **)languages);
      gsize i;

      for (i = len; i > 0; i--)
        {
          const gchar *name = languages [i - 1];
          GtkWidget *pill;

          pill = g_object_new (DZL_TYPE_PILL_BOX,
                               "visible", TRUE,
                               "label", name,
                               NULL);
          gtk_container_add (GTK_CONTAINER (self->tags_box), pill);
        }
    }

  build_system_name = ide_project_info_get_build_system_name (project_info);
  if (!ide_str_empty0 (build_system_name))
    {
      GtkWidget *pill;

      pill = g_object_new (DZL_TYPE_PILL_BOX,
                           "visible", TRUE,
                           "label", build_system_name,
                           NULL);
      gtk_container_add (GTK_CONTAINER (self->tags_box), pill);
    }
}

static void
ide_greeter_project_row_set_project_info (IdeGreeterProjectRow *self,
                                          IdeProjectInfo       *project_info)
{
  g_return_if_fail (IDE_IS_GREETER_PROJECT_ROW (self));
  g_return_if_fail (!project_info || IDE_IS_PROJECT_INFO (project_info));

  if (g_set_object (&self->project_info, project_info))
    {
      dzl_binding_group_set_source (self->bindings, project_info);

      if (project_info != NULL)
        {
          ide_greeter_project_row_add_tags (self, project_info);
          ide_greeter_project_row_create_search_text (self, project_info);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT_INFO]);
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

  str = dzl_g_date_time_format_for_display (dt);
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

      if ((relative_path = g_file_get_relative_path (home_dir, file)) ||
          (relative_path = g_file_get_path (file)))
        {
          g_value_take_string (to_value, relative_path);
          return TRUE;
        }

      g_free (relative_path);
    }

  uri = g_file_get_uri (file);
  g_value_set_string (to_value, uri);

  return TRUE;
}

const gchar *
ide_greeter_project_row_get_search_text (IdeGreeterProjectRow *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_PROJECT_ROW (self), NULL);

  return self->search_text;
}

static void
ide_greeter_project_row_finalize (GObject *object)
{
  IdeGreeterProjectRow *self = (IdeGreeterProjectRow *)object;

  g_clear_object (&self->project_info);
  g_clear_object (&self->bindings);
  g_clear_pointer (&self->search_text, g_free);

  G_OBJECT_CLASS (ide_greeter_project_row_parent_class)->finalize (object);
}

static void
ide_greeter_project_row_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeGreeterProjectRow *self = IDE_GREETER_PROJECT_ROW (object);

  switch (prop_id)
    {
    case PROP_PROJECT_INFO:
      g_value_set_object (value, ide_greeter_project_row_get_project_info (self));
      break;

    case PROP_SELECTED:
      g_object_get_property (G_OBJECT (self->checkbox), "active", value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_greeter_project_row_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeGreeterProjectRow *self = IDE_GREETER_PROJECT_ROW (object);

  switch (prop_id)
    {
    case PROP_SELECTED:
      g_object_set_property (G_OBJECT (self->checkbox), "active", value);
      break;

    case PROP_SELECTION_MODE:
      ide_greeter_project_row_set_selection_mode (self, g_value_get_boolean (value));
      break;

    case PROP_PROJECT_INFO:
      ide_greeter_project_row_set_project_info (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_greeter_project_row_class_init (IdeGreeterProjectRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_greeter_project_row_finalize;
  object_class->get_property = ide_greeter_project_row_get_property;
  object_class->set_property = ide_greeter_project_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-greeter-project-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterProjectRow, checkbox);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterProjectRow, date_label);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterProjectRow, description_label);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterProjectRow, location_label);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterProjectRow, tags_box);
  gtk_widget_class_bind_template_child (widget_class, IdeGreeterProjectRow, title_label);

  properties [PROP_SELECTED] =
    g_param_spec_boolean ("selected",
                          "Selected",
                          "Selected",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SELECTION_MODE] =
    g_param_spec_boolean ("selection-mode",
                          "Selection Mode",
                          "Selection Mode",
                          FALSE,
                          (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROJECT_INFO] =
    g_param_spec_object ("project-info",
                         "Project Information",
                         "The project information to render.",
                         IDE_TYPE_PROJECT_INFO,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  home_dir = g_file_new_for_path (g_get_home_dir ());
}

static void
ide_greeter_project_row_init (IdeGreeterProjectRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->bindings, "name", self->title_label, "label", 0);
  dzl_binding_group_bind_full (self->bindings, "last-modified-at", self->date_label, "label", 0,
                               humanize_date_time, NULL, NULL, NULL);
  dzl_binding_group_bind_full (self->bindings, "directory", self->location_label, "label", 0,
                               truncate_location, NULL, NULL, NULL);
  dzl_binding_group_bind (self->bindings, "description", self->description_label, "label", 0);

  g_object_bind_property (self->checkbox, "active", self, "selected", 0);
}
