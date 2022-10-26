/* ide-text-buffer-html-generator.c
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

#define G_LOG_DOMAIN "ide-text-buffer-html-generator"

#include "config.h"

#include <libide-code.h>
#include <libide-threading.h>

#include "ide-text-buffer-html-generator.h"

struct _IdeTextBufferHtmlGenerator
{
  IdeHtmlGenerator parent_instance;
  GSignalGroup *buffer_signals;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTextBufferHtmlGenerator, ide_text_buffer_html_generator, IDE_TYPE_HTML_GENERATOR)

static GParamSpec *properties [N_PROPS];

static inline GBytes *
get_buffer_bytes (GtkTextBuffer *buffer)
{
  GtkTextIter begin, end;
  char *slice;

  gtk_text_buffer_get_bounds (buffer, &begin, &end);
  slice = gtk_text_iter_get_slice (&begin, &end);

  return g_bytes_new_take (slice, strlen (slice));
}

static void
ide_text_buffer_html_generator_generate_async (IdeHtmlGenerator    *generator,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeTextBufferHtmlGenerator *self = (IdeTextBufferHtmlGenerator *)generator;
  g_autoptr(GtkTextBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_TEXT_BUFFER_HTML_GENERATOR (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_text_buffer_html_generator_generate_async);

  buffer = g_signal_group_dup_target (self->buffer_signals);

  if (IDE_IS_BUFFER (buffer))
    ide_task_return_pointer (task,
                             ide_buffer_dup_content (IDE_BUFFER (buffer)),
                             g_bytes_unref);
  else if (GTK_IS_TEXT_BUFFER (buffer))
    ide_task_return_pointer (task, get_buffer_bytes (buffer), g_bytes_unref);
  else
    ide_task_return_pointer (task,
                             g_bytes_new_take (g_strdup (""), 0),
                             g_bytes_unref);
}

static GBytes *
ide_text_buffer_html_generator_generate_finish (IdeHtmlGenerator  *generator,
                                                GAsyncResult      *result,
                                                GError           **error)
{
  g_assert (IDE_IS_TEXT_BUFFER_HTML_GENERATOR (generator));
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
ide_text_buffer_html_generator_set_buffer (IdeTextBufferHtmlGenerator *self,
                                           GtkTextBuffer              *buffer)
{
  g_assert (IDE_IS_TEXT_BUFFER_HTML_GENERATOR (self));
  g_assert (!buffer || GTK_IS_TEXT_BUFFER (buffer));

  g_signal_group_set_target (self->buffer_signals, buffer);

  if (IDE_IS_BUFFER (buffer))
    g_object_bind_property_full (buffer, "file",
                                 self, "base-uri",
                                 G_BINDING_SYNC_CREATE,
                                 file_to_base_uri,
                                 NULL, NULL, NULL);
}

static void
ide_text_buffer_html_generator_dispose (GObject *object)
{
  IdeTextBufferHtmlGenerator *self = (IdeTextBufferHtmlGenerator *)object;

  g_clear_object (&self->buffer_signals);

  G_OBJECT_CLASS (ide_text_buffer_html_generator_parent_class)->dispose (object);
}

static void
ide_text_buffer_html_generator_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  IdeTextBufferHtmlGenerator *self = IDE_TEXT_BUFFER_HTML_GENERATOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_take_object (value, g_signal_group_dup_target (self->buffer_signals));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_text_buffer_html_generator_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  IdeTextBufferHtmlGenerator *self = IDE_TEXT_BUFFER_HTML_GENERATOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_text_buffer_html_generator_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_text_buffer_html_generator_class_init (IdeTextBufferHtmlGeneratorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeHtmlGeneratorClass *generator_class = IDE_HTML_GENERATOR_CLASS (klass);

  object_class->dispose = ide_text_buffer_html_generator_dispose;
  object_class->get_property = ide_text_buffer_html_generator_get_property;
  object_class->set_property = ide_text_buffer_html_generator_set_property;

  generator_class->generate_async = ide_text_buffer_html_generator_generate_async;
  generator_class->generate_finish = ide_text_buffer_html_generator_generate_finish;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_text_buffer_html_generator_init (IdeTextBufferHtmlGenerator *self)
{
  self->buffer_signals = g_signal_group_new (GTK_TYPE_TEXT_BUFFER);

  g_signal_group_connect_object (self->buffer_signals,
                                 "changed",
                                 G_CALLBACK (ide_html_generator_invalidate),
                                 self,
                                 G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}
