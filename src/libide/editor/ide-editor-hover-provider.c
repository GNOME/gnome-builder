/* ide-editor-hover-provider.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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
 */

#include "config.h"

#define G_LOG_DOMAIN "ide-editor-hover-provider"

#include <glib/gi18n.h>

#include "buffers/ide-buffer.h"
#include "diagnostics/ide-diagnostic.h"
#include "editor/ide-editor-hover-provider.h"
#include "threading/ide-task.h"

struct _IdeEditorHoverProvider
{
  GObject parent_instance;
};

static void
ide_editor_hover_provider_hover_async (IdeHoverProvider    *provider,
                                       IdeHoverContext     *context,
                                       const GtkTextIter   *iter,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  IdeEditorHoverProvider *self = (IdeEditorHoverProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_EDITOR_HOVER_PROVIDER (self));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_hover_provider_hover_async);

  buffer = gtk_text_iter_get_buffer (iter);

  if (IDE_IS_BUFFER (buffer))
    {
      IdeDiagnostic *diag;

      diag = ide_buffer_get_diagnostic_at_iter (IDE_BUFFER (buffer), iter);

      if (diag != NULL)
        {
          g_autoptr(IdeMarkedContent) content = NULL;
          g_autofree gchar *text = ide_diagnostic_get_text_for_display (diag);

          content = ide_marked_content_new_from_data (text,
                                                      strlen (text),
                                                      IDE_MARKED_KIND_PLAINTEXT);
          ide_hover_context_add_content (context, _("Diagnostics"), content);
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "No information to display");
}

static gboolean
ide_editor_hover_provider_hover_finish (IdeHoverProvider  *self,
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
  iface->hover_async = ide_editor_hover_provider_hover_async;
  iface->hover_finish = ide_editor_hover_provider_hover_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeEditorHoverProvider, ide_editor_hover_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
ide_editor_hover_provider_class_init (IdeEditorHoverProviderClass *klass)
{
}

static void
ide_editor_hover_provider_init (IdeEditorHoverProvider *self)
{
}
