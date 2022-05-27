/* ide-template-input.c
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

#define G_LOG_DOMAIN "ide-template-input"

#include "config.h"

#include <libpeas/peas.h>

#include "ide-projects-global.h"
#include "ide-template-input.h"
#include "ide-template-provider.h"

#define DEFAULT_USE_VERSION_CONTROL TRUE
#define DEFAULT_PROJECT_VERSION "0.1.0"
#define DEFAULT_LANGUAGE "C"
#define DEFAULT_LICECNSE_NAME "gpl_3"

struct _IdeTemplateInput
{
  GObject parent_instance;

  GListStore *templates;

  GFile *directory;

  char *app_id;
  char *author;
  char *language;
  char *license_name;
  char *name;
  char *project_version;
  char *template;

  guint use_version_control : 1;
};

enum {
  PROP_0,
  PROP_APP_ID,
  PROP_AUTHOR,
  PROP_DIRECTORY,
  PROP_LANGUAGE,
  PROP_LICENSE_NAME,
  PROP_NAME,
  PROP_PROJECT_VERSION,
  PROP_TEMPLATE,
  PROP_TEMPLATE_NAME,
  PROP_TEMPLATES_MODEL,
  PROP_USE_VERSION_CONTROL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTemplateInput, ide_template_input, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static char *
get_template_name (IdeTemplateInput *self)
{
  guint n_items;

  g_assert (IDE_IS_TEMPLATE_INPUT (self));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->templates));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeProjectTemplate) template = g_list_model_get_item (G_LIST_MODEL (self->templates), i);
      g_autofree char *id = ide_project_template_get_id (template);

      if (g_strcmp0 (id, self->template) == 0)
        return ide_project_template_get_name (template);
    }

  return NULL;
}

static int
sort_by_priority (gconstpointer aptr,
                  gconstpointer bptr)
{
  IdeProjectTemplate *a = *(IdeProjectTemplate **)aptr;
  IdeProjectTemplate *b = *(IdeProjectTemplate **)bptr;

  return ide_project_template_compare (a, b);
}

static void
ide_template_input_set_templates (IdeTemplateInput *self,
                                  GPtrArray        *templates)
{
  IDE_ENTRY;

  g_assert (IDE_IS_TEMPLATE_INPUT (self));
  g_assert (templates != NULL);

  g_ptr_array_sort (templates, sort_by_priority);

  for (guint i = 0; i < templates->len; i++)
    {
      IdeProjectTemplate *template = g_ptr_array_index (templates, i);

      g_list_store_append (self->templates, template);
    }

  if (templates->len > 0)
    {
      g_autofree char *id = ide_project_template_get_id (g_ptr_array_index (templates, 0));
      ide_template_input_set_template (self, id);
    }

  IDE_EXIT;
}

static void
foreach_template_provider_cb (PeasExtensionSet *set,
                              PeasPluginInfo   *plugin_info,
                              PeasExtension    *exten,
                              gpointer          user_data)
{
  IdeTemplateProvider *provider = (IdeTemplateProvider *)exten;
  GPtrArray *templates = user_data;
  GList *list;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TEMPLATE_PROVIDER (provider));

  list = ide_template_provider_get_project_templates (provider);
  for (GList *iter = list; iter; iter = iter->next)
    g_ptr_array_add (templates, g_steal_pointer (&iter->data));
  g_list_free (list);
}

static void
ide_template_input_constructed (GObject *object)
{
  IdeTemplateInput *self = (IdeTemplateInput *)object;
  g_autoptr(PeasExtensionSet) set = NULL;
  g_autoptr(GPtrArray) templates = NULL;

  G_OBJECT_CLASS (ide_template_input_parent_class)->constructed (object);

  templates = g_ptr_array_new_with_free_func (g_object_unref);
  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_TEMPLATE_PROVIDER,
                                NULL);
  peas_extension_set_foreach (set, foreach_template_provider_cb, templates);

  ide_template_input_set_templates (self, templates);
}

static void
ide_template_input_dispose (GObject *object)
{
  IdeTemplateInput *self = (IdeTemplateInput *)object;

  g_clear_object (&self->directory);

  g_clear_pointer (&self->author, g_free);
  g_clear_pointer (&self->language, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->app_id, g_free);
  g_clear_pointer (&self->project_version, g_free);
  g_clear_pointer (&self->license_name, g_free);

  G_OBJECT_CLASS (ide_template_input_parent_class)->dispose (object);
}

static void
ide_template_input_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeTemplateInput *self = IDE_TEMPLATE_INPUT (object);

  switch (prop_id)
    {
    case PROP_AUTHOR:
      g_value_set_string (value, self->author);
      break;

    case PROP_DIRECTORY:
      g_value_set_object (value, self->directory);
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, self->language);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_APP_ID:
      g_value_set_string (value, self->app_id);
      break;

    case PROP_PROJECT_VERSION:
      g_value_set_string (value, self->project_version);
      break;

    case PROP_LICENSE_NAME:
      g_value_set_string (value, self->license_name);
      break;

    case PROP_TEMPLATE:
      g_value_set_string (value, self->template);
      break;

    case PROP_TEMPLATE_NAME:
      g_value_take_string (value, get_template_name (self));
      break;

    case PROP_TEMPLATES_MODEL:
      g_value_set_object (value, self->templates);
      break;

    case PROP_USE_VERSION_CONTROL:
      g_value_set_boolean (value, self->use_version_control);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_template_input_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeTemplateInput *self = IDE_TEMPLATE_INPUT (object);

  switch (prop_id)
    {
    case PROP_AUTHOR:
      ide_template_input_set_author (self, g_value_get_string (value));
      break;

    case PROP_DIRECTORY:
      ide_template_input_set_directory (self, g_value_get_object (value));
      break;

    case PROP_LANGUAGE:
      ide_template_input_set_language (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      ide_template_input_set_name (self, g_value_get_string (value));
      break;

    case PROP_APP_ID:
      ide_template_input_set_app_id (self, g_value_get_string (value));
      break;

    case PROP_PROJECT_VERSION:
      ide_template_input_set_project_version (self, g_value_get_string (value));
      break;

    case PROP_LICENSE_NAME:
      ide_template_input_set_license_name (self, g_value_get_string (value));
      break;

    case PROP_TEMPLATE:
      ide_template_input_set_template (self, g_value_get_string (value));
      break;

    case PROP_USE_VERSION_CONTROL:
      ide_template_input_set_use_version_control (self, g_value_get_boolean (value));
      break;


    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_template_input_class_init (IdeTemplateInputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_template_input_constructed;
  object_class->dispose = ide_template_input_dispose;
  object_class->get_property = ide_template_input_get_property;
  object_class->set_property = ide_template_input_set_property;

  properties [PROP_AUTHOR] =
    g_param_spec_string ("author", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory", NULL, NULL, G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGE] =
    g_param_spec_string ("language", NULL, NULL, DEFAULT_LANGUAGE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, "",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_APP_ID] =
    g_param_spec_string ("app-id", NULL, NULL, "",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROJECT_VERSION] =
    g_param_spec_string ("project-version", NULL, NULL, DEFAULT_PROJECT_VERSION,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LICENSE_NAME] =
    g_param_spec_string ("license-name", NULL, NULL, DEFAULT_LICECNSE_NAME,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEMPLATE] =
    g_param_spec_string ("template", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEMPLATE_NAME] =
    g_param_spec_string ("template-name", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEMPLATES_MODEL] =
    g_param_spec_object ("templates-model", NULL, NULL, G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_VERSION_CONTROL] =
    g_param_spec_boolean ("use-version-control", NULL, NULL, DEFAULT_USE_VERSION_CONTROL,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_template_input_init (IdeTemplateInput *self)
{
  self->name = g_strdup ("");
  self->directory = g_file_new_for_path (ide_get_projects_dir ());
  self->author = g_strdup (g_get_real_name ());
  self->app_id = g_strdup ("");
  self->language = g_strdup (DEFAULT_LANGUAGE);
  self->license_name = g_strdup (DEFAULT_LICECNSE_NAME);
  self->project_version = g_strdup (DEFAULT_PROJECT_VERSION);
  self->use_version_control = DEFAULT_USE_VERSION_CONTROL;
  self->templates = g_list_store_new (IDE_TYPE_PROJECT_TEMPLATE);
}

const char *
ide_template_input_get_author (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);
  return self->author;
}

/**
 * ide_template_input_get_directory:
 * @self: a #IdeTemplateInput
 *
 * Gets the directory to use to contain the new project directory.
 *
 * Returns: (transfer none) (not nullable): a #GFile for the directory
 *   to use when generating the template.
 */
GFile *
ide_template_input_get_directory (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);
  return self->directory;
}

const char *
ide_template_input_get_language (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);
  return self->language;
}

const char *
ide_template_input_get_name (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);
  return self->name;
}

const char *
ide_template_input_get_app_id (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);
  return self->app_id;
}

const char *
ide_template_input_get_project_version (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);
  return self->project_version;
}

const char *
ide_template_input_get_license_name (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);
  return self->license_name;
}

const char *
ide_template_input_get_template (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);
  return self->template;
}

gboolean
ide_template_input_get_use_version_control (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), FALSE);
  return self->use_version_control;
}

void
ide_template_input_set_author (IdeTemplateInput *self,
                               const char       *author)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_strcmp0 (author, self->author) != 0)
    {
      g_free (self->author);
      self->author = g_strdup (author);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AUTHOR]);
    }
}

void
ide_template_input_set_directory (IdeTemplateInput *self,
                                  GFile            *directory)
{
  g_autoptr(GFile) fallback = NULL;

  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  if (directory == NULL)
    directory = fallback = g_file_new_for_path (ide_get_projects_dir ());

  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_FILE (self->directory));

  if (g_file_equal (self->directory, directory))
    return;

  g_set_object (&self->directory, directory);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
}

void
ide_template_input_set_language (IdeTemplateInput *self,
                                 const char       *language)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_strcmp0 (language, self->language) != 0)
    {
      g_free (self->language);
      self->language = g_strdup (language);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }
}

void
ide_template_input_set_name (IdeTemplateInput *self,
                             const char       *name)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_strcmp0 (name, self->name) != 0)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

void
ide_template_input_set_app_id (IdeTemplateInput *self,
                               const char       *app_id)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_strcmp0 (app_id, self->app_id) != 0)
    {
      g_free (self->app_id);
      self->app_id = g_strdup (app_id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_APP_ID]);
    }
}

void
ide_template_input_set_project_version (IdeTemplateInput *self,
                                        const char       *project_version)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_strcmp0 (project_version, self->project_version) != 0)
    {
      g_free (self->project_version);
      self->project_version = g_strdup (project_version);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT_VERSION]);
    }
}

void
ide_template_input_set_license_name (IdeTemplateInput *self,
                                     const char       *license_name)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_strcmp0 (license_name, self->license_name) != 0)
    {
      g_free (self->license_name);
      self->license_name = g_strdup (license_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LICENSE_NAME]);
    }
}

void
ide_template_input_set_template (IdeTemplateInput *self,
                                 const char       *template)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_strcmp0 (template, self->template) != 0)
    {
      g_free (self->template);
      self->template = g_strdup (template);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TEMPLATE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TEMPLATE_NAME]);
    }
}

void
ide_template_input_set_use_version_control (IdeTemplateInput *self,
                                            gboolean          use_version_control)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  use_version_control = !!use_version_control;

  if (use_version_control != self->use_version_control)
    {
      self->use_version_control = use_version_control;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_VERSION_CONTROL]);
    }
}

static void
scope_take_string (TmplScope  *scope,
                   const char *name,
                   char       *value)
{
  tmpl_scope_set_string (scope, name, value);
  g_free (value);
}

static gchar *
capitalize (const gchar *input)
{
  gunichar c;
  GString *str;

  if (input == NULL)
    return NULL;

  if (*input == 0)
    return g_strdup ("");

  c = g_utf8_get_char (input);
  if (g_unichar_isupper (c))
    return g_strdup (input);

  str = g_string_new (NULL);
  input = g_utf8_next_char (input);
  g_string_append_unichar (str, g_unichar_toupper (c));
  if (*input)
    g_string_append (str, input);

  return g_string_free (str, FALSE);
}

static char *
camelize (const char *input)
{
  gboolean next_is_upper = TRUE;
  gboolean skip = FALSE;
  GString *str;

  if (input == NULL)
    return NULL;

  if (!strchr (input, '_') && !strchr (input, ' ') && !strchr (input, '-'))
    return capitalize (input);

  str = g_string_new (NULL);

	for (; *input; input = g_utf8_next_char (input))
    {
      gunichar c = g_utf8_get_char (input);

      switch (c)
      {
      case '_':
      case '-':
      case ' ':
        next_is_upper = TRUE;
        skip = TRUE;
        break;

      default:
        break;
      }

      if (skip)
        {
          skip = FALSE;
          continue;
        }

      if (next_is_upper)
        {
          c = g_unichar_toupper (c);
          next_is_upper = FALSE;
        }
      else
        c = g_unichar_tolower (c);

      g_string_append_unichar (str, c);
    }

  if (g_str_has_suffix (str->str, "Private"))
    g_string_truncate (str, str->len - strlen ("Private"));

  return g_string_free (str, FALSE);
}

static gchar *
functify (const gchar *input)
{
  gunichar last = 0;
  GString *str;

  if (input == NULL)
    return NULL;

  str = g_string_new (NULL);

  for (; *input; input = g_utf8_next_char (input))
    {
      gunichar c = g_utf8_get_char (input);
      gunichar n = g_utf8_get_char (g_utf8_next_char (input));

      if (last)
        {
          if ((g_unichar_islower (last) && g_unichar_isupper (c)) ||
              (g_unichar_isupper (c) && g_unichar_islower (n)))
            g_string_append_c (str, '_');
        }

      if ((c == ' ') || (c == '-'))
        c = '_';

      g_string_append_unichar (str, g_unichar_tolower (c));

      last = c;
    }

  if (g_str_has_suffix (str->str, "_private") ||
      g_str_has_suffix (str->str, "_PRIVATE"))
    g_string_truncate (str, str->len - strlen ("_private"));

  return g_string_free (str, FALSE);
}

/**
 * ide_template_input_to_scope:
 * @self: a #IdeTemplateInput
 *
 * Generates a #TmplScope with various state from the template input.
 *
 * Returns: (transfer full): a #TmplScope that can be used to expand templates
 */
TmplScope *
ide_template_input_to_scope (IdeTemplateInput *self)
{
  g_autoptr(TmplScope) scope = NULL;
  g_autoptr(GDateTime) now = NULL;
  g_autofree char *name_lower = NULL;
  g_autofree char *prefix = NULL;
  g_autofree char *Prefix = NULL;

  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);

  now = g_date_time_new_now_local ();
  scope = tmpl_scope_new ();

  tmpl_scope_set_string (scope, "author", self->author);
  tmpl_scope_set_string (scope, "project_version", self->project_version);
  scope_take_string (scope, "language", g_utf8_strdown (self->language, -1));
  tmpl_scope_set_boolean (scope, "versioning", self->use_version_control);
  scope_take_string (scope, "project_path", g_file_get_path (self->directory));

  /* Name variants for use as classes, functions, etc */
  name_lower = g_utf8_strdown (self->name ? self->name : "example", -1);
  tmpl_scope_set_string (scope, "name", name_lower);
  scope_take_string (scope, "name_", functify (name_lower));
  scope_take_string (scope, "NAME", g_utf8_strup (name_lower, -1));
  scope_take_string (scope, "YEAR", g_date_time_format (now, "%Y"));

  if (g_str_has_suffix (name_lower, "_glib"))
    prefix = g_strndup (name_lower, strlen (name_lower) - 5);
  else
    prefix = g_strdup (name_lower);
  Prefix = camelize (prefix);

  /* Various prefixes for use as namespaces, etc */
  tmpl_scope_set_string (scope, "prefix", prefix);
  scope_take_string (scope, "prefix_", g_utf8_strdown (prefix, -1));
  scope_take_string (scope, "PREFIX", g_utf8_strup (prefix, -1));
  tmpl_scope_set_string (scope, "PreFix", Prefix);
  scope_take_string (scope, "spaces", g_strnfill (strlen (prefix), ' '));
  scope_take_string (scope, "Spaces", g_strnfill (strlen (Prefix), ' '));

  return g_steal_pointer (&scope);
}

/**
 * ide_template_input_get_templates_model:
 * @self: a #IdeTemplateInput
 *
 * Returns: (transfer none): A #GListModel
 */
GListModel *
ide_template_input_get_templates_model (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);

  return G_LIST_MODEL (self->templates);
}
