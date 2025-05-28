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

#include <glib/gi18n.h>
#include <libpeas.h>

#include <libide-threading.h>
#include <libide-vcs.h>

#include "ide-projects-global.h"
#include "ide-project-template.h"
#include "ide-template-input.h"
#include "ide-template-locator.h"
#include "ide-template-provider.h"

#define DEFAULT_USE_VERSION_CONTROL TRUE
#define DEFAULT_PROJECT_VERSION "0.1.0"
#define DEFAULT_LANGUAGE "C"
#define DEFAULT_LICECNSE_NAME "GPL-3.0-or-later"
#define DEFAULT_VCS_MODULE_NAME "git"

struct _IdeTemplateInput
{
  GObject parent_instance;

  GListStore *templates;
  GtkStringList *languages;
  GtkStringList *licenses;
  GtkFilterListModel *filtered_templates;
  GtkCustomFilter *template_filter;

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
  PROP_LANGUAGES_MODEL,
  PROP_LICENSE_NAME,
  PROP_LICENSES_MODEL,
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
static const struct {
  const char *spdx;
  const char *short_path;
  const char *full_path;
} licenses[] = {
  { "AGPL-3.0-or-later", "agpl_3_short", "agpl_3_full" },
  { "Apache-2.0", "apache_2_short", "apache_2_full" },
  { "EUPL-1.2", "eupl_1_2_short", "eupl_1_2_full" },
  { "GPL-2.0-or-later", "gpl_2_short", "gpl_2_full" },
  { "GPL-3.0-or-later", "gpl_3_short", "gpl_3_full" },
  { "LGPL-2.1-or-later", "lgpl_2_1_short", "lgpl_2_1_full" },
  { "LGPL-3.0-or-later", "lgpl_3_short", "lgpl_3_full" },
  { "MIT", "mit_x11_short", "mit_x11_full" },
  { "MPL-2.0", "mpl_2_short", "mpl_2_full" },
  { "No License", NULL, NULL },
};

static const char *
get_template_name (IdeTemplateInput *self)
{
  guint n_items;

  g_assert (IDE_IS_TEMPLATE_INPUT (self));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->templates));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeProjectTemplate) template = g_list_model_get_item (G_LIST_MODEL (self->templates), i);
      const char *id = ide_project_template_get_id (template);

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

static int
sort_strings (const char * const *a,
              const char * const *b)
{
  return g_strcmp0 (*a, *b);
}

static void
ide_template_input_set_templates (IdeTemplateInput *self,
                                  GPtrArray        *templates)
{
  g_autoptr(GHashTable) seen_languages = NULL;
  g_autofree char **sorted_langs = NULL;
  guint len;

  IDE_ENTRY;

  g_assert (IDE_IS_TEMPLATE_INPUT (self));
  g_assert (templates != NULL);

  seen_languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  g_ptr_array_sort (templates, sort_by_priority);

  for (guint i = 0; i < templates->len; i++)
    {
      IdeProjectTemplate *template = g_ptr_array_index (templates, i);
      const char * const *langs = ide_project_template_get_languages (template);

      g_list_store_append (self->templates, template);

      if (langs == NULL)
        continue;

      for (guint j = 0; langs[j]; j++)
        {
          if (!g_hash_table_contains (seen_languages, langs[j]))
            g_hash_table_insert (seen_languages, g_strdup (langs[j]), NULL);
        }
    }

  if (templates->len > 0)
    {
      const char *id = ide_project_template_get_id (g_ptr_array_index (templates, 0));
      ide_template_input_set_template (self, id);
    }

  sorted_langs = (char **)g_hash_table_get_keys_as_array (seen_languages, &len);
  g_sort_array (sorted_langs,
                len,
                sizeof (char *),
                (GCompareDataFunc) sort_strings,
                NULL);

  gtk_string_list_splice (self->languages, 0, 0,
                          (const char * const *)sorted_langs);

  IDE_EXIT;
}

static gboolean
template_filter_func (gpointer item,
                      gpointer user_data)
{
  IdeProjectTemplate *template = item;
  const char *language = user_data;
  const char * const *languages;

  g_assert (IDE_IS_PROJECT_TEMPLATE (template));
  g_assert (language != NULL);

  if ((languages = ide_project_template_get_languages (template)))
    return g_strv_contains (languages, language);

  return FALSE;
}

static void
foreach_template_provider_cb (PeasExtensionSet *set,
                              PeasPluginInfo   *plugin_info,
                              GObject    *exten,
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

  g_clear_object (&self->template_filter);
  g_clear_object (&self->filtered_templates);
  g_clear_object (&self->directory);
  g_clear_object (&self->templates);
  g_clear_object (&self->languages);
  g_clear_object (&self->licenses);

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
      g_value_set_string (value, get_template_name (self));
      break;

    case PROP_TEMPLATES_MODEL:
      g_value_set_object (value, self->filtered_templates);
      break;

    case PROP_LANGUAGES_MODEL:
      g_value_set_object (value, self->languages);
      break;

    case PROP_LICENSES_MODEL:
      g_value_set_object (value, self->licenses);
      break;

    case PROP_USE_VERSION_CONTROL:
      g_value_set_boolean (value, ide_template_input_get_use_version_control (self));
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

  properties [PROP_LANGUAGES_MODEL] =
    g_param_spec_object ("languages-model", NULL, NULL, G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LICENSES_MODEL] =
    g_param_spec_object ("licenses-model", NULL, NULL, G_TYPE_LIST_MODEL,
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
  self->languages = gtk_string_list_new (NULL);
  self->licenses = gtk_string_list_new (NULL);

  for (guint i = 0; i < G_N_ELEMENTS (licenses); i++)
    gtk_string_list_append (self->licenses, licenses[i].spdx);

  self->template_filter = gtk_custom_filter_new (template_filter_func,
                                                 g_strdup (self->language),
                                                 g_free);
  self->filtered_templates = g_object_new (GTK_TYPE_FILTER_LIST_MODEL,
                                           "filter", self->template_filter,
                                           "model", self->templates,
                                           NULL);
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
  PeasPluginInfo *plugin_info;

  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), FALSE);

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (),
                                             DEFAULT_VCS_MODULE_NAME);

  return self->use_version_control &&
         plugin_info != NULL &&
         peas_plugin_info_is_loaded (plugin_info);
}

void
ide_template_input_set_author (IdeTemplateInput *self,
                               const char       *author)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_set_str (&self->author, author))
    {
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

static void
auto_select_template (IdeTemplateInput *self)
{
  const char *first_id = NULL;
  GListModel *model;
  guint n_items;

  g_assert (IDE_IS_TEMPLATE_INPUT (self));

  model = G_LIST_MODEL (self->filtered_templates);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeProjectTemplate) template = g_list_model_get_item (model, i);
      const char *id = ide_project_template_get_id (template);

      if (ide_str_equal0 (id, self->template))
        return;

      if (first_id == NULL)
        first_id = id;
    }

  if (first_id != NULL)
    ide_template_input_set_template (self, first_id);
}

void
ide_template_input_set_language (IdeTemplateInput *self,
                                 const char       *language)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_set_str (&self->language, language))
    {
      gtk_custom_filter_set_filter_func (self->template_filter,
                                         template_filter_func,
                                         g_strdup (language),
                                         g_free);
      auto_select_template (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }
}

void
ide_template_input_set_name (IdeTemplateInput *self,
                             const char       *name)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_set_str (&self->name, name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

void
ide_template_input_set_app_id (IdeTemplateInput *self,
                               const char       *app_id)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_set_str (&self->app_id, app_id))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_APP_ID]);
    }
}

void
ide_template_input_set_project_version (IdeTemplateInput *self,
                                        const char       *project_version)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_set_str (&self->project_version, project_version))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT_VERSION]);
    }
}

void
ide_template_input_set_license_name (IdeTemplateInput *self,
                                     const char       *license_name)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_set_str (&self->license_name, license_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LICENSE_NAME]);
    }
}

void
ide_template_input_set_template (IdeTemplateInput *self,
                                 const char       *template)
{
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));

  if (g_set_str (&self->template, template))
    {
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

static char *
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

static char *
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

static char *
build_app_path (const char *app_id)
{
  GString *str = g_string_new ("/");

  for (const char *c = app_id; *c; c = g_utf8_next_char (c))
    {
      if (*c == '.')
        g_string_append_c (str, '/');
      else
        g_string_append_unichar (str, g_utf8_get_char (c));
    }

  return g_string_free (str, FALSE);
}

static char *
get_short_license (IdeTemplateInput *self)
{
  g_assert (IDE_IS_TEMPLATE_INPUT (self));

  for (guint i = 0; i < G_N_ELEMENTS (licenses); i++)
    {
      if (g_strcmp0 (licenses[i].spdx, self->license_name) == 0)
        {
          g_autofree char *resource_path = NULL;
          g_autoptr(GBytes) bytes = NULL;
          const guint8 *data;
          gsize len;

          if (licenses[i].short_path == NULL)
            break;

          resource_path = g_strdup_printf ("/org/gnome/libide-projects/licenses/%s",
                                           licenses[i].short_path);
          bytes = g_resources_lookup_data (resource_path, 0, NULL);

          if (bytes == NULL)
            break;

          data = g_bytes_get_data (bytes, &len);

          /* All gresources contain a trailing \0 byte */
          return (char *)g_memdup2 (data, len + 1);
        }
    }

  return g_strdup ("");
}

static char *
get_spdx_id (IdeTemplateInput *self)
{
  g_assert (IDE_IS_TEMPLATE_INPUT (self));

  for (guint i = 0; i < G_N_ELEMENTS (licenses); i++)
    {
      if (g_strcmp0 (licenses[i].spdx, self->license_name) == 0)
        {
          if (licenses[i].short_path != NULL)
            return g_strdup (licenses[i].spdx);
          break;
        }
    }

  return g_strdup ("LicenseRef-proprietary");
}

static TmplScope *
ide_template_input_to_scope (IdeTemplateInput *self)
{
  g_autoptr(TmplScope) scope = NULL;
  g_autoptr(GDateTime) now = NULL;
  g_autoptr(GString) author_escape = NULL;
  g_autofree char *name_lower = NULL;
  g_autofree char *prefix_ = NULL;
  g_autofree char *prefix = NULL;
  g_autofree char *Prefix = NULL;
  g_autofree char *PreFix = NULL;
  const char *app_id;

  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);

  now = g_date_time_new_now_local ();
  scope = tmpl_scope_new ();

  author_escape = g_string_new (self->author);
  g_string_replace (author_escape, "'", "\\'", 0);

  app_id = !ide_str_empty0 (self->app_id) ? self->app_id : "org.gnome.Example";
  tmpl_scope_set_string (scope, "appid", app_id);
  scope_take_string (scope, "appid_path", build_app_path (app_id));

  tmpl_scope_set_string (scope, "template", self->template);
  tmpl_scope_set_string (scope, "author", self->author);
  tmpl_scope_set_string (scope, "author_escape", author_escape->str);
  tmpl_scope_set_string (scope, "project_version", self->project_version);
  scope_take_string (scope, "language", g_utf8_strdown (self->language, -1));
  tmpl_scope_set_boolean (scope, "versioning", ide_template_input_get_use_version_control (self));
  scope_take_string (scope, "project_path", g_file_get_path (self->directory));

  /* Name variants for use as classes, functions, etc */
  name_lower = g_utf8_strdown (self->name ? self->name : "example", -1);
  tmpl_scope_set_string (scope, "name", name_lower);
  scope_take_string (scope, "name_", functify (name_lower));
  scope_take_string (scope, "NAME", g_strdelimit (g_utf8_strup (name_lower, -1), "-", '_'));
  scope_take_string (scope, "year", g_date_time_format (now, "%Y"));
  scope_take_string (scope, "YEAR", g_date_time_format (now, "%Y"));
  scope_take_string (scope, "Title", capitalize (self->name));

  if (g_str_has_suffix (name_lower, "_glib"))
    prefix = g_strndup (name_lower, strlen (name_lower) - 5);
  else
    prefix = g_strdup (name_lower);
  Prefix = capitalize (prefix);
  PreFix = camelize (prefix);
  prefix_ = g_strdelimit (g_utf8_strdown (prefix, -1), "-", '_');

  /* Various prefixes for use as namespaces, etc */
  tmpl_scope_set_string (scope, "prefix", prefix);
  tmpl_scope_set_string (scope, "prefix_", prefix_);
  scope_take_string (scope, "PREFIX", g_strdelimit (g_utf8_strup (prefix, -1), "-", '_'));
  tmpl_scope_set_string (scope, "Prefix", Prefix);
  tmpl_scope_set_string (scope, "PreFix", PreFix);
  scope_take_string (scope, "spaces", g_strnfill (strlen (prefix_), ' '));
  scope_take_string (scope, "Spaces", g_strnfill (strlen (PreFix), ' '));

  scope_take_string (scope, "project_license", get_spdx_id (self));

  return g_steal_pointer (&scope);
}

/**
 * ide_template_input_get_license_path:
 * @self: a #IdeTemplateInput
 *
 * Gets a path to a #GResource containing the full license text.
 *
 * Returns: (transfer full) (nullable): a resource path or %NULL
 */
char *
ide_template_input_get_license_path (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);

  for (guint i = 0; i < G_N_ELEMENTS (licenses); i++)
    {
      if (g_strcmp0 (licenses[i].spdx, self->license_name) == 0)
        {
          if (licenses[i].full_path == NULL)
            return NULL;

          return g_strdup_printf ("/org/gnome/libide-projects/licenses/%s",
                                  licenses[i].full_path);
        }
    }

  return NULL;
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

  return G_LIST_MODEL (self->filtered_templates);
}

/**
 * ide_template_input_get_languages_model:
 * @self: a #IdeTemplateInput
 *
 * Returns: (transfer none): A #GListModel
 */
GListModel *
ide_template_input_get_languages_model (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);

  return G_LIST_MODEL (self->languages);
}

/**
 * ide_template_input_get_licenses_model:
 * @self: a #IdeTemplateInput
 *
 * Returns: (transfer none): A #GListModel
 */
GListModel *
ide_template_input_get_licenses_model (IdeTemplateInput *self)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), NULL);

  return G_LIST_MODEL (self->licenses);
}

static IdeProjectTemplate *
find_template (IdeTemplateInput *self,
               const char       *template_id)
{
  GListModel *model;
  guint n_items;

  g_assert (IDE_IS_TEMPLATE_INPUT (self));

  if (template_id == NULL)
    return NULL;

  model = G_LIST_MODEL (self->templates);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeProjectTemplate) template = g_list_model_get_item (model, i);
      const char *id = ide_project_template_get_id (template);

      if (ide_str_equal0 (template_id, id))
        return g_steal_pointer (&template);
    }

  return NULL;
}

IdeTemplateInputValidation
ide_template_input_validate (IdeTemplateInput *self)
{
  IdeTemplateInputValidation flags = 0;
  IdeProjectTemplate *template;
  g_autoptr(GFile) dest = NULL;
  const char * const *languages;

  g_return_val_if_fail (IDE_IS_TEMPLATE_INPUT (self), 0);

  if (!(template = find_template (self, self->template)))
    flags |= IDE_TEMPLATE_INPUT_INVAL_TEMPLATE;

  if (template && !ide_project_template_validate_app_id (template, self->app_id))
    flags |= IDE_TEMPLATE_INPUT_INVAL_APP_ID;

  if (ide_str_empty0 (self->name))
    flags |= IDE_TEMPLATE_INPUT_INVAL_NAME;
  else if (template && !ide_project_template_validate_name (template, self->name))
    flags |= IDE_TEMPLATE_INPUT_INVAL_NAME;

  if (self->directory == NULL ||
      self->name == NULL ||
      !(dest = g_file_get_child (self->directory, self->name)) ||
      g_file_query_exists (dest, NULL))
    flags |= IDE_TEMPLATE_INPUT_INVAL_LOCATION;

  if (template != NULL && /* ignore if template is not set*/
      (self->language == NULL ||
       !(languages = ide_project_template_get_languages (template)) ||
       !g_strv_contains (languages, self->language)))
    flags |= IDE_TEMPLATE_INPUT_INVAL_LANGUAGE;

  return flags;
}

static void
ide_template_input_initialize_vcs_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeVcsInitializer *initializer = (IdeVcsInitializer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GFile *directory;

  IDE_ENTRY;

  g_assert (IDE_IS_VCS_INITIALIZER (initializer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  directory = ide_task_get_task_data (task);
  g_assert (G_IS_FILE (directory));

  if (!ide_vcs_initializer_initialize_finish (initializer, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_object_ref (directory), g_object_unref);

  ide_object_destroy (IDE_OBJECT (initializer));

  IDE_EXIT;
}

static void
ide_template_input_expand_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr(IdeVcsInitializer) initializer = NULL;
  IdeProjectTemplate *template = (IdeProjectTemplate *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeTemplateInput *self;
  PeasPluginInfo *plugin_info;
  GCancellable *cancellable;
  IdeContext *context = NULL;
  PeasEngine *engine;
  GFile *directory;

  IDE_ENTRY;

  g_assert (IDE_IS_PROJECT_TEMPLATE (template));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  directory = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);
  context = g_object_get_data (G_OBJECT (task), "CONTEXT");

  g_assert (IDE_IS_TEMPLATE_INPUT (self));
  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_CONTEXT (context));

  if (!ide_project_template_expand_finish (template, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  engine = peas_engine_get_default ();

  if (!ide_template_input_get_use_version_control (self) ||
      !(plugin_info = peas_engine_get_plugin_info (engine, DEFAULT_VCS_MODULE_NAME)))
    {
      ide_task_return_pointer (task, g_object_ref (directory), g_object_unref);
      IDE_EXIT;
    }

  initializer = (IdeVcsInitializer *)
    peas_engine_create_extension (engine,
                                  plugin_info,
                                  IDE_TYPE_VCS_INITIALIZER,
                                  "parent", context,
                                  NULL);

  if (initializer == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to create initializer for %s version control",
                                 DEFAULT_VCS_MODULE_NAME);
      IDE_EXIT;
    }

  ide_vcs_initializer_initialize_async (initializer,
                                        directory,
                                        cancellable,
                                        ide_template_input_initialize_vcs_cb,
                                        g_steal_pointer (&task));

  IDE_EXIT;
}

void
ide_template_input_expand_async (IdeTemplateInput    *self,
                                 IdeContext          *context,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeProjectTemplate) template = NULL;
  g_autoptr(TmplScope) scope = NULL;
  g_autoptr(IdeTask) task = NULL;
  TmplTemplateLocator *locator;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TEMPLATE_INPUT (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_template_input_expand_async);
  ide_task_set_task_data (task,
                          g_file_get_child (self->directory, self->name),
                          g_object_unref);
  g_object_set_data_full (G_OBJECT (task),
                          "CONTEXT",
                          g_object_ref (context),
                          g_object_unref);

  if (ide_template_input_validate (self) != IDE_TEMPLATE_INPUT_VALID)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Template input is not valid");
      IDE_EXIT;
    }

  if (!(template = find_template (self, self->template)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Failed to locate template");
      IDE_EXIT;
    }

  if (!(scope = ide_template_input_to_scope (self)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Failed to create scope for template");
      IDE_EXIT;
    }

  if ((locator = ide_template_base_get_locator (IDE_TEMPLATE_BASE (template))) &&
      IDE_IS_TEMPLATE_LOCATOR (locator))
    {
      g_autofree char *license_text = get_short_license (self);
      ide_template_locator_set_license_text (IDE_TEMPLATE_LOCATOR (locator), license_text);
    }

  ide_project_template_expand_async (template,
                                     self,
                                     scope,
                                     cancellable,
                                     ide_template_input_expand_cb,
                                     g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_template_input_expand_finish:
 *
 * Returns: (transfer full): a #GFile or %NULL and @error is set.
 */
GFile *
ide_template_input_expand_finish (IdeTemplateInput  *self,
                                  GAsyncResult      *result,
                                  GError           **error)
{
  GFile *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_TEMPLATE_INPUT (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}
