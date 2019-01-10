/* ide-debugger-hover-provider.c
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

#define G_LOG_DOMAIN "ide-debugger-hover-provider"

#include "config.h"

#include <libide-code.h>
#include <libide-core.h>
#include <libide-debugger.h>
#include <libide-sourceview.h>
#include <libide-threading.h>
#include <glib/gi18n.h>

#include "ide-debugger-hover-controls.h"
#include "ide-debugger-hover-provider.h"

#define DEBUGGER_HOVER_PRIORITY 1000

struct _IdeDebuggerHoverProvider
{
  GObject parent_instance;
};

static void
ide_debugger_hover_provider_hover_async (IdeHoverProvider    *provider,
                                         IdeHoverContext     *context,
                                         const GtkTextIter   *iter,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdeDebuggerHoverProvider *self = (IdeDebuggerHoverProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeContext) icontext = NULL;
  IdeDebugManager *dbgmgr;
  const gchar *lang_id;
  IdeBuffer *buffer;
  GFile *file;
  guint line;

  g_assert (IDE_IS_DEBUGGER_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (iter != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_debugger_hover_provider_hover_async);

  buffer = IDE_BUFFER (gtk_text_iter_get_buffer (iter));

  if (gtk_source_buffer_iter_has_context_class (GTK_SOURCE_BUFFER (buffer), iter, "comment"))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  lang_id = ide_buffer_get_language_id (buffer);
  icontext = ide_buffer_ref_context (buffer);
  dbgmgr = ide_debug_manager_from_context (icontext);
  file = ide_buffer_get_file (buffer);
  line = gtk_text_iter_get_line (iter);

  if (ide_debug_manager_supports_language (dbgmgr, lang_id))
    {
      GtkWidget *controls;

      controls = ide_debugger_hover_controls_new (dbgmgr, file, line + 1);
      ide_hover_context_add_widget (context, DEBUGGER_HOVER_PRIORITY, _("Debugger"), controls);
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_debugger_hover_provider_hover_finish (IdeHoverProvider  *provider,
                                          GAsyncResult      *result,
                                          GError           **error)
{
  g_assert (IDE_IS_DEBUGGER_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
  iface->hover_async = ide_debugger_hover_provider_hover_async;
  iface->hover_finish = ide_debugger_hover_provider_hover_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeDebuggerHoverProvider, ide_debugger_hover_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
ide_debugger_hover_provider_class_init (IdeDebuggerHoverProviderClass *klass)
{
}

static void
ide_debugger_hover_provider_init (IdeDebuggerHoverProvider *self)
{
}
