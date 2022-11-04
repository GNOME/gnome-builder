/* gbp-rst-html-generator.c
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

#define G_LOG_DOMAIN "gbp-rst-html-generator"

#include "config.h"

#include <errno.h>

#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-rst-html-generator.h"

struct _GbpRstHtmlGenerator
{
  IdeHtmlGenerator parent_instance;
  GSignalGroup *buffer_signals;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpRstHtmlGenerator, gbp_rst_html_generator, IDE_TYPE_HTML_GENERATOR)

static GParamSpec *properties [N_PROPS];
static GBytes *rst2html;

static void
gbp_rst_html_generator_communicate_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;
  gsize len;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  len = strlen (stdout_buf);
  bytes = g_bytes_new_take (g_steal_pointer (&stdout_buf), len);

  ide_task_return_pointer (task,
                           g_steal_pointer (&bytes),
                           g_bytes_unref);
}

static const char *
find_python (void)
{
  static const char *python;

  if (python == NULL)
    {
      if (g_find_program_in_path ("python3"))
        python = "python3";
      else
        python = "python";
    }

  return python;
}

static void
gbp_rst_html_generator_generate_async (IdeHtmlGenerator    *generator,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GbpRstHtmlGenerator *self = (GbpRstHtmlGenerator *)generator;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) content = NULL;
  GSubprocessFlags flags;
  const char *python;
  const char *source_path;
  GFile *file;
  int fd;

  g_assert (GBP_IS_RST_HTML_GENERATOR (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_rst_html_generator_generate_async);

  if (!(buffer = g_signal_group_dup_target (self->buffer_signals)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "Operation was cancelled");
      return;
    }

  content = ide_buffer_dup_content (buffer);
  file = ide_buffer_get_file (buffer);
  source_path = g_file_peek_path (file);

  if (-1 == (fd = ide_foundry_bytes_to_memfd (content, "rst2html-input")))
    {
      int errsv = errno;
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 g_io_error_from_errno (errsv),
                                 "%s",
                                 g_strerror (errsv));
      return;
    }

  python = find_python ();
  flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDIN_PIPE;

  if (!g_getenv ("RST_DEBUG"))
    flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;

  launcher = ide_subprocess_launcher_new (flags);
  ide_subprocess_launcher_push_args (launcher, IDE_STRV_INIT (python, "-", source_path));
  ide_subprocess_launcher_take_fd (launcher, fd, 3);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         (const char *)g_bytes_get_data (rst2html, NULL),
                                         cancellable,
                                         gbp_rst_html_generator_communicate_cb,
                                         g_steal_pointer (&task));
}

static GBytes *
gbp_rst_html_generator_generate_finish (IdeHtmlGenerator  *generator,
                                        GAsyncResult      *result,
                                        GError           **error)
{
  g_assert (GBP_IS_RST_HTML_GENERATOR (generator));
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
gbp_rst_html_generator_set_buffer (GbpRstHtmlGenerator *self,
                                   IdeBuffer           *buffer)
{
  g_assert (GBP_IS_RST_HTML_GENERATOR (self));
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
gbp_rst_html_generator_dispose (GObject *object)
{
  GbpRstHtmlGenerator *self = (GbpRstHtmlGenerator *)object;

  g_clear_object (&self->buffer_signals);

  G_OBJECT_CLASS (gbp_rst_html_generator_parent_class)->dispose (object);
}

static void
gbp_rst_html_generator_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpRstHtmlGenerator *self = GBP_RST_HTML_GENERATOR (object);

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
gbp_rst_html_generator_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpRstHtmlGenerator *self = GBP_RST_HTML_GENERATOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      gbp_rst_html_generator_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_rst_html_generator_class_init (GbpRstHtmlGeneratorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeHtmlGeneratorClass *generator_class = IDE_HTML_GENERATOR_CLASS (klass);

  object_class->dispose = gbp_rst_html_generator_dispose;
  object_class->get_property = gbp_rst_html_generator_get_property;
  object_class->set_property = gbp_rst_html_generator_set_property;

  generator_class->generate_async = gbp_rst_html_generator_generate_async;
  generator_class->generate_finish = gbp_rst_html_generator_generate_finish;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  rst2html = g_resources_lookup_data ("/plugins/sphinx-preview/rst2html.py", 0, NULL);
}

static void
gbp_rst_html_generator_init (GbpRstHtmlGenerator *self)
{
  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_group_connect_object (self->buffer_signals,
                                 "changed",
                                 G_CALLBACK (ide_html_generator_invalidate),
                                 self,
                                 G_CONNECT_SWAPPED);
}
