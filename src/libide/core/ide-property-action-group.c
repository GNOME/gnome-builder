/* ide-property-action-group.c
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-property-action-group"

#include "config.h"

#include "gsettings-mapping.h"
#include "ide-property-action-group.h"

static void action_group_iface_init (GActionGroupInterface *iface);

struct _IdePropertyActionGroup
{
  GObject       parent_instance;
  GWeakRef      item_wr;
  GObjectClass *object_class;
  GArray       *mappings;
};

typedef enum
{
  FLAG_NONE          = 0,
  FLAG_NULL_AS_EMPTY = 1 << 0,
} Flags;

typedef struct
{
  const char         *action_name;
  const GVariantType *parameter_type;
  const GVariantType *state_type;
  GParamSpec         *pspec;
  Flags               flags;
} Mapping;

enum {
  PROP_0,
  PROP_ITEM,
  PROP_ITEM_TYPE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE_WITH_CODE (IdePropertyActionGroup, ide_property_action_group, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, action_group_iface_init))

static GVariant *
get_property_state (gpointer            instance,
                    GParamSpec         *pspec,
                    const GVariantType *state_type,
                    Flags               flags)
{
  GValue value = G_VALUE_INIT;
  GVariant *ret;

  g_assert (G_IS_OBJECT (instance));
  g_assert (pspec != NULL);
  g_assert (state_type != NULL);

  g_value_init (&value, pspec->value_type);
  g_object_get_property (instance, pspec->name, &value);

  if ((flags & FLAG_NULL_AS_EMPTY) &&
      G_VALUE_HOLDS_STRING (&value) &&
      g_value_get_string (&value) == NULL)
    g_value_set_static_string (&value, "");

  ret = g_settings_set_mapping (&value, state_type, NULL);

  g_value_unset (&value);

  return g_variant_ref_sink (ret);
}

static const GVariantType *
determine_type (GParamSpec *pspec)
{
  if (G_TYPE_IS_ENUM (pspec->value_type))
    return G_VARIANT_TYPE_STRING;

  switch (pspec->value_type)
    {
    case G_TYPE_BOOLEAN:
      return G_VARIANT_TYPE_BOOLEAN;

    case G_TYPE_INT:
      return G_VARIANT_TYPE_INT32;

    case G_TYPE_UINT:
      return G_VARIANT_TYPE_UINT32;

    case G_TYPE_DOUBLE:
    case G_TYPE_FLOAT:
      return G_VARIANT_TYPE_DOUBLE;

    case G_TYPE_STRING:
      return G_VARIANT_TYPE_STRING;

    default:
      return NULL;
    }
}

static void
ide_property_action_group_set_item_type (IdePropertyActionGroup *self,
                                         GType                   item_type)
{
  g_assert (IDE_IS_PROPERTY_ACTION_GROUP (self));
  g_assert (g_type_is_a (item_type, G_TYPE_OBJECT));

  self->object_class = g_type_class_ref (item_type);
}

static void
ide_property_action_group_dispose (GObject *object)
{
  IdePropertyActionGroup *self = (IdePropertyActionGroup *)object;

  if (self->mappings->len > 0)
    g_array_remove_range (self->mappings, 0, self->mappings->len);

  g_weak_ref_set (&self->item_wr, NULL);

  G_OBJECT_CLASS (ide_property_action_group_parent_class)->dispose (object);
}

static void
ide_property_action_group_finalize (GObject *object)
{
  IdePropertyActionGroup *self = (IdePropertyActionGroup *)object;

  g_weak_ref_clear (&self->item_wr);
  g_clear_pointer (&self->object_class, g_type_class_unref);
  g_clear_pointer (&self->mappings, g_array_unref);

  G_OBJECT_CLASS (ide_property_action_group_parent_class)->finalize (object);
}

static void
ide_property_action_group_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, G_OBJECT_CLASS_TYPE (self->object_class));
      break;

    case PROP_ITEM:
      g_value_take_object (value, g_weak_ref_get (&self->item_wr));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_property_action_group_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      ide_property_action_group_set_item_type (self, g_value_get_gtype (value));
      break;

    case PROP_ITEM:
      ide_property_action_group_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_property_action_group_class_init (IdePropertyActionGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_property_action_group_dispose;
  object_class->finalize = ide_property_action_group_finalize;
  object_class->get_property = ide_property_action_group_get_property;
  object_class->set_property = ide_property_action_group_set_property;

  properties [PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        G_TYPE_OBJECT,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_property_action_group_init (IdePropertyActionGroup *self)
{
  g_weak_ref_init (&self->item_wr, NULL);
  self->mappings = g_array_new (FALSE, FALSE, sizeof (Mapping));
}

IdePropertyActionGroup *
ide_property_action_group_new (GType item_type)
{
  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);

  return g_object_new (IDE_TYPE_PROPERTY_ACTION_GROUP,
                       "item-type", item_type,
                       NULL);
}

GType
ide_property_action_group_get_item_type (IdePropertyActionGroup *self)
{
  g_return_val_if_fail (IDE_IS_PROPERTY_ACTION_GROUP (self), 0);

  return G_OBJECT_CLASS_TYPE (self->object_class);
}

gpointer
ide_property_action_group_dup_item (IdePropertyActionGroup *self)
{
  g_return_val_if_fail (IDE_IS_PROPERTY_ACTION_GROUP (self), NULL);

  return g_weak_ref_get (&self->item_wr);
}

void
ide_property_action_group_set_item (IdePropertyActionGroup *self,
                                    gpointer                item)
{
  g_autoptr(GObject) old_item = NULL;
  gboolean enabled;
  gboolean toggle_enable;

  g_return_if_fail (IDE_IS_PROPERTY_ACTION_GROUP (self));
  g_return_if_fail (G_IS_OBJECT_CLASS (self->object_class));
  g_return_if_fail (!item || G_TYPE_CHECK_INSTANCE_TYPE (item, G_OBJECT_CLASS_TYPE (self->object_class)));

  old_item = g_weak_ref_get (&self->item_wr);

  if (old_item == item)
    return;

  enabled = item != NULL;
  toggle_enable = !old_item != !item;

  g_weak_ref_set (&self->item_wr, item);

  for (guint i = 0; i < self->mappings->len; i++)
    {
      const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);

      if (item != NULL)
        {
          g_autoptr(GVariant) value = get_property_state (item, mapping->pspec, mapping->state_type, mapping->flags);
          g_action_group_action_state_changed (G_ACTION_GROUP (self), mapping->action_name, value);
        }

      if (toggle_enable)
        g_action_group_action_enabled_changed (G_ACTION_GROUP (self), mapping->action_name, enabled);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

static gboolean
ide_property_action_group_has_action (GActionGroup *group,
                                      const char   *action_name)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);

  for (guint i = 0; i < self->mappings->len; i++)
    {
      const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);

      if (g_strcmp0 (mapping->action_name, action_name) == 0)
        return TRUE;
    }

  return FALSE;
}

static char **
ide_property_action_group_list_actions (GActionGroup *group)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);
  GArray *ar = g_array_new (TRUE, FALSE, sizeof (char *));

  for (guint i = 0; i < self->mappings->len; i++)
    {
      const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);
      char *name = g_strdup (mapping->action_name);

      g_array_append_val (ar, name);
    }

  return (char **)g_array_free (ar, FALSE);
}

static gboolean
ide_property_action_group_get_action_enabled (GActionGroup *group,
                                              const char   *action_name)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);
  g_autoptr(GObject) item = g_weak_ref_get (&self->item_wr);
  return item != NULL;
}

static GVariant *
ide_property_action_group_get_action_state (GActionGroup *group,
                                            const char   *action_name)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);
  g_autoptr(GObject) item = g_weak_ref_get (&self->item_wr);

  if (item != NULL)
    {
      for (guint i = 0; i < self->mappings->len; i++)
        {
          const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);

          if (g_strcmp0 (mapping->action_name, action_name) == 0)
            return get_property_state (item, mapping->pspec, mapping->state_type, mapping->flags);
        }
    }

  return NULL;
}

static GVariant *
ide_property_action_group_get_action_state_hint (GActionGroup *group,
                                                 const char   *action_name)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);
  g_autoptr(GObject) item = g_weak_ref_get (&self->item_wr);

  g_return_val_if_fail (item != NULL, NULL);

  for (guint i = 0; i < self->mappings->len; i++)
    {
      const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);

      if (g_strcmp0 (mapping->action_name, action_name) == 0)
        {
          if (mapping->pspec != NULL)
            {
              if (mapping->pspec->value_type == G_TYPE_INT)
                {
                  GParamSpecInt *pspec = (GParamSpecInt *)mapping->pspec;
                  return g_variant_new ("(ii)", pspec->minimum, pspec->maximum);
                }
              else if (mapping->pspec->value_type == G_TYPE_UINT)
                {
                  GParamSpecUInt *pspec = (GParamSpecUInt *)mapping->pspec;
                  return g_variant_new ("(uu)", pspec->minimum, pspec->maximum);
                }
              else if (mapping->pspec->value_type == G_TYPE_FLOAT)
                {
                  GParamSpecFloat *pspec = (GParamSpecFloat *)mapping->pspec;
                  return g_variant_new ("(dd)", (double)pspec->minimum, (double)pspec->maximum);
                }
              else if (mapping->pspec->value_type == G_TYPE_DOUBLE)
                {
                  GParamSpecDouble *pspec = (GParamSpecDouble *)mapping->pspec;
                  return g_variant_new ("(dd)", pspec->minimum, pspec->maximum);
                }
            }

          break;
        }
    }

  return NULL;
}

static void
ide_property_action_group_change_action_state (GActionGroup *group,
                                               const char   *action_name,
                                               GVariant     *value)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);
  g_autoptr(GObject) item = NULL;
  g_autoptr(GVariant) hold = NULL;

  g_assert (IDE_IS_PROPERTY_ACTION_GROUP (self));
  g_assert (action_name != NULL);

  if (!(item = g_weak_ref_get (&self->item_wr)))
    {
      g_warning ("Attempt to change state of action %s but it is disabled",
                 action_name);
      return;
    }

  if (value != NULL)
    hold = g_variant_ref_sink (value);

  for (guint i = 0; i < self->mappings->len; i++)
    {
      const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);

      g_assert (mapping->pspec != NULL);

      if (g_strcmp0 (action_name, mapping->action_name) == 0)
        {
          GValue gvalue = G_VALUE_INIT;

          g_value_init (&gvalue, mapping->pspec->value_type);
          g_settings_get_mapping (&gvalue, value, NULL);
          g_object_set_property (item, mapping->pspec->name, &gvalue);
          g_value_unset (&gvalue);

          g_action_group_action_state_changed (G_ACTION_GROUP (self),
                                               mapping->action_name,
                                               value);

          return;
        }
    }

  g_warning ("Failed to locate action %s", action_name);
}

static const GVariantType *
ide_property_action_group_get_action_state_type (GActionGroup *group,
                                                 const char   *action_name)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);

  for (guint i = 0; i < self->mappings->len; i++)
    {
      const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);

      if (g_strcmp0 (mapping->action_name, action_name) == 0)
        return mapping->state_type;
    }

  return NULL;
}

static void
ide_property_action_group_activate_action (GActionGroup *group,
                                           const char   *action_name,
                                           GVariant     *parameter)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);
  g_autoptr(GObject) item = g_weak_ref_get (&self->item_wr);

  g_return_if_fail (item != NULL);

  for (guint i = 0; i < self->mappings->len; i++)
    {
      const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);

      if (g_strcmp0 (mapping->action_name, action_name) == 0)
        {
          if (mapping->pspec->value_type == G_TYPE_BOOLEAN)
            {
              gboolean value;

              g_return_if_fail (parameter == NULL);

              g_object_get (item, mapping->pspec->name, &value, NULL);
              g_object_set (item, mapping->pspec->name, !value, NULL);
            }
          else
            {
              g_return_if_fail (parameter != NULL && g_variant_is_of_type (parameter, mapping->state_type));

              ide_property_action_group_change_action_state (group, action_name, parameter);
            }
        }
    }
}

static const GVariantType *
ide_property_action_group_get_action_parameter_type (GActionGroup *group,
                                                     const char   *action_name)
{
  IdePropertyActionGroup *self = IDE_PROPERTY_ACTION_GROUP (group);

  for (guint i = 0; i < self->mappings->len; i++)
    {
      const Mapping *mapping = &g_array_index (self->mappings, Mapping, i);

      if (g_strcmp0 (mapping->action_name, action_name) == 0)
        return mapping->parameter_type;
    }

  return NULL;
}

static void
action_group_iface_init (GActionGroupInterface *iface)
{
  iface->has_action = ide_property_action_group_has_action;
  iface->list_actions = ide_property_action_group_list_actions;
  iface->get_action_enabled = ide_property_action_group_get_action_enabled;
  iface->get_action_parameter_type = ide_property_action_group_get_action_parameter_type;
  iface->get_action_state = ide_property_action_group_get_action_state;
  iface->get_action_state_type = ide_property_action_group_get_action_state_type;
  iface->get_action_state_hint = ide_property_action_group_get_action_state_hint;
  iface->change_action_state = ide_property_action_group_change_action_state;
  iface->activate_action = ide_property_action_group_activate_action;
}

void
ide_property_action_group_add_all (IdePropertyActionGroup *self)
{
  g_autofree GParamSpec **pspecs = NULL;
  guint n_pspecs;

  g_return_if_fail (IDE_IS_PROPERTY_ACTION_GROUP (self));
  g_return_if_fail (G_IS_OBJECT_CLASS (self->object_class));

  pspecs = g_object_class_list_properties (self->object_class, &n_pspecs);

  for (guint i = 0; i < n_pspecs; i++)
    {
      GParamSpec *pspec = pspecs[i];
      const GVariantType *state_type;
      Mapping mapping = {0};

      if (~pspec->flags & G_PARAM_READABLE || ~pspec->flags & G_PARAM_WRITABLE || pspec->flags & G_PARAM_CONSTRUCT_ONLY)
        continue;

      if (!(state_type = determine_type (pspec)))
        continue;

      mapping.action_name = g_intern_string (pspec->name);
      mapping.pspec = pspec;
      mapping.state_type = state_type;
      if (pspec->value_type != G_TYPE_BOOLEAN)
        mapping.parameter_type = state_type;

      g_array_append_val (self->mappings, mapping);
    }
}

static void
ide_property_action_group_add_internal (IdePropertyActionGroup *self,
                                        const char             *action_name,
                                        const char             *property_name,
                                        Flags                   flags)
{
  const GVariantType *state_type;
  GParamSpec *pspec;
  Mapping mapping = {0};

  g_return_if_fail (IDE_IS_PROPERTY_ACTION_GROUP (self));
  g_return_if_fail (G_IS_OBJECT_CLASS (self->object_class));
  g_return_if_fail (action_name != NULL);
  g_return_if_fail (property_name != NULL);

  action_name = g_intern_string (action_name);
  property_name = g_intern_string (property_name);

  if (!(pspec = g_object_class_find_property (self->object_class, property_name)))
    {
      g_warning ("Failed to locate property %s on type %s",
                 property_name,
                 G_OBJECT_CLASS_NAME (self->object_class));
      return;
    }

  if (~pspec->flags & G_PARAM_READABLE || ~pspec->flags & G_PARAM_WRITABLE || pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_warning ("Property must be read/write and not construct-only: %s:%s",
                 G_OBJECT_CLASS_NAME (self->object_class),
                 property_name);
      return;
    }

  if (!(state_type = determine_type (pspec)))
    {
      g_warning ("Cannot determine type for %s:%s",
                 G_OBJECT_CLASS_NAME (self->object_class),
                 property_name);
      return;
    }

  mapping.action_name = action_name;
  mapping.pspec = pspec;
  mapping.state_type = state_type;
  mapping.flags = flags;

  if (pspec->value_type != G_TYPE_BOOLEAN)
    mapping.parameter_type = state_type;

  g_array_append_val (self->mappings, mapping);
}

void
ide_property_action_group_add (IdePropertyActionGroup *self,
                               const char             *action_name,
                               const char             *property_name)
{
  ide_property_action_group_add_internal (self, action_name, property_name, FLAG_NONE);
}

void
ide_property_action_group_add_string (IdePropertyActionGroup *self,
                                      const char             *action_name,
                                      const char             *property_name,
                                      gboolean                treat_null_as_empty)
{
  ide_property_action_group_add_internal (self,
                                          action_name,
                                          property_name,
                                          treat_null_as_empty ? FLAG_NULL_AS_EMPTY : 0);
}
