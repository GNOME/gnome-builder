/* tmpl-token.c
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>

#include "tmpl-token.h"

struct _TmplToken
{
  TmplTokenType type;
  gchar *text;
};

static TmplToken *
tmpl_token_new (void)
{
  TmplToken *self;

  self = g_slice_new0 (TmplToken);

  return self;
}

void
tmpl_token_free (TmplToken *self)
{
  if (self != NULL)
    {
      g_free (self->text);
      g_slice_free (TmplToken, self);
    }
}

TmplToken *
tmpl_token_new_eof (void)
{
  TmplToken *ret;

  ret = tmpl_token_new ();
  ret->type = TMPL_TOKEN_EOF;

  return ret;
}

/**
 * tmpl_token_new_text:
 * @text: (transfer full): The text for the token.
 *
 * Creates a new #TmplToken containing @text.
 * This is a text literal type.
 *
 * Returns: (transfer full): A newly allocated #TmplToken.
 */
TmplToken *
tmpl_token_new_text (gchar *text)
{
  TmplToken *self;

  self = tmpl_token_new ();
  self->type = TMPL_TOKEN_TEXT;
  self->text = text;

  return self;
}

TmplToken *
tmpl_token_new_unichar (gunichar ch)
{
  gchar utf8[8];
  gint len;

  len = g_unichar_to_utf8 (ch, utf8);
  utf8 [len] = '\0';

  return tmpl_token_new_text (g_strdup (utf8));
}

TmplTokenType
tmpl_token_type (TmplToken *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->type;
}

gchar *
tmpl_token_include_get_path (TmplToken *self)
{
  char *path = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->type == TMPL_TOKEN_INCLUDE, NULL);

  if (1 == sscanf (self->text, "include \"%m[^\"]", &path))
    {
      gchar *tmp = g_strdup (path);
      free (path);
      return tmp;
    }

  return NULL;
}

TmplToken *
tmpl_token_new_generic (gchar *text)
{
  TmplToken *self;

  g_return_val_if_fail (text != NULL, NULL);

  self = g_slice_new0 (TmplToken);

  if (g_str_has_prefix (text, "if "))
    {
      self->type = TMPL_TOKEN_IF;
      self->text = g_strstrip (g_strdup (text + 3));
    }
  else if (g_str_has_prefix (text, "else if "))
    {
      self->type = TMPL_TOKEN_ELSE_IF;
      self->text = g_strstrip (g_strdup (text + 8));
    }
  else if (g_str_has_prefix (text, "else"))
    {
      self->type = TMPL_TOKEN_ELSE;
      self->text = NULL;
    }
  else if (g_str_has_prefix (text, "end"))
    {
      self->type = TMPL_TOKEN_END;
      self->text = NULL;
    }
  else if (g_str_has_prefix (text, "for "))
    {
      self->type = TMPL_TOKEN_FOR;
      self->text = g_strstrip (g_strdup (text + 4));
    }
  else if (g_str_has_prefix (text, "include "))
    {
      self->type = TMPL_TOKEN_INCLUDE;
      self->text = g_strstrip (g_strdup (text));
    }
  else
    {
      self->type = TMPL_TOKEN_EXPRESSION;
      self->text = g_strstrip (text);
      text = NULL;
    }

  g_free (text);

  return self;
}

const gchar *
tmpl_token_get_text (TmplToken *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->text;
}
