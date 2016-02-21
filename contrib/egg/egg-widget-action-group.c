/* egg-widget-action-group.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include <string.h>

#include "egg-widget-action-group.h"

static gboolean
supports_types (const GType *types,
                guint        n_types)
{
  guint i;

  g_assert (types != NULL || n_types == 0);

  for (i = 0; i < n_types; i++)
    {
      switch (types [i])
        {
        case G_TYPE_STRING:
        case G_TYPE_INT:
        case G_TYPE_UINT:
        case G_TYPE_INT64:
        case G_TYPE_UINT64:
        case G_TYPE_BOOLEAN:
        case G_TYPE_DOUBLE:
        case G_TYPE_FLOAT:
        case G_TYPE_CHAR:
        case G_TYPE_UCHAR:
        case G_TYPE_ENUM:
        case G_TYPE_FLAGS:
        case G_TYPE_VARIANT:
        case G_TYPE_NONE:
          break;

        default:
          if (G_TYPE_IS_FLAGS (types [i]) || G_TYPE_IS_ENUM (types [i]))
            break;

          return FALSE;
        }
    }

  return TRUE;
}

static GVariantType *
create_variant_type (const GType *types,
                     guint        n_types)
{
  GString *str;
  gint i;

  g_assert (types != NULL || n_types == 0);

  str = g_string_new ("(");

  for (i = 0; i < n_types; i++)
    {
      switch (types [i])
        {
        case G_TYPE_STRING:
          g_string_append_c (str, 's');
          break;

        case G_TYPE_INT:
          g_string_append_c (str, 'i');
          break;

        case G_TYPE_UINT:
          g_string_append_c (str, 'u');
          break;

        case G_TYPE_INT64:
          g_string_append_c (str, 'x');
          break;

        case G_TYPE_UINT64:
          g_string_append_c (str, 't');
          break;

        case G_TYPE_BOOLEAN:
          g_string_append_c (str, 'b');
          break;

        case G_TYPE_DOUBLE:
        case G_TYPE_FLOAT:
          g_string_append_c (str, 'd');
          break;

        case G_TYPE_CHAR:
        case G_TYPE_UCHAR:
          g_string_append_c (str, 'y');
          break;

        case G_TYPE_VARIANT:
          g_string_append_c (str, 'v');
          break;

        case G_TYPE_NONE:
          break;

        default:
          if (G_TYPE_IS_ENUM (types [i]) || G_TYPE_IS_FLAGS (types [i]))
            {
              g_string_append_c (str, 'u');
              break;
            }

          return FALSE;
        }
    }

  g_string_append_c (str, ')');

  return (GVariantType *)g_string_free (str, (str->len == 2));
}

static void
egg_widget_action_group_activate (GSimpleAction *action,
                                  GVariant      *params,
                                  GtkWidget     *widget)
{
  const GSignalQuery *query;
  g_auto(GValue) return_value = G_VALUE_INIT;
  g_auto(GValue) instance = G_VALUE_INIT;
  GArray *ar;
  GVariantIter iter;
  gsize n_children;
  gint i;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GTK_IS_WIDGET (widget));

  query = g_object_get_data (G_OBJECT (action), "EGG_SIGNAL_INFO");

  if (query == NULL)
    {
      g_critical ("EGG_SIGNAL_INFO is missing, cannot emit signal.");
      return;
    }

  if (params)
    g_print ("Activating %s with %s\n", query->signal_name, g_variant_print (params, TRUE));

  if (params == NULL && query->n_params != 0)
    {
      g_critical ("%s::%s() requires %d parameters",
                  G_OBJECT_TYPE_NAME (widget), query->signal_name, query->n_params);
      return;
    }

  if (query->return_type != G_TYPE_NONE)
    g_value_init (&return_value, query->return_type);

  g_value_init (&instance, query->itype);
  g_value_set_object (&instance, widget);

  if (params == NULL)
    {
      g_signal_emitv (&instance, query->signal_id, 0, &return_value);
      return;
    }

  g_assert (g_variant_is_container (params));
  g_assert (params != NULL);

  n_children = g_variant_iter_init (&iter, params);

  if (n_children != query->n_params)
    {
      g_critical ("%s::%s() requires %d params, got %d",
                  G_OBJECT_TYPE_NAME (widget), query->signal_name,
                  (gint)n_children, query->n_params);
      return;
    }

  ar = g_array_new (FALSE, FALSE, sizeof (GValue));

  g_array_append_val (ar, instance);

  g_variant_iter_init (&iter, params);

  for (i = 0; i < query->n_params; i++)
    {
      g_autoptr(GVariant) param = NULL;
      GValue value = G_VALUE_INIT;

      param = g_variant_iter_next_value (&iter);

#define CONVERT_PARAM(TYPE, VARIANT_TYPE, setter, getter, ...) \
  case G_TYPE_##TYPE: \
    { \
      if (!g_variant_is_of_type (param, G_VARIANT_TYPE_##VARIANT_TYPE)) \
        { \
          g_critical ("parameter type mismatch for signal %s", \
                      query->signal_name); \
          goto skip_emit; \
        } \
      g_value_init (&value, G_TYPE_##TYPE); \
      g_value_set_##setter (&value, g_variant_get_##getter (param, ##__VA_ARGS__)); \
      g_array_append_val (ar, value); \
    } \
    break

      switch (query->param_types [i])
        {
        CONVERT_PARAM(STRING, STRING, string, string, NULL);
        CONVERT_PARAM(INT, INT32, int, int32);
        CONVERT_PARAM(UINT, UINT32, uint, uint32);
        CONVERT_PARAM(INT64, INT64, int64, int64);
        CONVERT_PARAM(UINT64, UINT64, uint64, uint64);
        CONVERT_PARAM(BOOLEAN, BOOLEAN, boolean, boolean);
        CONVERT_PARAM(DOUBLE, DOUBLE, double, double);
        CONVERT_PARAM(FLOAT, DOUBLE, float, double);
        CONVERT_PARAM(CHAR, BYTE, schar, byte);
        CONVERT_PARAM(UCHAR, BYTE, uchar, byte);
        CONVERT_PARAM(VARIANT, VARIANT, variant, variant);

        default:
          if (G_TYPE_IS_ENUM(query->param_types [i]))
            {
              if (!g_variant_is_of_type (param, G_VARIANT_TYPE_UINT32))
                goto skip_emit;
              g_value_init (&value, query->param_types [i]);
              g_value_set_enum (&value, g_variant_get_uint32 (param));
              g_array_append_val (ar, value);
              break;
            }
          else if (G_TYPE_IS_FLAGS (query->param_types [i]))
            {
              if (!g_variant_is_of_type (param, G_VARIANT_TYPE_UINT32))
                goto skip_emit;
              g_value_init (&value, query->param_types [i]);
              g_value_set_flags (&value, g_variant_get_uint32 (param));
              g_array_append_val (ar, value);
              break;
            }

          g_critical ("Unknown param type: %s", g_type_name (query->param_types [i]));
          goto skip_emit;
        }

#undef CONVERT_PARAM
    }

  g_signal_emitv ((GValue *)ar->data, query->signal_id, 0, &return_value);

skip_emit:
  /* ignore instance */
  for (i = 1; i < ar->len; i++)
    g_value_unset (&g_array_index (ar, GValue, i));

  g_array_unref (ar);
}

static void
query_free (gpointer data)
{
  g_slice_free (GSignalQuery, data);
}

GAction *
create_action (const GSignalQuery *query,
               GtkWidget          *widget)
{
  GSimpleAction *action;
  GVariantType *param_type;
  GSignalQuery *query_copy;

  g_assert (query != NULL);
  g_assert (query->signal_id != 0);
  g_assert (GTK_IS_WIDGET (widget));

  param_type = create_variant_type (query->param_types, query->n_params);
  action = g_simple_action_new (query->signal_name, param_type);

  /* Save signal info for marshalling upon callback */
  query_copy = g_slice_new0 (GSignalQuery);
  memcpy (query_copy, query, sizeof *query_copy);
  g_object_set_data_full (G_OBJECT (action), "EGG_SIGNAL_INFO", query_copy, query_free);

  /* connect our marshaller to the action */
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (egg_widget_action_group_activate),
                           widget,
                           0);

  g_free (param_type);

  return G_ACTION (action);
}

/**
 * egg_widget_action_group_new:
 * @widget: A #GtkWidget
 *
 * Creates a new #GActionGroup that can proxy signal actions
 * to @widget.
 *
 * Returns: (transfer full): A newly allocated #GActionGroup.
 */
GActionGroup *
egg_widget_action_group_new (GtkWidget *widget)
{
  GSimpleActionGroup *self;
  GType type;
  guint *signals;
  guint n_signals = 0;
  guint i;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  self = g_simple_action_group_new ();

  for (type = G_OBJECT_TYPE (widget);
       type != G_TYPE_INITIALLY_UNOWNED;
       type = g_type_parent (type))
    {
      signals = g_signal_list_ids (type, &n_signals);

      for (i = 0; i < n_signals; i++)
        {
          GSignalQuery query;
          GAction *action;

          g_signal_query (signals [i], &query);

          if ((query.signal_flags & G_SIGNAL_ACTION) == 0)
            continue;

          if (!supports_types (&query.return_type, 1))
            continue;

          if (!supports_types (query.param_types, query.n_params))
            continue;

          action = create_action (&query, widget);
          g_action_map_add_action (G_ACTION_MAP (self), action);
          g_object_unref (action);
        }

      g_free (signals);
    }

  return G_ACTION_GROUP (self);
}

void
egg_widget_action_group_attach (gpointer     instance,
                                const gchar *name)
{
  GActionGroup *group;

  g_return_if_fail (GTK_IS_WIDGET (instance));
  g_return_if_fail (name != NULL);

  group = egg_widget_action_group_new (instance);
  gtk_widget_insert_action_group (instance, name, group);
  g_object_unref (group);
}
