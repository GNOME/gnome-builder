/*
 * gbp-arduino-template.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-template"

#include "config.h"

#include <libide-threading.h>

#include "gbp-arduino-template.h"

struct _GbpArduinoTemplate
{
  IdeProjectTemplate parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpArduinoTemplate, gbp_arduino_template, IDE_TYPE_PROJECT_TEMPLATE)

static const struct
{
  const char *language;
  const char *resource_path;
  const char *output_path;
  int mode;
} mappings[] = {
  { "C", "/plugins/arduino/resources/sketch.yaml", "sketch.yaml", 0640 },
  { "C", "/plugins/arduino/resources/sketch.ino", "{{exec_name}}.ino", 0640 },
};

static void
gbp_arduino_template_expand_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GbpArduinoTemplate *self = (GbpArduinoTemplate *) object;
  g_autoptr (IdeTask) task = user_data;
  g_autoptr (GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_ARDUINO_TEMPLATE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_template_base_expand_all_finish (IDE_TEMPLATE_BASE (self), result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_arduino_template_expand_async (IdeProjectTemplate *template,
                                   IdeTemplateInput   *input,
                                   TmplScope          *scope,
                                   GCancellable       *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer            user_data)
{
  GbpArduinoTemplate *self = (GbpArduinoTemplate *) template;
  g_autoptr (IdeTask) task = NULL;
  g_autoptr (GFile) destdir = NULL;
  g_autofree char *exec_name = NULL;
  g_autofree char *license_path = NULL;
  const char *language;
  const char *name;
  GFile *directory;

  IDE_ENTRY;

  g_assert (GBP_IS_ARDUINO_TEMPLATE (template));
  g_assert (IDE_IS_TEMPLATE_INPUT (input));
  g_assert (scope != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (template, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_arduino_template_expand_async);

  language = ide_template_input_get_language (input);
  if (!ide_str_equal0 (language, "C"))
    {
      language = "C";
      tmpl_scope_set_string (scope, "language", "C");
    }

  directory = ide_template_input_get_directory (input);
  name = ide_template_input_get_name (input);
  destdir = g_file_get_child (directory, name);

  exec_name = g_strdelimit (g_strstrip (g_strdup (name)), " \t\n", '_');
  tmpl_scope_set_string (scope, "exec_name", exec_name);

  if ((license_path = ide_template_input_get_license_path (input)))
    {
      g_autoptr (GFile) copying = g_file_get_child (destdir, "COPYING");
      ide_template_base_add_resource (IDE_TEMPLATE_BASE (self),
                                      license_path, copying, scope, 0);
    }

  for (guint i = 0; i < G_N_ELEMENTS (mappings); i++)
    {
      const char *resource_path = mappings[i].resource_path;
      const char *output_path = mappings[i].output_path;
      g_autofree char *dest_eval = NULL;
      g_autoptr (GFile) dest_file = NULL;

      if (output_path && strstr (output_path, "{{") != NULL)
        {
          g_autoptr (TmplTemplate) expander = tmpl_template_new (NULL);
          g_autoptr (GError) error = NULL;

          if (!tmpl_template_parse_string (expander, output_path, &error))
            {
              ide_task_return_error (task, g_steal_pointer (&error));
              IDE_EXIT;
            }

          if (!(dest_eval = tmpl_template_expand_string (expander, scope, &error)))
            {
              ide_task_return_error (task, g_steal_pointer (&error));
              IDE_EXIT;
            }

          output_path = dest_eval;
        }

      dest_file = g_file_get_child (destdir, output_path ? output_path : resource_path);

      ide_template_base_add_resource (IDE_TEMPLATE_BASE (self),
                                      resource_path,
                                      dest_file,
                                      scope,
                                      mappings[i].mode);
    }

  ide_template_base_expand_all_async (IDE_TEMPLATE_BASE (self),
                                      cancellable,
                                      gbp_arduino_template_expand_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_arduino_template_expand_finish (IdeProjectTemplate *template,
                                    GAsyncResult       *result,
                                    GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_ARDUINO_TEMPLATE (template));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_arduino_template_class_init (GbpArduinoTemplateClass *klass)
{
  IdeProjectTemplateClass *template_class = IDE_PROJECT_TEMPLATE_CLASS (klass);

  template_class->expand_async = gbp_arduino_template_expand_async;
  template_class->expand_finish = gbp_arduino_template_expand_finish;
}

static void
gbp_arduino_template_init (GbpArduinoTemplate *self)
{
}

