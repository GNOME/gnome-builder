/* gbp-editor-hover-provider.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editor-hover-provider"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-sourceview.h>
#include <libide-threading.h>

#include "gbp-editor-hover-provider.h"

#define DIAGNOSTICS_HOVER_PRIORITY 500

struct _GbpEditorHoverProvider
{
  GObject parent_instance;
};

static void
gbp_editor_hover_provider_hover_async (IdeHoverProvider    *provider,
                                       IdeHoverContext     *context,
                                       const GtkTextIter   *iter,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GbpEditorHoverProvider *self = (GbpEditorHoverProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  GtkTextBuffer *buffer;

  g_assert (GBP_IS_EDITOR_HOVER_PROVIDER (self));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_editor_hover_provider_hover_async);

  buffer = gtk_text_iter_get_buffer (iter);

  if (IDE_IS_BUFFER (buffer))
    {
      GFile *file = ide_buffer_get_file (IDE_BUFFER (buffer));
      guint line = gtk_text_iter_get_line (iter);
      IdeDiagnostics *diagnostics;
      IdeDiagnostic *diag;

      if ((diagnostics = ide_buffer_get_diagnostics (IDE_BUFFER (buffer))) &&
          (diag = ide_diagnostics_get_diagnostic_at_line (diagnostics, file, line)))
        {
          g_autoptr(IdeMarkedContent) content = NULL;
          g_autofree gchar *text = ide_diagnostic_get_text_for_display (diag);

          content = ide_marked_content_new_from_data (text,
                                                      strlen (text),
                                                      IDE_MARKED_KIND_PLAINTEXT);
          ide_hover_context_add_content (context,
                                         DIAGNOSTICS_HOVER_PRIORITY,
                                         _("Diagnostics"),
                                         content);
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "No information to display");
}

static gboolean
gbp_editor_hover_provider_hover_finish (IdeHoverProvider  *self,
                                        GAsyncResult      *result,
                                        GError           **error)
{
  g_assert (IDE_IS_HOVER_PROVIDER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
  iface->hover_async = gbp_editor_hover_provider_hover_async;
  iface->hover_finish = gbp_editor_hover_provider_hover_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditorHoverProvider, gbp_editor_hover_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
gbp_editor_hover_provider_class_init (GbpEditorHoverProviderClass *klass)
{
}

static void
gbp_editor_hover_provider_init (GbpEditorHoverProvider *self)
{
}
