/* gbp-symbol-hover-provider.c
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

#define G_LOG_DOMAIN "gbp-symbol-hover-provider"

#include "config.h"

#include <libide-code.h>
#include <libide-gui.h>
#include <libide-editor.h>
#include <glib/gi18n.h>

#include "gbp-symbol-hover-provider.h"

#define SYMBOL_TREE_HOVER_PRIORITY 100

struct _GbpSymbolHoverProvider
{
  GObject parent_instance;
};

static gboolean
on_activate_link (GtkLabel    *label,
                  const gchar *uristr,
                  IdeLocation *location)
{
  IdeWorkspace *workspace;
  IdeSurface *surface;

  g_assert (uristr != NULL);
  g_assert (GTK_IS_LABEL (label));
  g_assert (IDE_IS_LOCATION (location));

  if (!(workspace = ide_widget_get_workspace (GTK_WIDGET (label))))
    return FALSE;

  if (!(surface = ide_workspace_get_surface_by_name (workspace, "editor")))
    return FALSE;

  ide_editor_surface_focus_location (IDE_EDITOR_SURFACE (surface), location);

  return TRUE;
}

static void
gbp_symbol_hover_provider_get_symbol_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GString) str = NULL;
  g_autoptr(IdeMarkedContent) content = NULL;
  g_autofree gchar *tt = NULL;
  IdeHoverContext *context;
  const gchar *name;
  GtkWidget *box;
  struct {
    const gchar *kind;
    IdeLocation *loc;
  } loc[] = {
    { _("Location"), NULL },
    { _("Declaration"), NULL },
  };

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(symbol = ide_buffer_get_symbol_at_location_finish (buffer, result, &error)))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  context = ide_task_get_task_data (task);
  g_assert (context != NULL);
  g_assert (IDE_IS_HOVER_CONTEXT (context));

  loc[0].loc = ide_symbol_get_location (symbol);
  loc[1].loc = ide_symbol_get_header_location (symbol);

  if (!loc[0].loc && !loc[1].loc)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);

  name = ide_symbol_get_name (symbol);
  if (!name || !*name)
    name = _("Unnamed Symbol");

  tt = g_strdup_printf ("<tt><span size='smaller'>%s</span></tt>", name);
  gtk_container_add (GTK_CONTAINER (box),
                     g_object_new (GTK_TYPE_LABEL,
                                   "ellipsize", PANGO_ELLIPSIZE_END,
                                   "visible", TRUE,
                                   "xalign", 0.0f,
                                   "margin-bottom", 3,
                                   "selectable", TRUE,
                                   "use-markup", TRUE,
                                   "label", tt,
                                   NULL));


  for (guint i = 0; i < G_N_ELEMENTS (loc); i++)
    {
      if (loc[i].loc != NULL)
        {
          GtkWidget *label;
          GFile *file = ide_location_get_file (loc[i].loc);
          g_autofree gchar *base = g_file_get_basename (file);
          g_autofree gchar *markup = g_strdup_printf ("<span size='smaller'>%s: <a href='#'>%s</a></span>",
                                                      loc[i].kind, base);

          label = g_object_new (GTK_TYPE_LABEL,
                                "visible", TRUE,
                                "xalign", 0.0f,
                                "use-markup", TRUE,
                                "label", markup,
                                NULL);
          g_signal_connect_data (label,
                                 "activate-link",
                                 G_CALLBACK (on_activate_link),
                                 g_object_ref (loc[i].loc),
                                 (GClosureNotify)g_object_unref,
                                 0);
          gtk_container_add (GTK_CONTAINER (box), label);
        }
    }

  ide_hover_context_add_widget (context, SYMBOL_TREE_HOVER_PRIORITY, _("Symbol"), box);
  ide_task_return_boolean (task, TRUE);
}

static void
gbp_symbol_hover_provider_hover_async (IdeHoverProvider    *provider,
                                       IdeHoverContext     *context,
                                       const GtkTextIter   *iter,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GbpSymbolHoverProvider *self = (GbpSymbolHoverProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  IdeBuffer *buffer;

  g_assert (GBP_IS_SYMBOL_HOVER_PROVIDER (self));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (iter != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_symbol_hover_provider_hover_async);
  ide_task_set_task_data (task, g_object_ref (context), g_object_unref);

  buffer = IDE_BUFFER (gtk_text_iter_get_buffer (iter));
  ide_buffer_get_symbol_at_location_async (buffer,
                                           iter,
                                           cancellable,
                                           gbp_symbol_hover_provider_get_symbol_cb,
                                           g_steal_pointer (&task));
}

static gboolean
gbp_symbol_hover_provider_hover_finish (IdeHoverProvider  *provider,
                                        GAsyncResult      *result,
                                        GError           **error)
{
  g_assert (GBP_IS_SYMBOL_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
  iface->hover_async = gbp_symbol_hover_provider_hover_async;
  iface->hover_finish = gbp_symbol_hover_provider_hover_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpSymbolHoverProvider, gbp_symbol_hover_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
gbp_symbol_hover_provider_class_init (GbpSymbolHoverProviderClass *klass)
{
}

static void
gbp_symbol_hover_provider_init (GbpSymbolHoverProvider *self)
{
}
