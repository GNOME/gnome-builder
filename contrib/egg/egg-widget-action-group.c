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

#define G_LOG_DOMAIN "egg-widget-action-group"

#include <string.h>

#include "egg-widget-action-group.h"

struct _EggWidgetActionGroup
{
  GObject     parent_instance;
  GtkWidget  *widget;
  GHashTable *enabled;
};

static void action_group_iface_init (GActionGroupInterface *iface);

G_DEFINE_TYPE_EXTENDED (EggWidgetActionGroup, egg_widget_action_group, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, action_group_iface_init))

enum {
  PROP_0,
  PROP_WIDGET,
  N_PROPS
};

static GHashTable *cached_types;
static GParamSpec *properties [N_PROPS];

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

static const GVariantType *
create_variant_type (const GType *types,
                     guint        n_types)
{
  const GVariantType *ret = NULL;
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

  if (g_str_equal (str->str, "()"))
    {
      g_string_free (str, TRUE);
      return NULL;
    }

  if (cached_types == NULL)
    cached_types = g_hash_table_new (g_str_hash, g_str_equal);

  ret = g_hash_table_lookup (cached_types, str->str);

  if (ret == NULL)
    {
      gchar *type_str = g_string_free (str, FALSE);
      g_hash_table_insert (cached_types, type_str, type_str);
      ret = (const GVariantType *)type_str;
    }

  return ret;
}

static void
do_activate (EggWidgetActionGroup *self,
             GtkWidget            *widget,
             GSignalQuery         *query,
             GVariant             *params)
{
  g_auto(GValue) return_value = G_VALUE_INIT;
  g_auto(GValue) instance = G_VALUE_INIT;
  GArray *ar;
  GVariantIter iter;
  gsize n_children;
  gint i;

  g_assert (query != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  if (params != NULL)
    g_debug ("Activating %s with %s\n", query->signal_name, g_variant_print (params, TRUE));

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

  g_signal_emitv ((GValue *)(gpointer)ar->data, query->signal_id, 0, &return_value);

skip_emit:
  /* ignore instance */
  for (i = 1; i < ar->len; i++)
    g_value_unset (&g_array_index (ar, GValue, i));

  g_array_unref (ar);
}

static void
egg_widget_action_group_set_widget (EggWidgetActionGroup *self,
                                    GtkWidget            *widget)
{
  g_assert (EGG_IS_WIDGET_ACTION_GROUP (self));
  g_assert (!widget || GTK_IS_WIDGET (widget));

  if (widget != self->widget)
    {
      if (self->widget != NULL)
        {
          g_signal_handlers_disconnect_by_func (self->widget,
                                                G_CALLBACK (gtk_widget_destroyed),
                                                &self->widget);
          self->widget = NULL;
        }

      if (widget != NULL)
        {
          self->widget = widget;
          g_signal_connect (self->widget,
                            "destroy",
                            G_CALLBACK (gtk_widget_destroyed),
                            &self->widget);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WIDGET]);
    }
}

static void
egg_widget_action_group_finalize (GObject *object)
{
  EggWidgetActionGroup *self = (EggWidgetActionGroup *)object;

  g_clear_pointer (&self->enabled, g_hash_table_unref);

  G_OBJECT_CLASS (egg_widget_action_group_parent_class)->finalize (object);
}

static void
egg_widget_action_group_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  EggWidgetActionGroup *self = EGG_WIDGET_ACTION_GROUP (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, self->widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_widget_action_group_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  EggWidgetActionGroup *self = EGG_WIDGET_ACTION_GROUP (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      egg_widget_action_group_set_widget (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_widget_action_group_class_init (EggWidgetActionGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = egg_widget_action_group_finalize;
  object_class->get_property = egg_widget_action_group_get_property;
  object_class->set_property = egg_widget_action_group_set_property;

  properties [PROP_WIDGET] =
    g_param_spec_object ("widget",
                         "Widget",
                         "Widget",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
egg_widget_action_group_init (EggWidgetActionGroup *self)
{
}

static gboolean
egg_widget_action_group_has_action (GActionGroup *group,
                                    const gchar  *action_name)
{
  EggWidgetActionGroup *self = (EggWidgetActionGroup *)group;

  g_assert (EGG_IS_WIDGET_ACTION_GROUP (self));
  g_assert (action_name != NULL);

  if (GTK_IS_WIDGET (self->widget))
    return (0 != g_signal_lookup (action_name, G_OBJECT_TYPE (self->widget)));

  return FALSE;
}

static gchar **
egg_widget_action_group_list_actions (GActionGroup *group)
{
  EggWidgetActionGroup *self = (EggWidgetActionGroup *)group;
  GPtrArray *ar;

  g_assert (EGG_IS_WIDGET_ACTION_GROUP (self));

  ar = g_ptr_array_new ();

  if (self->widget != NULL)
    {
      for (GType type = G_OBJECT_TYPE (self->widget);
           type != G_TYPE_INVALID;
           type = g_type_parent (type))
        {
          g_autofree guint *signal_ids = NULL;
          guint n_ids = 0;
          guint i;

          signal_ids = g_signal_list_ids (type, &n_ids);

          for (i = 0; i < n_ids; i++)
            {
              GSignalQuery query;

              g_signal_query (signal_ids[i], &query);

              if ((query.signal_flags & G_SIGNAL_ACTION) != 0)
                g_ptr_array_add (ar, g_strdup (query.signal_name));
            }
        }
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}

static gboolean
egg_widget_action_group_get_action_enabled (GActionGroup *group,
                                            const gchar  *action_name)
{
  EggWidgetActionGroup *self = (EggWidgetActionGroup *)group;

  g_assert (EGG_IS_WIDGET_ACTION_GROUP (group));
  g_assert (action_name != NULL);

  if (self->enabled && g_hash_table_contains (self->enabled, action_name))
    return GPOINTER_TO_INT (g_hash_table_lookup (self->enabled, action_name));

  return TRUE;
}

const GVariantType *
egg_widget_action_group_get_action_parameter_type (GActionGroup *group,
                                                   const gchar  *action_name)
{
  EggWidgetActionGroup *self = (EggWidgetActionGroup *)group;
  GSignalQuery query;
  guint signal_id;

  g_assert (EGG_IS_WIDGET_ACTION_GROUP (self));
  g_assert (action_name != NULL);

  if (!GTK_IS_WIDGET (self->widget))
    return NULL;

  signal_id = g_signal_lookup (action_name, G_OBJECT_TYPE (self->widget));
  if (signal_id == 0)
    return NULL;

  g_signal_query (signal_id, &query);

  if (!supports_types (query.param_types, query.n_params))
    return NULL;

  return create_variant_type (query.param_types, query.n_params);
}

const GVariantType *
egg_widget_action_group_get_action_state_type (GActionGroup *group,
                                               const gchar  *action_name)
{
  g_assert (EGG_IS_WIDGET_ACTION_GROUP (group));
  g_assert (action_name != NULL);

  return NULL;
}

static void
egg_widget_action_group_activate_action (GActionGroup *group,
                                         const gchar  *action_name,
                                         GVariant     *params)
{
  EggWidgetActionGroup *self = (EggWidgetActionGroup *)group;

  g_assert (EGG_IS_WIDGET_ACTION_GROUP (group));
  g_assert (action_name != NULL);

  if (GTK_IS_WIDGET (self->widget))
    {
      guint signal_id;

      signal_id = g_signal_lookup (action_name, G_OBJECT_TYPE (self->widget));

      if (signal_id != 0)
        {
          GSignalQuery query;

          g_signal_query (signal_id, &query);

          if (query.signal_flags & G_SIGNAL_ACTION)
            {
              do_activate (self, self->widget, &query, params);
              return;
            }
        }
    }

  g_warning ("Failed to activate action %s due to missing widget or action",
             action_name);
}

static gboolean
egg_widget_action_group_query_action (GActionGroup        *group,
                                      const gchar         *action_name,
                                      gboolean            *enabled,
                                      const GVariantType **parameter_type,
                                      const GVariantType **state_type,
                                      GVariant           **state_hint,
                                      GVariant           **state)
{
  EggWidgetActionGroup *self = (EggWidgetActionGroup *)group;

  g_assert (EGG_IS_WIDGET_ACTION_GROUP (group));

  if (!GTK_IS_WIDGET (self->widget))
    return FALSE;

  if (!g_signal_lookup (action_name, G_OBJECT_TYPE (self->widget)))
    return FALSE;

  if (state_hint)
    *state_hint = NULL;

  if (state_type)
    *state_type = NULL;

  if (state)
    *state = NULL;

  if (parameter_type)
    *parameter_type = egg_widget_action_group_get_action_parameter_type (group, action_name);

  if (enabled)
    *enabled = egg_widget_action_group_get_action_enabled (group, action_name);

  return TRUE;
}

static void
action_group_iface_init (GActionGroupInterface *iface)
{
  iface->has_action = egg_widget_action_group_has_action;
  iface->list_actions = egg_widget_action_group_list_actions;
  iface->get_action_enabled = egg_widget_action_group_get_action_enabled;
  iface->get_action_parameter_type = egg_widget_action_group_get_action_parameter_type;
  iface->get_action_state_type = egg_widget_action_group_get_action_state_type;
  iface->activate_action = egg_widget_action_group_activate_action;
  iface->query_action = egg_widget_action_group_query_action;
}

/**
 * egg_widget_action_group_new:
 *
 * Returns: (transfer full): An #EggWidgetActionGroup.
 */
GActionGroup *
egg_widget_action_group_new (GtkWidget *widget)
{
  return g_object_new (EGG_TYPE_WIDGET_ACTION_GROUP,
                       "widget", widget,
                       NULL);
}

/**
 * egg_widget_action_group_attach:
 * @widget: (type Gtk.Widget): A #GtkWidget
 * @group_name: the group name to use for the action group
 *
 * Helper function to create an #EggWidgetActionGroup and attach
 * it to @widget using the group name @group_name.
 */
void
egg_widget_action_group_attach (gpointer     widget,
                                const gchar *group_name)
{
  GActionGroup *group;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (group_name != NULL);

  group = egg_widget_action_group_new (widget);
  gtk_widget_insert_action_group (widget, group_name, group);
  g_object_unref (group);
}

void
egg_widget_action_group_set_action_enabled (EggWidgetActionGroup *self,
                                            const gchar          *action_name,
                                            gboolean              enabled)
{
  g_return_if_fail (EGG_IS_WIDGET_ACTION_GROUP (self));
  g_return_if_fail (action_name != NULL);
  g_return_if_fail (egg_widget_action_group_has_action (G_ACTION_GROUP (self), action_name));

  enabled = !!enabled;

  if (self->enabled == NULL)
    self->enabled = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  g_hash_table_insert (self->enabled, g_strdup (action_name), GINT_TO_POINTER (enabled));
  g_action_group_action_enabled_changed (G_ACTION_GROUP (self), action_name, enabled);

  g_debug ("Action %s %s", action_name, enabled ? "enabled" : "disabled");
}
