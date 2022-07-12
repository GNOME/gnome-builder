/* gbp-make-template.c
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

#define G_LOG_DOMAIN "gbp-make-template"

#include "config.h"

#include <libide-threading.h>

#include "gbp-make-template.h"

struct _GbpMakeTemplate
{
  IdeProjectTemplate parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpMakeTemplate, gbp_make_template, IDE_TYPE_PROJECT_TEMPLATE)

static const struct {
  const char *language;
  const char *resource;
  const char *path;
  int mode;
} mappings[] = {
  { NULL, "/plugins/make-templates/resources/Makefile", "Makefile", 0640 },
  { "C", "/plugins/make-templates/resources/main.c", "main.c", 0640 },
  { "C++", "/plugins/make-templates/resources/main.cpp", "main.cpp", 0640 },
};

static void
gbp_make_template_expand_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GbpMakeTemplate *self = (GbpMakeTemplate *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MAKE_TEMPLATE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_template_base_expand_all_finish (IDE_TEMPLATE_BASE (self), result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_make_template_expand_async (IdeProjectTemplate  *template,
                                IdeTemplateInput    *input,
                                TmplScope           *scope,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GbpMakeTemplate *self = (GbpMakeTemplate *)template;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) destdir = NULL;
  g_autofree char *exec_name = NULL;
  g_autofree char *license_path = NULL;
  const char *language;
  const char *name;
  GFile *directory;

  IDE_ENTRY;

  g_assert (IDE_IS_PROJECT_TEMPLATE (self));
  g_assert (IDE_IS_TEMPLATE_INPUT (input));
  g_assert (scope != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_make_template_expand_async);

  language = ide_template_input_get_language (input);

  if (!g_strv_contains (IDE_STRV_INIT ("C", "C++"), language))
    {
      language = "C";
      tmpl_scope_set_string (scope, "language", "c");
    }

  directory = ide_template_input_get_directory (input);
  name = ide_template_input_get_name (input);
  destdir = g_file_get_child (directory, name);

  exec_name = g_strdelimit (g_strstrip (g_strdup (name)), " \t\n", '-');
  tmpl_scope_set_string (scope, "exec_name", exec_name);

  if ((license_path = ide_template_input_get_license_path (input)))
    {
      g_autoptr(GFile) copying = g_file_get_child (destdir, "COPYING");
      ide_template_base_add_resource (IDE_TEMPLATE_BASE (self),
                                      license_path, copying, scope, 0);
    }

  for (guint i = 0; i < G_N_ELEMENTS (mappings); i++)
    {
      g_autoptr(GFile) child = NULL;

      if (mappings[i].language != NULL &&
          !ide_str_equal0 (mappings[i].language, language))
        continue;

      child = g_file_get_child (destdir, mappings[i].path);
      ide_template_base_add_resource (IDE_TEMPLATE_BASE (self),
                                      mappings[i].resource,
                                      child,
                                      scope,
                                      mappings[i].mode);
    }

  ide_template_base_expand_all_async (IDE_TEMPLATE_BASE (self),
                                      cancellable,
                                      gbp_make_template_expand_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_make_template_expand_finish (IdeProjectTemplate  *template,
                                 GAsyncResult        *result,
                                 GError             **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_MAKE_TEMPLATE (template));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_make_template_class_init (GbpMakeTemplateClass *klass)
{
  IdeProjectTemplateClass *template_class = IDE_PROJECT_TEMPLATE_CLASS (klass);

  template_class->expand_async = gbp_make_template_expand_async;
  template_class->expand_finish = gbp_make_template_expand_finish;
}

static void
gbp_make_template_init (GbpMakeTemplate *self)
{
}
