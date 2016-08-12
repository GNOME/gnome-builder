/* tmpl-iterator.c
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

#include <gio/gio.h>
#include <string.h>

#include "tmpl-iterator.h"

typedef gboolean (*GetValue) (TmplIterator *iter,
                              GValue       *value);
typedef gboolean (*MoveNext) (TmplIterator *iter);
typedef void     (*Destroy)  (TmplIterator *iter);

static gboolean
string_move_next (TmplIterator *iter)
{
  if (iter->instance)
    {
      iter->instance = g_utf8_next_char ((gchar *)iter->instance);
      return (*(gchar *)iter->instance) != 0;
    }

  return FALSE;
}

static gboolean
string_get_value (TmplIterator *iter,
                  GValue       *value)
{
  if (iter->instance)
    {
      gunichar ch = g_utf8_get_char ((gchar *)iter->instance);
      gchar str[8];

      str [g_unichar_to_utf8 (ch, str)] = '\0';
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, str);

      return TRUE;
    }

  return FALSE;
}

static gboolean
list_model_move_next (TmplIterator *iter)
{
  guint index = GPOINTER_TO_INT (iter->data1);
  guint n_items = GPOINTER_TO_INT (iter->data2);

  index++;

  /* We are 1 based indexing here */
  if (index <= n_items)
    {
      iter->data1 = GINT_TO_POINTER (index);
      return TRUE;
    }

  return FALSE;
}

static gboolean
list_model_get_value (TmplIterator *iter,
                      GValue       *value)
{
  guint index = GPOINTER_TO_INT (iter->data1);
  GObject *obj;

  g_return_val_if_fail (index > 0, FALSE);

  obj = g_list_model_get_item (iter->instance, index - 1);

  g_value_init (value, g_list_model_get_item_type (iter->instance));

  if (obj != NULL)
    g_value_take_object (value, obj);

  return TRUE;
}

void
tmpl_iterator_init (TmplIterator *iter,
                    const GValue *value)
{
  memset (iter, 0, sizeof *iter);

  if (value == NULL)
    return;

  if (G_VALUE_HOLDS_STRING (value))
    {
      iter->instance = (gchar *)g_value_get_string (value);
      iter->move_next = string_move_next;
      iter->get_value = string_get_value;
      iter->destroy = NULL;
    }
  else if (G_VALUE_HOLDS (value, G_TYPE_LIST_MODEL))
    {
      iter->instance = g_value_get_object (value);
      iter->move_next = list_model_move_next;
      iter->get_value = list_model_get_value;
      iter->destroy = NULL;

      if (iter->instance != NULL)
        {
          guint n_items;

          n_items = g_list_model_get_n_items (iter->instance);
          iter->data1 = GUINT_TO_POINTER (iter->data1);
          iter->data2 = GUINT_TO_POINTER (n_items);
        }
    }

  /* TODO: More iter types */
}

gboolean
tmpl_iterator_next (TmplIterator *iter)
{
  if (iter == NULL || iter->move_next == NULL)
    return FALSE;

  return ((MoveNext)iter->move_next) (iter);
}

void
tmpl_iterator_get_value (TmplIterator *iter,
                         GValue       *value)
{
  ((GetValue)iter->get_value) (iter, value);
}

void
tmpl_iterator_destroy (TmplIterator *iter)
{
  if (iter->destroy != NULL)
    ((Destroy)(iter->destroy)) (iter);
  memset (iter, 0, sizeof *iter);
}
