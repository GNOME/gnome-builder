/* ide-html-generator.c
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

#define G_LOG_DOMAIN "ide-html-generator"

#include "config.h"

#include <libide-threading.h>

#include "ide-html-generator.h"
#include "ide-text-buffer-html-generator.h"

typedef struct
{
  char *base_uri;
} IdeHtmlGeneratorPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeHtmlGenerator, ide_html_generator, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BASE_URI,
  N_PROPS
};

enum {
  INVALIDATE,
  N_SIGNALS
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

static void
ide_html_generator_real_generate_async (IdeHtmlGenerator    *self,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_html_generator_real_generate_async);
  ide_task_return_unsupported_error (task);
}

static GBytes *
ide_html_generator_real_generate_finish (IdeHtmlGenerator  *self,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_html_generator_dispose (GObject *object)
{
  IdeHtmlGenerator *self = (IdeHtmlGenerator *)object;
  IdeHtmlGeneratorPrivate *priv = ide_html_generator_get_instance_private (self);

  g_clear_pointer (&priv->base_uri, g_free);

  G_OBJECT_CLASS (ide_html_generator_parent_class)->dispose (object);
}

static void
ide_html_generator_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeHtmlGenerator *self = IDE_HTML_GENERATOR (object);

  switch (prop_id)
    {
    case PROP_BASE_URI:
      g_value_set_string (value, ide_html_generator_get_base_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_html_generator_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeHtmlGenerator *self = IDE_HTML_GENERATOR (object);

  switch (prop_id)
    {
    case PROP_BASE_URI:
      ide_html_generator_set_base_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_html_generator_class_init (IdeHtmlGeneratorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_html_generator_dispose;
  object_class->get_property = ide_html_generator_get_property;
  object_class->set_property = ide_html_generator_set_property;

  klass->generate_async = ide_html_generator_real_generate_async;
  klass->generate_finish = ide_html_generator_real_generate_finish;

  properties [PROP_BASE_URI] =
    g_param_spec_string ("base-uri", NULL, NULL, NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeHtmlGenerator::invalidate:
   *
   * The "invalidate" signal is emitted when contents have changed.
   *
   * This signal will be emitted by subclasses when the contents have changed
   * and HTML will need to be regenerated.
   */
  signals [INVALIDATE] =
    g_signal_new ("invalidate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeHtmlGeneratorClass, invalidate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
ide_html_generator_init (IdeHtmlGenerator *self)
{
}

/**
 * ide_html_generator_generate_async:
 * @self: a #IdeHtmlGenerator
 * @cancellable: a #GCancellable
 * @callback: a function to call after completion
 * @user_data: closure data for @callback
 *
 * Asynchronously generate HTML.
 *
 * This virtual function should be implemented by subclasses to generate
 * HTML based on some form of input (which is left to the subclass).
 *
 * Upon completion, @callback is called and expected to call
 * ide_html_generator_generate_finish() to retrieve the result.
 */
void
ide_html_generator_generate_async (IdeHtmlGenerator    *self,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_HTML_GENERATOR (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_HTML_GENERATOR_GET_CLASS (self)->generate_async (self, cancellable, callback, user_data);
}

/**
 * ide_html_generator_generate_finish:
 * @self: a #IdeHtmlGenerator
 * @result: a #GAsyncResult
 * @error: a location for a #GError
 *
 * Completes a request to generate HTML.
 *
 * This function is used to complete a request to generate HTML from some
 * form of input, asynchronously. The content of the HTML is dependent on
 * the subclass implementation of #IdeHtmlGenerator.
 *
 * It is required that the resulting bytes have a NULL terminator at
 * the end which is not part of the bytes length.
 *
 * Returns: (transfer full): a #GBytes if successful; otherwise %NULL
 *   and @error is set.
 */
GBytes *
ide_html_generator_generate_finish (IdeHtmlGenerator  *self,
                                    GAsyncResult      *result,
                                    GError           **error)
{
  GBytes *ret;

  g_return_val_if_fail (IDE_IS_HTML_GENERATOR (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = IDE_HTML_GENERATOR_GET_CLASS (self)->generate_finish (self, result, error);

#ifdef G_ENABLE_DEBUG
  if (ret != NULL)
    {
      const guint8 *data;
      gsize len;

      data = g_bytes_get_data (ret, &len);
      g_assert (data[len] == 0);
    }
#endif

  return ret;
}

/**
 * ide_html_generator_invalidate:
 * @self: a #IdeHtmlGenerator
 *
 * Notifies that the last generated HTML is now invalid.
 *
 * This is used by subclasses to denote that the HTML contents
 * have changed and will need to be regenerated.
 */
void
ide_html_generator_invalidate (IdeHtmlGenerator *self)
{
  g_return_if_fail (IDE_IS_HTML_GENERATOR (self));

  g_signal_emit (self, signals [INVALIDATE], 0);
}

/**
 * ide_html_generator_new_for_buffer:
 * @buffer: a #GtkTextBuffer
 *
 * Create a 1:1 HTML generator for a buffer.
 *
 * Creates a #IdeHtmlGenerator that passes the content directly from
 * what is found in a #GtkTextBuffer.
 *
 * Returns: (transfer full): an #IdeHtmlGenerator
 */
IdeHtmlGenerator *
ide_html_generator_new_for_buffer (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  return g_object_new (IDE_TYPE_TEXT_BUFFER_HTML_GENERATOR,
                       "buffer", buffer,
                       NULL);
}

const char *
ide_html_generator_get_base_uri (IdeHtmlGenerator *self)
{
  IdeHtmlGeneratorPrivate *priv = ide_html_generator_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_HTML_GENERATOR (self), NULL);

  return priv->base_uri;
}

void
ide_html_generator_set_base_uri (IdeHtmlGenerator *self,
                                 const char       *base_uri)
{
  IdeHtmlGeneratorPrivate *priv = ide_html_generator_get_instance_private (self);

  g_return_if_fail (IDE_IS_HTML_GENERATOR (self));

  if (g_set_str (&priv->base_uri, base_uri))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BASE_URI]);
    }
}
