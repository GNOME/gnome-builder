/* gbp-sphinx-html-generator.c
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

#define G_LOG_DOMAIN "gbp-sphinx-html-generator"

#include "config.h"

#include <libide-code.h>
#include <libide-threading.h>

#include "gbp-sphinx-compiler.h"
#include "gbp-sphinx-html-generator.h"

struct _GbpSphinxHtmlGenerator
{
  IdeHtmlGenerator   parent_instance;
  GSignalGroup      *buffer_signals;
  GbpSphinxCompiler *compiler;
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_COMPILER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpSphinxHtmlGenerator, gbp_sphinx_html_generator, IDE_TYPE_HTML_GENERATOR)

static GParamSpec *properties [N_PROPS];

static void
gbp_sphinx_html_generator_compile_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpSphinxCompiler *compiler = (GbpSphinxCompiler *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *contents = NULL;
  gsize len;

  g_assert (GBP_IS_SPHINX_COMPILER (compiler));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(contents = gbp_sphinx_compiler_compile_finish (compiler, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  len = strlen (contents);
  ide_task_return_pointer (task,
                           g_bytes_new_take (g_steal_pointer (&contents), len),
                           g_bytes_unref);
}

static void
gbp_sphinx_html_generator_generate_async (IdeHtmlGenerator    *generator,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GbpSphinxHtmlGenerator *self = (GbpSphinxHtmlGenerator *)generator;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = NULL;
  GFile *file;

  g_assert (GBP_IS_SPHINX_HTML_GENERATOR (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_sphinx_html_generator_generate_async);

  if (self->compiler == NULL ||
      !(buffer = g_signal_group_dup_target (self->buffer_signals)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Operation was cancelled");
      return;
    }

  file = ide_buffer_get_file (buffer);

  gbp_sphinx_compiler_compile_async (self->compiler,
                                     file,
                                     NULL,
                                     cancellable,
                                     gbp_sphinx_html_generator_compile_cb,
                                     g_steal_pointer (&task));
}

static GBytes *
gbp_sphinx_html_generator_generate_finish (IdeHtmlGenerator  *generator,
                                           GAsyncResult      *result,
                                           GError           **error)
{
  g_assert (GBP_IS_SPHINX_HTML_GENERATOR (generator));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static gboolean
file_to_base_uri (GBinding     *binding,
                  const GValue *from,
                  GValue       *to,
                  gpointer      user_data)
{
  g_value_set_string (to, g_file_get_uri (g_value_get_object (from)));
  return TRUE;
}

static void
gbp_sphinx_html_generator_set_buffer (GbpSphinxHtmlGenerator *self,
                                      IdeBuffer              *buffer)
{
  g_assert (GBP_IS_SPHINX_HTML_GENERATOR (self));
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  g_signal_group_set_target (self->buffer_signals, buffer);

  if (IDE_IS_BUFFER (buffer))
    g_object_bind_property_full (buffer, "file",
                                 self, "base-uri",
                                 G_BINDING_SYNC_CREATE,
                                 file_to_base_uri,
                                 NULL, NULL, NULL);
}

static void
gbp_sphinx_html_generator_dispose (GObject *object)
{
  GbpSphinxHtmlGenerator *self = (GbpSphinxHtmlGenerator *)object;

  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->compiler);

  G_OBJECT_CLASS (gbp_sphinx_html_generator_parent_class)->dispose (object);
}

static void
gbp_sphinx_html_generator_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpSphinxHtmlGenerator *self = GBP_SPHINX_HTML_GENERATOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_take_object (value, g_signal_group_dup_target (self->buffer_signals));
      break;

    case PROP_COMPILER:
      g_value_set_object (value, self->compiler);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_sphinx_html_generator_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbpSphinxHtmlGenerator *self = GBP_SPHINX_HTML_GENERATOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      gbp_sphinx_html_generator_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_COMPILER:
      self->compiler = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_sphinx_html_generator_class_init (GbpSphinxHtmlGeneratorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeHtmlGeneratorClass *generator_class = IDE_HTML_GENERATOR_CLASS (klass);

  object_class->dispose = gbp_sphinx_html_generator_dispose;
  object_class->get_property = gbp_sphinx_html_generator_get_property;
  object_class->set_property = gbp_sphinx_html_generator_set_property;

  generator_class->generate_async = gbp_sphinx_html_generator_generate_async;
  generator_class->generate_finish = gbp_sphinx_html_generator_generate_finish;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_COMPILER] =
    g_param_spec_object ("compiler", NULL, NULL,
                         GBP_TYPE_SPHINX_COMPILER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_sphinx_html_generator_init (GbpSphinxHtmlGenerator *self)
{
  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_group_connect_object (self->buffer_signals,
                                 "changed",
                                 G_CALLBACK (ide_html_generator_invalidate),
                                 self,
                                 G_CONNECT_SWAPPED);
}
