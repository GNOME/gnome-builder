/* ide-project-template.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-project-template"

#include "config.h"

#include "ide-project-template.h"

G_DEFINE_TYPE (IdeProjectTemplate, ide_project_template, IDE_TYPE_TEMPLATE_BASE)

enum {
  PROP_0,
  PROP_DESCRIPTION,
  PROP_ID,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
ide_project_template_real_validate_name (IdeProjectTemplate *self,
                                         const char         *name)
{
  g_assert (IDE_IS_PROJECT_TEMPLATE (self));

  if (name == NULL)
    return FALSE;

  if (g_unichar_isdigit (g_utf8_get_char (name)))
    return FALSE;

  for (const char *c = name; *c; c = g_utf8_next_char (c))
    {
      gunichar ch = g_utf8_get_char (c);

      if (g_unichar_isspace (ch))
        return FALSE;

      if (ch == '/')
        return FALSE;
    }

  return TRUE;
}

static gboolean
ide_project_template_real_validate_app_id (IdeProjectTemplate *self,
                                           const char         *app_id)
{
  guint n_dots = 0;

  g_assert (IDE_IS_PROJECT_TEMPLATE (self));

  /* Rely on defaults if empty */
  if (ide_str_empty0 (app_id))
    return TRUE;

  if (!g_application_id_is_valid (app_id))
    return FALSE;

  /* Flatpak's require at least 3 parts to be valid, which is more than
   * what g_application_id_is_valid() will require. Additionally, you
   * cannot have "-" in Flatpak app ids.
   */
  for (const char *c = app_id; *c; c = g_utf8_next_char (c))
    {
      switch (*c)
        {
        case '-':
          return FALSE;

        case '.':
          n_dots++;
          break;

        default:
          break;
        }
    }

  return n_dots >= 2;
}

static void
ide_project_template_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeProjectTemplate *self = IDE_PROJECT_TEMPLATE (object);

  switch (prop_id)
    {
    case PROP_DESCRIPTION:
      g_value_take_string (value, ide_project_template_get_description (self));
      break;

    case PROP_ID:
      g_value_take_string (value, ide_project_template_get_id (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, ide_project_template_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_template_class_init (IdeProjectTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_project_template_get_property;

  klass->validate_name = ide_project_template_real_validate_name;
  klass->validate_app_id = ide_project_template_real_validate_app_id;

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_project_template_init (IdeProjectTemplate *self)
{
}

gchar *
ide_project_template_get_id (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->get_id (self);
}

gchar *
ide_project_template_get_name (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->get_name (self);
}

gchar *
ide_project_template_get_description (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->get_description (self);
}

/**
 * ide_project_template_get_languages:
 * @self: an #IdeProjectTemplate
 *
 * Gets the list of languages that this template can support when generating
 * the project.
 *
 * Returns: (transfer full): A newly allocated, NULL terminated list of
 *   supported languages.
 */
gchar **
ide_project_template_get_languages (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->get_languages (self);
}

gchar *
ide_project_template_get_icon_name (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->get_icon_name (self);
}

/**
 * ide_project_template_expand_async:
 * @self: an #IdeProjectTemplate
 * @input: the template input
 * @scope: scope for the template
 * @cancellable: (nullable): a #GCancellable or %NULL.
 * @callback: the callback for the asynchronous operation.
 * @user_data: user data for @callback.
 *
 * Asynchronously requests expansion of the template.
 *
 * This may involve creating files and directories on disk as well as
 * expanding files based on the contents of @params.
 *
 * It is expected that this method is only called once on an #IdeProjectTemplate.
 */
void
ide_project_template_expand_async (IdeProjectTemplate  *self,
                                   IdeTemplateInput    *input,
                                   TmplScope           *scope,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_PROJECT_TEMPLATE (self));
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (input));
  g_return_if_fail (scope != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_PROJECT_TEMPLATE_GET_CLASS (self)->expand_async (self, input, scope, cancellable, callback, user_data);
}

gboolean
ide_project_template_expand_finish (IdeProjectTemplate  *self,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->expand_finish (self, result, error);
}

/**
 * ide_project_template_get_priority:
 * @self: a #IdeProjectTemplate
 *
 * Gets the priority of the template. This can be used to sort the templates
 * in the "new project" view.
 *
 * Returns: the priority of the template
 */
gint
ide_project_template_get_priority (IdeProjectTemplate *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), 0);

  if (IDE_PROJECT_TEMPLATE_GET_CLASS (self)->get_priority)
    return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->get_priority (self);

  return 0;
}

gint
ide_project_template_compare (IdeProjectTemplate *a,
                              IdeProjectTemplate *b)
{
  gint ret;

  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (a), 0);
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (b), 0);

  ret = ide_project_template_get_priority (a) - ide_project_template_get_priority (b);

  if (ret == 0)
    {
      g_autofree gchar *a_name = ide_project_template_get_name (a);
      g_autofree gchar *b_name = ide_project_template_get_name (b);
      ret = g_utf8_collate (a_name, b_name);
    }

  return ret;
}

gboolean
ide_project_template_validate_name (IdeProjectTemplate *self,
                                    const char         *name)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), FALSE);

  return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->validate_name (self, name);
}

gboolean
ide_project_template_validate_app_id (IdeProjectTemplate *self,
                                      const char         *app_id)
{
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), FALSE);

  return IDE_PROJECT_TEMPLATE_GET_CLASS (self)->validate_app_id (self, app_id);
}
