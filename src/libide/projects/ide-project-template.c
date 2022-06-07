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

typedef struct
{
  char *id;
  char *name;
  char *description;
  char **languages;
  int priority;
} IdeProjectTemplatePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeProjectTemplate, ide_project_template, IDE_TYPE_TEMPLATE_BASE)

enum {
  PROP_0,
  PROP_DESCRIPTION,
  PROP_ID,
  PROP_NAME,
  PROP_LANGUAGES,
  PROP_PRIORITY,
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
ide_project_template_dispose (GObject *object)
{
  IdeProjectTemplate *self = (IdeProjectTemplate *)object;
  IdeProjectTemplatePrivate *priv = ide_project_template_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->description, g_free);
  g_clear_pointer (&priv->languages, g_strfreev);

  G_OBJECT_CLASS (ide_project_template_parent_class)->dispose (object);
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
      g_value_set_string (value, ide_project_template_get_description (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_project_template_get_id (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_project_template_get_name (self));
      break;

    case PROP_LANGUAGES:
      g_value_set_boxed (value, ide_project_template_get_languages (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, ide_project_template_get_priority (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_template_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeProjectTemplate *self = IDE_PROJECT_TEMPLATE (object);
  IdeProjectTemplatePrivate *priv = ide_project_template_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DESCRIPTION:
      priv->description = g_value_dup_string (value);
      break;

    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_LANGUAGES:
      priv->languages = g_value_dup_boxed (value);
      break;

    case PROP_PRIORITY:
      priv->priority = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_template_class_init (IdeProjectTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_project_template_dispose;
  object_class->get_property = ide_project_template_get_property;
  object_class->set_property = ide_project_template_set_property;

  klass->validate_name = ide_project_template_real_validate_name;
  klass->validate_app_id = ide_project_template_real_validate_app_id;

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL, NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL, NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGES] =
    g_param_spec_boxed ("languages", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_project_template_init (IdeProjectTemplate *self)
{
}

const char *
ide_project_template_get_id (IdeProjectTemplate *self)
{
  IdeProjectTemplatePrivate *priv = ide_project_template_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return priv->id;
}

const char *
ide_project_template_get_name (IdeProjectTemplate *self)
{
  IdeProjectTemplatePrivate *priv = ide_project_template_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return priv->name;
}

const char *
ide_project_template_get_description (IdeProjectTemplate *self)
{
  IdeProjectTemplatePrivate *priv = ide_project_template_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return priv->description;
}

/**
 * ide_project_template_get_languages:
 * @self: an #IdeProjectTemplate
 *
 * Gets the list of languages that this template can support when generating
 * the project.
 *
 * Returns: (transfer none) (nullable): an array of language names
 */
const char * const *
ide_project_template_get_languages (IdeProjectTemplate *self)
{
  IdeProjectTemplatePrivate *priv = ide_project_template_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), NULL);

  return (const char * const *)priv->languages;
}

int
ide_project_template_get_priority (IdeProjectTemplate *self)
{
  IdeProjectTemplatePrivate *priv = ide_project_template_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (self), 0);

  return priv->priority;
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

int
ide_project_template_compare (IdeProjectTemplate *a,
                              IdeProjectTemplate *b)
{
  const char *a_name;
  const char *b_name;
  int prio_a;
  int prio_b;

  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (a), 0);
  g_return_val_if_fail (IDE_IS_PROJECT_TEMPLATE (b), 0);

  prio_a = ide_project_template_get_priority (a);
  prio_b = ide_project_template_get_priority (b);

  if (prio_a < prio_b)
    return -1;
  else if (prio_a > prio_b)
    return 1;

  a_name = ide_project_template_get_name (a);
  b_name = ide_project_template_get_name (b);

  return g_utf8_collate (a_name, b_name);
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
