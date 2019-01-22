/* ide-greeter-row.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-greeter-row"

#include "config.h"

#include <glib/gi18n.h>
#include <stdlib.h>

#include "ide-greeter-row.h"

typedef struct
{
  IdeProjectInfo *project_info;

  /* Template Widgets */
  GtkCheckButton *check_button;
  GtkLabel       *title;
  GtkLabel       *subtitle;
  GtkImage       *image;
  GtkBox         *tags;
} IdeGreeterRowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeGreeterRow, ide_greeter_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_PROJECT_INFO,
  PROP_SELECTED,
  N_PROPS
};

enum {
  TAG_NONE,
  TAG_BUILD_SYSTEM,
  TAG_LANGUAGE,
};

static GParamSpec *properties [N_PROPS];
static GHashTable *icon_name_map;

/**
 * ide_greeter_row_new:
 *
 * Create a new #IdeGreeterRow.
 *
 * Returns: (transfer full): a newly created #IdeGreeterRow
 */
IdeGreeterRow *
ide_greeter_row_new (void)
{
  return g_object_new (IDE_TYPE_GREETER_ROW, NULL);
}

static void
ide_greeter_row_get_preferred_width (GtkWidget *widget,
                                     gint      *min_width,
                                     gint      *nat_width)
{
  *min_width = 600;
  *nat_width = 600;
}

static void
ide_greeter_row_finalize (GObject *object)
{
  IdeGreeterRow *self = (IdeGreeterRow *)object;
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);

  g_clear_object (&priv->project_info);

  G_OBJECT_CLASS (ide_greeter_row_parent_class)->finalize (object);
}

static void
ide_greeter_row_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeGreeterRow *self = IDE_GREETER_ROW (object);
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PROJECT_INFO:
      g_value_set_object (value, ide_greeter_row_get_project_info (self));
      break;

    case PROP_SELECTED:
      g_value_set_boolean (value,
                           gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->check_button)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_greeter_row_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeGreeterRow *self = IDE_GREETER_ROW (object);
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PROJECT_INFO:
      ide_greeter_row_set_project_info (self, g_value_get_object (value));
      break;

    case PROP_SELECTED:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->check_button),
                                    g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_greeter_row_class_init (IdeGreeterRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_greeter_row_finalize;
  object_class->get_property = ide_greeter_row_get_property;
  object_class->set_property = ide_greeter_row_set_property;

  widget_class->get_preferred_width = ide_greeter_row_get_preferred_width;

  /**
   * IdeGreeterRow:project-info:
   *
   * The "project-info" property contains information about the project
   * to be displayed.
   *
   * Since: 3.32
   */
  properties [PROP_PROJECT_INFO] =
    g_param_spec_object ("project-info",
                         "Project Info",
                         "Project Info",
                         IDE_TYPE_PROJECT_INFO,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SELECTED] =
    g_param_spec_boolean ("selected",
                          "Selected",
                          "If the row is selected",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-greeter-row.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeGreeterRow, check_button);
  gtk_widget_class_bind_template_child_private (widget_class, IdeGreeterRow, image);
  gtk_widget_class_bind_template_child_private (widget_class, IdeGreeterRow, subtitle);
  gtk_widget_class_bind_template_child_private (widget_class, IdeGreeterRow, tags);
  gtk_widget_class_bind_template_child_private (widget_class, IdeGreeterRow, title);

#define ADD(a,b) g_hash_table_insert(icon_name_map, (gpointer)a, (gpointer)b)
  icon_name_map = g_hash_table_new (g_str_hash, g_str_equal);
  ADD ("python", "text-x-python-symbolic");
  ADD ("c", "text-x-csrc-symbolic");
  ADD ("c++", "text-x-cpp-symbolic");
  ADD ("css", "text-x-css-symbolic");
  ADD ("html", "text-x-html-symbolic");
  ADD ("ruby", "text-x-ruby-symbolic");
  ADD ("rust", "text-x-rust-symbolic");
  ADD ("javascript", "text-x-javascript-symbolic");
  ADD ("vala", "text-x-vala-symbolic");
  ADD ("xml", "text-x-xml-symbolic");
#undef ADD
}

static void
ide_greeter_row_init (IdeGreeterRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ide_greeter_row_clear (IdeGreeterRow *self)
{
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);

  g_assert (IDE_IS_GREETER_ROW (self));

  g_object_set (priv->image, "icon-name", NULL, NULL);
  gtk_label_set_label (priv->title, NULL);
  gtk_label_set_label (priv->subtitle, NULL);
  gtk_container_foreach (GTK_CONTAINER (priv->tags),
                         (GtkCallback)gtk_widget_destroy,
                         NULL);
}

/**
 * ide_greeter_row_get_project_info:
 * @self: an #IdeGreeterRow
 *
 * Gets the #IdeGreeterRow:project-info property.
 *
 * Returns: (transfer none) (nullable): an #IdeProjectInfo or %NULL
 *
 * Since: 3.32
 */
IdeProjectInfo *
ide_greeter_row_get_project_info (IdeGreeterRow *self)
{
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_GREETER_ROW (self), NULL);

  return priv->project_info;
}

static const gchar *
get_language_icon_name (const gchar *language)
{
  g_autofree gchar *lower = NULL;

  if (language == NULL)
    return NULL;

  lower = g_utf8_strdown (language, -1);

  return g_hash_table_lookup (icon_name_map, lower);
}

static void
ide_greeter_row_add_tag (IdeGreeterRow *self,
                         const gchar   *name,
                         gint           kind)
{
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);
  const gchar *icon_name = NULL;
  GtkBox *box;
  GtkImage *image;
  GtkLabel *label;
  gboolean skip_label = FALSE;

  g_assert (IDE_IS_GREETER_ROW (self));
  g_assert (name != NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 3,
                      "visible", TRUE,
                      NULL);


  if (kind == TAG_BUILD_SYSTEM)
    icon_name = "builder-build-symbolic";

  if (kind == TAG_LANGUAGE)
    {
      if ((icon_name = get_language_icon_name (name)))
        skip_label = TRUE;
    }

  if (icon_name != NULL)
    {
      image = g_object_new (GTK_TYPE_IMAGE,
                            "icon-name", icon_name,
                            "valign", GTK_ALIGN_BASELINE,
                            "visible", TRUE,
                            NULL);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (image), "dim-label");
      gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));
    }

  if (!skip_label)
    {
      label = g_object_new (GTK_TYPE_LABEL,
                            "label", name,
                            "valign", GTK_ALIGN_BASELINE,
                            "visible", TRUE,
                            NULL);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (label), "dim-label");
      gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));
    }

  gtk_container_add (GTK_CONTAINER (priv->tags), GTK_WIDGET (box));
}

static gint
compare_language (gconstpointer a,
                  gconstpointer b)
{
  return g_strcmp0 (*(gchar **)a, *(gchar **)b);
}

static gboolean
ignore_build_system (const gchar *str)
{
  /* Handle translated and untranslated strings */
  return ide_str_empty0 (str) ||
         g_str_equal (str, "Directory") ||
         g_str_equal (str, "Fallback") ||
         g_str_equal (str, _("Directory")) ||
         g_str_equal (str, _("Fallback"));
}

void
ide_greeter_row_set_project_info (IdeGreeterRow  *self,
                                  IdeProjectInfo *project_info)
{
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);

  g_return_if_fail (IDE_IS_GREETER_ROW (self));
  g_return_if_fail (!project_info || IDE_IS_PROJECT_INFO (project_info));

  if (g_set_object (&priv->project_info, project_info))
    {
      ide_greeter_row_clear (self);

      if (project_info != NULL)
        {
          g_autofree gchar *collapsed = NULL;
          g_auto(GStrv) languages = g_strdupv ((gchar **)ide_project_info_get_languages (project_info));
          const gchar *name = ide_project_info_get_name (project_info);
          const gchar *build_system = ide_project_info_get_build_system_name (project_info);
          GFile *directory = ide_project_info_get_directory (project_info);
          const gchar *desc = ide_project_info_get_description (project_info);
          GIcon *icon = ide_project_info_get_icon (project_info);

          if (!ide_str_empty0 (desc))
            gtk_widget_set_tooltip_text (GTK_WIDGET (self), desc);

          if (directory != NULL)
            {
              if ((collapsed = ide_path_collapse (g_file_peek_path (directory))))
                desc = collapsed;
            }

          gtk_label_set_label (priv->title, name);
          gtk_label_set_label (priv->subtitle, desc);

          if (!ignore_build_system (build_system))
            ide_greeter_row_add_tag (self, build_system, TAG_BUILD_SYSTEM);

          if (languages != NULL)
            {
              qsort (languages, g_strv_length (languages), sizeof (gchar*), compare_language);
              for (guint i = 0; languages[i] != NULL; i++)
                ide_greeter_row_add_tag (self, g_strstrip (languages[i]), TAG_LANGUAGE);
            }

          if (icon != NULL)
            g_object_set (priv->image,
                          "pixel-size", 32,
                          "gicon", icon,
                          "visible", icon != NULL,
                          NULL);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT_INFO]);
    }
}

/**
 * ide_greeter_row_get_search_text:
 * @self: a #IdeGreeterRow
 *
 * Gets a new string containing the search text for the greeter row.
 *
 * Returns: (transfer full) (nullable): a string or %NULL
 *
 * Since: 3.32
 */
gchar *
ide_greeter_row_get_search_text (IdeGreeterRow *self)
{
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);
  GString *str;

  g_return_val_if_fail (IDE_IS_GREETER_ROW (self), NULL);

  str = g_string_new (NULL);
  g_string_append_printf (str, "%s ", gtk_label_get_text (priv->title) ?: "");
  g_string_append_printf (str, "%s ", gtk_label_get_text (priv->subtitle) ?: "");

  return g_string_free (g_steal_pointer (&str), FALSE);
}

gboolean
ide_greeter_row_get_selection_mode (IdeGreeterRow *self)
{
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_GREETER_ROW (self), FALSE);

  return gtk_widget_get_visible (GTK_WIDGET (priv->check_button));
}

void
ide_greeter_row_set_selection_mode (IdeGreeterRow *self,
                                    gboolean       selection_mode)
{
  IdeGreeterRowPrivate *priv = ide_greeter_row_get_instance_private (self);

  g_return_if_fail (IDE_IS_GREETER_ROW (self));

  gtk_widget_set_visible (GTK_WIDGET (priv->check_button), selection_mode);
}
