/* gbp-markdown-html-generator.c
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

#define G_LOG_DOMAIN "gbp-markdown-html-generator"

#include "config.h"

#include <libide-code.h>
#include <libide-threading.h>

#include "gbp-markdown-html-generator.h"

struct _GbpMarkdownHtmlGenerator
{
  IdeHtmlGenerator parent_instance;
  GSignalGroup *buffer_signals;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpMarkdownHtmlGenerator, gbp_markdown_html_generator, IDE_TYPE_HTML_GENERATOR)

static char *markdown_html_prefix;
static char *markdown_html_suffix;
static GParamSpec *properties [N_PROPS];

static void
gbp_markdown_html_generator_generate_async (IdeHtmlGenerator    *generator,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GbpMarkdownHtmlGenerator *self = (GbpMarkdownHtmlGenerator *)generator;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autofree char *escaped = NULL;
  const char *input;
  char *tmp;
  gsize len;

  g_assert (GBP_IS_MARKDOWN_HTML_GENERATOR (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_markdown_html_generator_generate_async);

  if (!(buffer = g_signal_group_dup_target (self->buffer_signals)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Operation was cancelled");
      return;
    }

  bytes = ide_buffer_dup_content (buffer);
  input = g_bytes_get_data (bytes, &len);
  escaped = g_markup_escape_text (input, len);
  tmp = g_strconcat (markdown_html_prefix,
                     escaped,
                     markdown_html_suffix,
                     NULL);

  ide_task_return_pointer (task,
                           g_bytes_new_take (tmp, strlen (tmp)),
                           g_bytes_unref);
}

static GBytes *
gbp_markdown_html_generator_generate_finish (IdeHtmlGenerator  *generator,
                                             GAsyncResult      *result,
                                             GError           **error)
{
  g_assert (GBP_IS_MARKDOWN_HTML_GENERATOR (generator));
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
gbp_markdown_html_generator_set_buffer (GbpMarkdownHtmlGenerator *self,
                                        IdeBuffer                *buffer)
{
  g_assert (GBP_IS_MARKDOWN_HTML_GENERATOR (self));
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
gbp_markdown_html_generator_dispose (GObject *object)
{
  GbpMarkdownHtmlGenerator *self = (GbpMarkdownHtmlGenerator *)object;

  g_clear_object (&self->buffer_signals);

  G_OBJECT_CLASS (gbp_markdown_html_generator_parent_class)->dispose (object);
}

static void
gbp_markdown_html_generator_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbpMarkdownHtmlGenerator *self = GBP_MARKDOWN_HTML_GENERATOR (object);

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
gbp_markdown_html_generator_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbpMarkdownHtmlGenerator *self = GBP_MARKDOWN_HTML_GENERATOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      gbp_markdown_html_generator_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_markdown_html_generator_class_init (GbpMarkdownHtmlGeneratorClass *klass)
{
  g_autoptr(GBytes) markdown_view_js = g_resources_lookup_data ("/plugins/markdown-preview/js/markdown-view.js", 0, NULL);
  g_autoptr(GBytes) markdown_css = g_resources_lookup_data ("/plugins/markdown-preview/css/markdown.css", 0, NULL);
  g_autoptr(GBytes) marked_js = g_resources_lookup_data ("/plugins/markdown-preview/js/marked.js", 0, NULL);

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeHtmlGeneratorClass *generator_class = IDE_HTML_GENERATOR_CLASS (klass);

  object_class->dispose = gbp_markdown_html_generator_dispose;
  object_class->get_property = gbp_markdown_html_generator_get_property;
  object_class->set_property = gbp_markdown_html_generator_set_property;

  generator_class->generate_async = gbp_markdown_html_generator_generate_async;
  generator_class->generate_finish = gbp_markdown_html_generator_generate_finish;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  markdown_html_prefix = g_strconcat ("<html>\n",
                                      " <head>\n",
                                      "  <style>", g_bytes_get_data (markdown_css, NULL), "</style>\n",
                                      "  <script>", g_bytes_get_data (marked_js, NULL), "</script>\n",
                                      "  <script>", g_bytes_get_data (markdown_view_js, NULL), "</script>\n",
                                      " </head>\n",
                                      " <body onload=\"preview()\">\n",
                                      "  <div class=\"markdown-body\" id=\"preview\"></div>\n",
                                      "  <div id=\"markdown-source\">",
                                      NULL);
  markdown_html_suffix = (char *)"</div>\n </body>\n</html>\n";
}

static void
gbp_markdown_html_generator_init (GbpMarkdownHtmlGenerator *self)
{
  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_group_connect_object (self->buffer_signals,
                                 "changed",
                                 G_CALLBACK (ide_html_generator_invalidate),
                                 self,
                                 G_CONNECT_SWAPPED);
}
