/* gbp-meson-template.c
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

#define G_LOG_DOMAIN "gbp-meson-template"

#include "config.h"

#include <string.h>

#include <libide-threading.h>

#include "gbp-meson-template.h"

struct _GbpMesonTemplate
{
  IdeProjectTemplate parent_instance;
  const char * const *extra_scope;
  const GbpMesonTemplateExpansion *expansions;
  guint n_expansions;
  const GbpMesonTemplateLanguageScope *language_scope;
  guint n_language_scope;
};

G_DEFINE_FINAL_TYPE (GbpMesonTemplate, gbp_meson_template, IDE_TYPE_PROJECT_TEMPLATE)

static void
add_to_scope (TmplScope  *scope,
              const char *pattern)
{
  g_autofree char *key = NULL;
  const char *val;

  g_assert (scope != NULL);
  g_assert (pattern != NULL);

  val = strchr (pattern, '=');

  /* If it is just "FOO" then set "FOO" to True */
  if (val == NULL)
    {
      tmpl_scope_set_boolean (scope, pattern, TRUE);
      return;
    }

  key = g_strndup (pattern, val - pattern);
  val++;

  /* If simple key=value, set the bool/string */
  if (strstr (val, "{{") == NULL)
    {
      if (ide_str_equal0 (val, "false"))
        tmpl_scope_set_boolean (scope, key, FALSE);
      else if (ide_str_equal0 (val, "true"))
        tmpl_scope_set_boolean (scope, key, TRUE);
      else
        tmpl_scope_set_string (scope, key, val);

      return;
    }

  /* More complex, we have a template to expand from scope */
  {
    g_autoptr(TmplTemplate) template = tmpl_template_new (NULL);
    g_autoptr(GError) error = NULL;
    g_autofree char *expanded = NULL;

    if (!tmpl_template_parse_string (template, val, &error))
      {
        g_warning ("Failed to parse template %s: %s",
                   val, error->message);
        return;
      }

    if (!(expanded = tmpl_template_expand_string (template, scope, &error)))
      {
        g_warning ("Failed to expand template %s: %s",
                   val, error->message);
        return;
      }

    tmpl_scope_set_string (scope, key, expanded);
  }
}

static void
gbp_meson_template_expand_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GbpMesonTemplate *self = (GbpMesonTemplate *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TEMPLATE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_template_base_expand_all_finish (IDE_TEMPLATE_BASE (self), result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_meson_template_expand_async (IdeProjectTemplate  *template,
                                 IdeTemplateInput    *input,
                                 TmplScope           *scope,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GbpMesonTemplate *self = (GbpMesonTemplate *)template;
  g_autofree char *license_path = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) destdir = NULL;
  const char *language;
  const char *name;
  GFile *directory;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TEMPLATE (template));
  g_assert (IDE_IS_TEMPLATE_INPUT (input));
  g_assert (scope != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (template, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_template_expand_async);

  name = ide_template_input_get_name (input);
  language = ide_template_input_get_language (input);
  directory = ide_template_input_get_directory (input);
  destdir = g_file_get_child (directory, name);

  if (self->expansions == NULL || self->n_expansions == 0)
    {
      ide_task_return_unsupported_error (task);
      IDE_EXIT;
    }

  /* Setup our license for the project */
  if ((license_path = ide_template_input_get_license_path (input)))
    {
      g_autoptr(GFile) copying = g_file_get_child (destdir, "COPYING");
      ide_template_base_add_resource (IDE_TEMPLATE_BASE (self),
                                      license_path, copying, scope, 0);
    }

  /* First setup some defaults for our scope */
  tmpl_scope_set_boolean (scope, "is_adwaita", FALSE);
  tmpl_scope_set_boolean (scope, "is_gtk4", FALSE);
  tmpl_scope_set_boolean (scope, "is_cli", FALSE);
  tmpl_scope_set_boolean (scope, "enable_gnome", FALSE);
  tmpl_scope_set_boolean (scope, "enable_i18n", FALSE);

  /* Add any extra scope to the expander which might be needed */
  if (self->extra_scope != NULL)
    {
      for (guint j = 0; self->extra_scope[j]; j++)
        add_to_scope (scope, self->extra_scope[j]);
    }

  /* Now add any per-language scope necessary */
  if (self->language_scope != NULL)
    {
      for (guint j = 0; j < self->n_language_scope; j++)
        {
          if (!ide_str_equal0 (language, self->language_scope[j].language) ||
              self->language_scope[j].extra_scope == NULL)
            continue;

          for (guint k = 0; self->language_scope[j].extra_scope[k]; k++)
            add_to_scope (scope, self->language_scope[j].extra_scope[k]);
        }
    }

  for (guint i = 0; i < self->n_expansions; i++)
    {
      const char *src = self->expansions[i].input;
      const char *dest = self->expansions[i].output_pattern;
      g_autofree char *dest_eval = NULL;
      g_autofree char *resource_path = NULL;
      g_autoptr(GFile) dest_file = NULL;
      int mode = 0;

      if (self->expansions[i].languages != NULL &&
          !g_strv_contains (self->expansions[i].languages, language))
        continue;

      /* Expand the destination filename if necessary using a template */
      if (strstr (dest, "{{") != NULL)
        {
          g_autoptr(TmplTemplate) expander = tmpl_template_new (NULL);
          g_autoptr(GError) error = NULL;

          if (!tmpl_template_parse_string (expander, dest, &error))
            {
              ide_task_return_error (task, g_steal_pointer (&error));
              IDE_EXIT;
            }

          if (!(dest_eval = tmpl_template_expand_string (expander, scope, &error)))
            {
              ide_task_return_error (task, g_steal_pointer (&error));
              IDE_EXIT;
            }

          dest = dest_eval;
        }

      resource_path = g_strdup_printf ("/plugins/meson-templates/resources/%s", src);
      dest_file = g_file_get_child (destdir, dest);

      if (self->expansions[i].executable)
        mode = 0750;

      ide_template_base_add_resource (IDE_TEMPLATE_BASE (self),
                                      resource_path, dest_file, scope, mode);
    }

  ide_template_base_expand_all_async (IDE_TEMPLATE_BASE (self),
                                      cancellable,
                                      gbp_meson_template_expand_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_meson_template_expand_finish (IdeProjectTemplate  *template,
                                  GAsyncResult        *result,
                                  GError             **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TEMPLATE (template));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_meson_template_class_init (GbpMesonTemplateClass *klass)
{
  IdeProjectTemplateClass *template_class = IDE_PROJECT_TEMPLATE_CLASS (klass);

  template_class->expand_async = gbp_meson_template_expand_async;
  template_class->expand_finish = gbp_meson_template_expand_finish;
}

static void
gbp_meson_template_init (GbpMesonTemplate *self)
{
}

void
gbp_meson_template_set_expansions (GbpMesonTemplate                *self,
                                   const GbpMesonTemplateExpansion *expansions,
                                   guint                            n_expansions)
{
  g_return_if_fail (GBP_IS_MESON_TEMPLATE (self));
  g_return_if_fail (n_expansions == 0 || expansions != NULL);

  self->expansions = expansions;
  self->n_expansions = n_expansions;
}

void
gbp_meson_template_set_extra_scope (GbpMesonTemplate   *self,
                                    const char * const *extra_scope)
{
  g_return_if_fail (GBP_IS_MESON_TEMPLATE (self));

  self->extra_scope = extra_scope;
}

void
gbp_meson_template_set_language_scope (GbpMesonTemplate                    *self,
                                       const GbpMesonTemplateLanguageScope *language_scope,
                                       guint                                n_language_scope)
{
  g_return_if_fail (GBP_IS_MESON_TEMPLATE (self));
  g_return_if_fail (n_language_scope == 0 || language_scope != NULL);

  self->language_scope = language_scope;
  self->n_language_scope = n_language_scope;
}
