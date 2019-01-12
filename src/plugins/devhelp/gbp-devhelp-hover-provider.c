/* gbp-devhelp-hover-provider.c
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

#define G_LOG_DOMAIN "gbp-devhelp-hover-provider"

#include "config.h"

#include <devhelp/devhelp.h>
#include <libide-sourceview.h>
#include <glib/gi18n.h>

#include "gbp-devhelp-hover-provider.h"

/* Given devhelp #671 issue, set a small priority
 * so it stays as the last element on the popover */
#define DEVHELP_HOVER_PROVIDER_PRIORITY 90

struct _GbpDevhelpHoverProvider
{
  GObject parent_instance;
};

typedef struct
{
  IdeHoverContext *context;
  IdeSymbol       *symbol;
  gchar           *word;
} Hover;

static void
hover_free (Hover *h)
{
  g_clear_object (&h->context);
  g_clear_pointer (&h->word, g_free);
  g_clear_object (&h->symbol);
  g_slice_free (Hover, h);
}

static DhKeywordModel *
get_keyword_model (void)
{
  static DhKeywordModel *model;

  g_assert (IDE_IS_MAIN_THREAD ());

  if (model == NULL)
    model = dh_keyword_model_new ();

  return model;
}

static void
find_and_apply_content (Hover *h)
{
  const gchar *names[2] = { NULL };
  DhKeywordModel *model;

  g_assert (h != NULL);
  g_assert (IDE_IS_HOVER_CONTEXT (h->context));

  /* TODO: Add ide_symbol_get_type_name() so that we can resolve the
   *       type and not just the field name.
   */
  if (h->symbol != NULL)
    names[0] = ide_symbol_get_name (h->symbol);
  names[1] = h->word;

  model = get_keyword_model ();

  for (guint i = 0; i < G_N_ELEMENTS (names); i++)
    {
      const gchar *word = names[i];
      g_autofree gchar *freeme = NULL;
      DhLink *item;

      if (dzl_str_empty0 (word))
        continue;

      for (guint j = 0; word[j]; j++)
        {
          if (!g_ascii_isalnum (word[j]) && word[j] != '_')
            {
              word = freeme = g_strndup (word, j);
              break;
            }
        }

      if ((item = dh_keyword_model_filter (model, word, NULL, NULL)))
        {
          g_autoptr(IdeMarkedContent) content = NULL;
          GtkWidget *view;

          view = dh_assistant_view_new ();

          if (!dh_assistant_view_set_link (DH_ASSISTANT_VIEW (view), item))
            {
              gtk_widget_destroy (view);
              break;
            }

          /* It would be nice if we could coordinate with WebKitWebView
           * about a proper natural size request.
           */
          g_object_set (view,
                        "halign", GTK_ALIGN_FILL,
                        "height-request", 200,
                        "hexpand", FALSE,
                        "valign", GTK_ALIGN_START,
                        "vexpand", FALSE,
                        "width-request", 400,
                        NULL);

          ide_hover_context_add_widget (h->context,
                                        DEVHELP_HOVER_PROVIDER_PRIORITY,
                                        _("Devhelp"),
                                        g_steal_pointer (&view));

          break;
        }
    }
}

static void
gbp_devhelp_hover_provider_get_symbol_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  Hover *h;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  h = ide_task_get_task_data (task);
  g_assert (h != NULL);
  g_assert (h->context != NULL);
  g_assert (h->symbol == NULL);

  if ((symbol = ide_buffer_get_symbol_at_location_finish (buffer, result, NULL)))
    h->symbol = g_steal_pointer (&symbol);

  if (ide_task_return_error_if_cancelled (task))
    return;

  find_and_apply_content (h);

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_devhelp_hover_provider_hover_async (IdeHoverProvider    *provider,
                                        IdeHoverContext     *context,
                                        const GtkTextIter   *iter,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpDevhelpHoverProvider *self = (GbpDevhelpHoverProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  IdeBuffer *buffer;
  Hover *h;

  g_assert (GBP_IS_DEVHELP_HOVER_PROVIDER (self));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (iter != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_devhelp_hover_provider_hover_async);

  /*
   * The goal here is to find the name of the symbol underneath the cursor.
   * However, if we fail to resolve that, then we can just use the word as
   * a keyword and search the index using that.
   */

  h = g_slice_new0 (Hover);
  h->context = g_object_ref (context);
  h->word = ide_text_iter_current_symbol (iter, NULL);
  ide_task_set_task_data (task, h, hover_free);

  buffer = IDE_BUFFER (gtk_text_iter_get_buffer (iter));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_buffer_get_symbol_at_location_async (buffer,
                                           iter,
                                           cancellable,
                                           gbp_devhelp_hover_provider_get_symbol_cb,
                                           g_steal_pointer (&task));
}

static gboolean
gbp_devhelp_hover_provider_hover_finish (IdeHoverProvider  *provider,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  g_assert (GBP_IS_DEVHELP_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
  iface->hover_async = gbp_devhelp_hover_provider_hover_async;
  iface->hover_finish = gbp_devhelp_hover_provider_hover_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpDevhelpHoverProvider, gbp_devhelp_hover_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER,
                                                hover_provider_iface_init))

static void
gbp_devhelp_hover_provider_class_init (GbpDevhelpHoverProviderClass *klass)
{
}

static void
gbp_devhelp_hover_provider_init (GbpDevhelpHoverProvider *self)
{
}
