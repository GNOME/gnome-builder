/* ide-lsp-highlighter.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-lsp-highlighter"

#include "config.h"

#include <libide-code.h>
#include <jsonrpc-glib.h>

#include "ide-lsp-highlighter.h"
#include "ide-lsp-util.h"

#define DELAY_TIMEOUT_MSEC 333

/*
 * NOTE: This is not an ideal way to do an indexer because we don't get all the
 * symbols that might be available. It also doesn't allow us to restrict the
 * highlights to the proper scope. However, until Language Server Protocol
 * provides a way to do this, it's about the best we can do.
 */

typedef struct
{
  IdeHighlightEngine *engine;

  IdeLspClient       *client;
  IdeHighlightIndex  *index;
  GSignalGroup       *buffer_signals;

  const gchar        *style_map[IDE_SYMBOL_KIND_LAST];

  guint               queued_update;

  guint               active : 1;
  guint               dirty : 1;
} IdeLspHighlighterPrivate;

static void highlighter_iface_init           (IdeHighlighterInterface *iface);
static void ide_lsp_highlighter_queue_update (IdeLspHighlighter       *self);

G_DEFINE_TYPE_WITH_CODE (IdeLspHighlighter, ide_lsp_highlighter, IDE_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeLspHighlighter)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HIGHLIGHTER, highlighter_iface_init))

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_lsp_highlighter_set_index (IdeLspHighlighter *self,
                               IdeHighlightIndex *index)
{
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_HIGHLIGHTER (self));

  g_clear_pointer (&priv->index, ide_highlight_index_unref);
  if (index != NULL)
    priv->index = ide_highlight_index_ref (index);

  if (priv->engine != NULL)
    {
      if (priv->index != NULL)
        ide_highlight_engine_rebuild (priv->engine);
      else
        ide_highlight_engine_clear (priv->engine);
    }

  IDE_EXIT;
}

static void
ide_lsp_highlighter_document_symbol_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(IdeLspHighlighter) self = user_data;
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(GError) error = NULL;
  GVariantIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_LSP_HIGHLIGHTER (self));

  priv->active = FALSE;

  if (!ide_lsp_client_call_finish (client, result, &return_value, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("%s", error->message);
      IDE_EXIT;
    }

  /* TODO: We should get the tag to have the proper name based on the type. */

  if (g_variant_iter_init (&iter, return_value))
    {
      g_autoptr(IdeHighlightIndex) index = ide_highlight_index_new ();
      GVariant *member = NULL;

      while (g_variant_iter_loop (&iter, "v", &member))
        {
          const gchar *name = NULL;
          const gchar *tag = NULL;
          gboolean success;
          IdeSymbolKind symkind;
          gint64 kind = 0;

          success = JSONRPC_MESSAGE_PARSE (member,
            "name", JSONRPC_MESSAGE_GET_STRING (&name),
            "kind", JSONRPC_MESSAGE_GET_INT64 (&kind)
          );

          if (!success)
            {
              IDE_TRACE_MSG ("Failed to unwrap name and kind from symbol");
              continue;
            }

          symkind = ide_lsp_decode_symbol_kind (kind);

          if ((tag = priv->style_map[symkind]))
            {
              ide_highlight_index_insert (index, name, (gpointer)tag);
              continue;
            }

          switch (kind)
            {
            case 6:   /* METHOD */
            case 12:  /* FUNCTION */
            case 9:   /* CONSTRUCTOR */
              tag = "def:function";
              break;

            case 2:   /* MODULE */
            case 3:   /* NAMESPACE */
            case 4:   /* PACKAGE */
            case 5:   /* CLASS */
            case 10:  /* ENUM */
            case 11:  /* INTERFACE */
              tag = "def:type";
              break;

            case 14:  /* CONSTANT */
              tag = "def:constant";
              break;

            case 7:   /* PROPERTY */
            case 8:   /* FIELD */
            case 13:  /* VARIABLE */
              tag = "def:identifier";
              break;

            default:
              tag = NULL;
            }

          if (tag != NULL)
            ide_highlight_index_insert (index, name, (gpointer)tag);
        }

      ide_lsp_highlighter_set_index (self, index);
    }

  if (priv->dirty)
    ide_lsp_highlighter_queue_update (self);

  IDE_EXIT;
}

static gboolean
ide_lsp_highlighter_update_symbols (gpointer data)
{
  IdeLspHighlighter *self = data;
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  g_assert (IDE_IS_LSP_HIGHLIGHTER (self));

  priv->queued_update = 0;

  if (priv->client != NULL && priv->engine != NULL)
    {
      g_autoptr(GVariant) params = NULL;
      g_autofree gchar *uri = NULL;
      IdeBuffer *buffer;

      buffer = ide_highlight_engine_get_buffer (priv->engine);
      uri = ide_buffer_dup_uri (buffer);

      params = JSONRPC_MESSAGE_NEW (
        "textDocument", "{",
          "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
        "}"
      );

      priv->active = TRUE;
      priv->dirty = FALSE;

      ide_lsp_client_call_async (priv->client,
                                      "textDocument/documentSymbol",
                                      params,
                                      NULL,
                                      ide_lsp_highlighter_document_symbol_cb,
                                      g_object_ref (self));
    }

  return G_SOURCE_REMOVE;
}

static void
ide_lsp_highlighter_queue_update (IdeLspHighlighter *self)
{
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_HIGHLIGHTER (self));

  priv->dirty = TRUE;

  /*
   * Queue an update to get the newest symbol list (which we'll use to build
   * the highlight index).
   */

  if (priv->queued_update == 0 && priv->active == FALSE)
    priv->queued_update = g_timeout_add (DELAY_TIMEOUT_MSEC,
                                         ide_lsp_highlighter_update_symbols,
                                         self);

  IDE_EXIT;
}

static void
ide_lsp_highlighter_buffer_line_flags_changed (IdeLspHighlighter *self,
                                               IdeBuffer         *buffer)
{
  g_assert (IDE_IS_LSP_HIGHLIGHTER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_lsp_highlighter_queue_update (self);
}

static void
ide_lsp_highlighter_destroy (IdeObject *object)
{
  IdeLspHighlighter *self = (IdeLspHighlighter *)object;
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  g_assert (IDE_IS_LSP_HIGHLIGHTER (self));

  priv->engine = NULL;

  g_clear_handle_id (&priv->queued_update, g_source_remove);

  g_clear_pointer (&priv->index, ide_highlight_index_unref);
  g_clear_object (&priv->buffer_signals);
  g_clear_object (&priv->client);

  IDE_OBJECT_CLASS (ide_lsp_highlighter_parent_class)->destroy (object);
}

static void
ide_lsp_highlighter_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeLspHighlighter *self = IDE_LSP_HIGHLIGHTER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_highlighter_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_highlighter_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeLspHighlighter *self = IDE_LSP_HIGHLIGHTER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      ide_lsp_highlighter_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_highlighter_class_init (IdeLspHighlighterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_lsp_highlighter_get_property;
  object_class->set_property = ide_lsp_highlighter_set_property;

  i_object_class->destroy = ide_lsp_highlighter_destroy;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "Client",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_highlighter_init (IdeLspHighlighter *self)
{
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  priv->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);

  /*
   * We sort of cheat here by using ::line-flags-changed instead of :;changed
   * because it signifies to us that a diagnostics query has come back from the
   * language server and therefore we are more likely to get a valid response
   * for the documentation lookup. Otherwise, we can often get in a situation
   * where the language server gives us an empty set back (or at least with
   * the rust language server).
   */
  g_signal_group_connect_object (priv->buffer_signals,
                                   "line-flags-changed",
                                   G_CALLBACK (ide_lsp_highlighter_buffer_line_flags_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
}

/**
 * ide_lsp_highlighter_get_client:
 *
 * Returns: (transfer none) (nullable): An #IdeLspHighlighter or %NULL.
 */
IdeLspClient *
ide_lsp_highlighter_get_client (IdeLspHighlighter *self)
{
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_HIGHLIGHTER (self), NULL);

  return priv->client;
}

void
ide_lsp_highlighter_set_client (IdeLspHighlighter *self,
                                IdeLspClient      *client)
{
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_HIGHLIGHTER (self));
  g_return_if_fail (!client || IDE_IS_LSP_CLIENT (client));

  if (g_set_object (&priv->client, client))
    {
      ide_lsp_highlighter_queue_update (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
    }
}

static inline gboolean
accepts_char (gunichar ch)
{
  return (ch == '_' || g_unichar_isalnum (ch));
}

static inline gboolean
select_next_word (GtkTextIter *begin,
                  GtkTextIter *end)
{

  g_assert (begin != NULL);
  g_assert (end != NULL);

  *end = *begin;

  while (!accepts_char (gtk_text_iter_get_char (begin)))
    if (!gtk_text_iter_forward_char (begin))
      return FALSE;

  *end = *begin;

  while (accepts_char (gtk_text_iter_get_char (end)))
    if (!gtk_text_iter_forward_char (end))
      return !gtk_text_iter_equal (begin, end);

  return TRUE;
}

static void
remove_tags (const GtkTextIter *begin,
             const GtkTextIter *end,
             const GSList      *tags_to_remove)
{
  GtkTextBuffer *buffer;

  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (tags_to_remove == NULL)
    return;

  buffer = gtk_text_iter_get_buffer (begin);
  for (const GSList *iter = tags_to_remove; iter; iter = iter->next)
    gtk_text_buffer_remove_tag (buffer, iter->data, begin, end);
}

static void
ide_lsp_highlighter_update (IdeHighlighter       *highlighter,
                            const GSList         *tags_to_remove,
                            IdeHighlightCallback  callback,
                            const GtkTextIter    *range_begin,
                            const GtkTextIter    *range_end,
                            GtkTextIter          *location)
{
  IdeLspHighlighter *self = (IdeLspHighlighter *)highlighter;
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);
  GtkSourceBuffer *source_buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_LSP_HIGHLIGHTER (self));
  g_assert (callback != NULL);

  if (priv->index == NULL)
    {
      *location = *range_end;
      return;
    }

  source_buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (range_begin));

  begin = end = *location = *range_begin;

  while (gtk_text_iter_compare (&begin, range_end) < 0)
    {
      GtkTextIter last = begin;

      if (!select_next_word (&begin, &end))
        {
          remove_tags (&last, range_end, tags_to_remove);
          goto completed;
        }

      remove_tags (&last, &end, tags_to_remove);

      if (gtk_text_iter_compare (&begin, range_end) >= 0)
        goto completed;

      g_assert (!gtk_text_iter_equal (&begin, &end));

      if (!gtk_source_buffer_iter_has_context_class (source_buffer, &begin, "string") &&
          !gtk_source_buffer_iter_has_context_class (source_buffer, &begin, "path") &&
          !gtk_source_buffer_iter_has_context_class (source_buffer, &begin, "comment"))
        {
          const gchar *tag;
          gchar *word;

          word = gtk_text_iter_get_slice (&begin, &end);
          tag = ide_highlight_index_lookup (priv->index, word);
          g_free (word);

          if (tag != NULL)
            {
              if (callback (&begin, &end, tag) == IDE_HIGHLIGHT_STOP)
                {
                  *location = end;
                  return;
                }
            }
        }

      begin = end;
    }

completed:
  *location = *range_end;
}

static void
ide_lsp_highlighter_set_engine (IdeHighlighter     *highlighter,
                                IdeHighlightEngine *engine)
{
  IdeLspHighlighter *self = (IdeLspHighlighter *)highlighter;
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_HIGHLIGHTER (self));
  g_assert (!engine || IDE_IS_HIGHLIGHT_ENGINE (engine));

  priv->engine = engine;

  g_signal_group_set_target (priv->buffer_signals, NULL);

  if (engine != NULL)
    {
      IdeBuffer *buffer;

      buffer = ide_highlight_engine_get_buffer (engine);
      g_signal_group_set_target (priv->buffer_signals, buffer);
      ide_lsp_highlighter_queue_update (self);
    }

  IDE_EXIT;
}

static void
highlighter_iface_init (IdeHighlighterInterface *iface)
{
  iface->update = ide_lsp_highlighter_update;
  iface->set_engine = ide_lsp_highlighter_set_engine;
}

void
ide_lsp_highlighter_set_kind_style (IdeLspHighlighter *self,
                                    IdeSymbolKind      kind,
                                    const gchar       *style)
{
  IdeLspHighlighterPrivate *priv = ide_lsp_highlighter_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_HIGHLIGHTER (self));
  g_return_if_fail (kind < IDE_SYMBOL_KIND_LAST);

  priv->style_map[kind] = g_intern_string (style);
}
