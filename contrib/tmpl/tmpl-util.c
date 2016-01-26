/* tmpl-util-private.c
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

#include <glib-object.h>

#include "tmpl-gi-private.h"
#include "tmpl-util-private.h"

static gboolean
destroy_in_main_context_cb (gpointer data)
{
  struct {
    gpointer       data;
    GDestroyNotify destroy;
  } *state = data;

  g_assert (state != NULL);
  g_assert (state->data != NULL);
  g_assert (state->destroy != NULL);

  state->destroy (state->data);
  g_slice_free1 (sizeof *state, state);

  return G_SOURCE_REMOVE;
}

void
tmpl_destroy_in_main_context (GMainContext   *main_context,
                              gpointer        data,
                              GDestroyNotify  destroy)
{
  GSource *idle;
  struct {
    gpointer       data;
    GDestroyNotify destroy;
  } *state;

  g_assert (main_context != NULL);
  g_assert (data != NULL);
  g_assert (destroy != NULL);

  state = g_slice_alloc (sizeof *state);
  state->data = data;
  state->destroy = destroy;

  idle = g_idle_source_new ();
  g_source_set_callback (idle, destroy_in_main_context_cb, state, NULL);
  g_source_attach (idle, main_context);
}

gchar *
tmpl_value_repr (const GValue *value)
{
  GValue coerced = G_VALUE_INIT;
  gchar *ret = NULL;

  g_return_val_if_fail (value != NULL, NULL);

  if (G_VALUE_TYPE (value) != G_TYPE_INVALID)
    {
      g_value_init (&coerced, G_TYPE_STRING);

      if (G_VALUE_HOLDS_BOOLEAN (value))
        {
          ret = g_strdup (g_value_get_boolean (value) ? "true" : "false");
        }
      else if (G_VALUE_HOLDS_STRING (value) && g_value_get_string (value))
        {
          gchar *escaped;

          escaped = g_strescape (g_value_get_string (value), NULL);
          ret = g_strdup_printf ("\"%s\"", escaped);
          g_free (escaped);
        }
      else if (G_VALUE_HOLDS (value, TMPL_TYPE_TYPELIB))
        {
          GITypelib *tl = g_value_get_pointer (value);

          if (tl != NULL)
            {
              const gchar *ns = g_typelib_get_namespace (tl);
              ret = g_strdup_printf ("<Namespace \"%s\">", ns);
            }
          else
            {
              ret = g_strdup_printf ("<Namespace at %p>", tl);
            }
        }
      else if (g_value_transform (value, &coerced))
        ret = g_value_dup_string (&coerced);
      else if (G_VALUE_HOLDS_OBJECT (value))
        {
          GObject *obj = g_value_get_object (value);

          ret = g_strdup_printf ("<%s at %p>",
                                 obj ? G_OBJECT_TYPE_NAME (obj)
                                     : G_VALUE_TYPE_NAME (value),
                                 obj);
        }
      else if (G_VALUE_HOLDS_BOXED (value))
        ret = g_strdup_printf ("<%s at %p>",
                               G_VALUE_TYPE_NAME (value),
                               g_value_get_boxed (value));
      else
        ret = g_strdup_printf ("<%s>", G_VALUE_TYPE_NAME (value));

      g_value_unset (&coerced);
    }

  return ret;
}

gboolean
tmpl_value_as_boolean (const GValue *value)
{
  gboolean ret = FALSE;

  if (value != NULL && G_VALUE_TYPE (value) != G_TYPE_INVALID)
    {
      GValue coerced = G_VALUE_INIT;

      g_value_init (&coerced, G_TYPE_BOOLEAN);

      if (!g_value_transform (value, &coerced))
        {
          if (G_VALUE_HOLDS_STRING (value))
            ret = g_value_get_string (value) && *g_value_get_string (value);
          else if (G_VALUE_HOLDS_DOUBLE (value))
            ret = g_value_get_double (value) != 0.0;
          else if (G_VALUE_HOLDS_INT (value))
            ret = !!g_value_get_int (value);
          else if (G_VALUE_HOLDS_UINT (value))
            ret = !!g_value_get_uint (value);
          else if (G_VALUE_HOLDS_INT64 (value))
            ret = !!g_value_get_int64 (value);
          else if (G_VALUE_HOLDS_UINT64 (value))
            ret = !!g_value_get_uint64 (value);
          else if (G_VALUE_HOLDS_LONG (value))
            ret = !!g_value_get_long (value);
          else if (G_VALUE_HOLDS_ULONG (value))
            ret = !!g_value_get_ulong (value);
          else if (G_VALUE_HOLDS_FLOAT (value))
            ret = g_value_get_float (value) != 0.0;
          else if (G_VALUE_HOLDS_BOXED (value))
            ret = !!g_value_get_boxed (value);
          else if (G_VALUE_HOLDS_OBJECT (value))
            ret = !!g_value_get_object (value);
          else if (G_VALUE_HOLDS_VARIANT (value))
            ret = !!g_value_get_variant (value);
        }
      else
        ret = g_value_get_boolean (&coerced);
    }

  return ret;
}
