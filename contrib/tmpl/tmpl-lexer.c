/* tmpl-lexer.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "tmpl-lexer"

#include "tmpl-error.h"
#include "tmpl-debug.h"
#include "tmpl-lexer.h"
#include "tmpl-template-locator.h"
#include "tmpl-token-input-stream.h"

struct _TmplLexer
{
  GQueue               *stream_stack;
  TmplTemplateLocator  *locator;
  GHashTable           *circular;
  GSList               *unget;
};

G_DEFINE_POINTER_TYPE (TmplLexer, tmpl_lexer)

TmplLexer *
tmpl_lexer_new (GInputStream        *stream,
                TmplTemplateLocator *locator)
{
  TmplLexer *self;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (!locator || TMPL_IS_TEMPLATE_LOCATOR (locator), NULL);

  self = g_slice_new0 (TmplLexer);
  self->stream_stack = g_queue_new ();
  self->locator = locator ? g_object_ref (locator) : tmpl_template_locator_new ();
  self->circular = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  g_queue_push_head (self->stream_stack, tmpl_token_input_stream_new (stream));

  return self;
}

void
tmpl_lexer_free (TmplLexer *self)
{
  if (self != NULL)
    {
      const GList *iter;

      for (iter = self->stream_stack->head; iter != NULL; iter = iter->next)
        {
          TmplTokenInputStream *stream = iter->data;

          g_object_unref (stream);
        }

      g_clear_pointer (&self->circular, g_hash_table_unref);
      g_clear_pointer (&self->stream_stack, g_queue_free);
      g_clear_object (&self->locator);
      g_slice_free (TmplLexer, self);
    }
}

/**
 * tmpl_lexer_next:
 * @self: A #TmplLexer.
 * @token: (out) (transfer full): A location for a #TmplToken.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: A location for a #GError or %NULL.
 *
 * Reads the next token.
 *
 * It is possible for %FALSE to be returned and @error left unset.
 * This happens at the end of the stream.
 *
 * Returns: %TRUE if @token was set, otherwise %FALSE.
 */
gboolean
tmpl_lexer_next (TmplLexer     *self,
                 TmplToken    **token,
                 GCancellable  *cancellable,
                 GError       **error)
{
  TmplTokenInputStream *stream;
  GError *local_error = NULL;
  gboolean ret = FALSE;

  TMPL_ENTRY;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (token != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  *token = NULL;

  if (self->unget != NULL)
    {
      *token = self->unget->data;
      self->unget = g_slist_remove_link (self->unget, self->unget);
      TMPL_RETURN (TRUE);
    }

  while ((stream = g_queue_peek_head (self->stream_stack)))
    {
      if (!(*token = tmpl_token_input_stream_read_token (stream, cancellable, &local_error)))
        {
          /*
           * If we finished this stream, try to move to the next one.
           */
          if (local_error == NULL)
            {
              g_queue_pop_head (self->stream_stack);
              g_object_unref (stream);
              continue;
            }

          TMPL_GOTO (finish);
        }

      /*
       * If the current token is an include token, we need to resolve the
       * include path and read tokens from it.
       */
      if (tmpl_token_type (*token) == TMPL_TOKEN_INCLUDE)
        {
          const gchar *path = tmpl_token_include_get_path (*token);
          GInputStream *input;

          g_assert (self->circular != NULL);
          g_assert (path != NULL);

          if (g_hash_table_contains (self->circular, path))
            {
              local_error = g_error_new (TMPL_ERROR,
                                         TMPL_ERROR_CIRCULAR_INCLUDE,
                                         "A circular include was detected: \"%s\"",
                                         path);
              g_clear_pointer (token, tmpl_token_free);
              TMPL_GOTO (finish);
            }

          if (!(input = tmpl_template_locator_locate (self->locator, path, &local_error)))
            {
              g_clear_pointer (token, tmpl_token_free);
              TMPL_GOTO (finish);
            }

          g_hash_table_insert (self->circular, g_strdup (path), NULL);

          stream = tmpl_token_input_stream_new (input);
          g_queue_push_head (self->stream_stack, stream);

          g_clear_pointer (token, tmpl_token_free);
          g_object_unref (input);

          continue;
        }

      ret = TRUE;
      break;
    }

  if (*token == NULL)
    {
      *token = tmpl_token_new_eof ();
      ret = TRUE;
    }

finish:
  if ((ret == FALSE) && (local_error != NULL))
    g_propagate_error (error, local_error);

  g_assert (ret == FALSE || *token != NULL);

  TMPL_RETURN (ret);
}

void
tmpl_lexer_unget (TmplLexer *self,
                  TmplToken *token)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (token != NULL);

  self->unget = g_slist_prepend (self->unget, token);
}
