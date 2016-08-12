/* gbp-gobject-spec.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "gbp-gobject-spec"

#include <string.h>

#include "gbp-gobject-spec.h"

struct _GbpGobjectSpec
{
  GObject     parent_instance;

  gchar      *class_name;
  gchar      *name;
  gchar      *namespace;
  gchar      *parent_name;

  GListStore *properties;
  GListStore *signals;

  guint       final : 1;
};

G_DEFINE_TYPE (GbpGobjectSpec, gbp_gobject_spec, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CLASS_NAME,
  PROP_FINAL,
  PROP_NAME,
  PROP_NAMESPACE,
  PROP_PARENT_NAME,
  PROP_PROPERTIES,
  PROP_READY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_gobject_spec_rebuild (GbpGobjectSpec *self)
{
  g_assert (GBP_IS_GOBJECT_SPEC (self));

  g_free (self->name);
  self->name = g_strdup_printf ("%s%s",
                                self->namespace ?: "",
                                self->class_name ?: "");
}

static void
gbp_gobject_spec_set_class_name (GbpGobjectSpec *self,
                                 const gchar    *class_name)
{
  g_assert (GBP_IS_GOBJECT_SPEC (self));

  g_free (self->class_name);
  self->class_name = g_strdup (class_name);

  gbp_gobject_spec_rebuild (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLASS_NAME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);
}

static gchar *
mangle_name (const gchar *name)
{
  GString *symbol_name = g_string_new ("");
  gint i;

  /* copied from gtkbuilder.c */

  for (i = 0; name[i] != '\0'; i++)
    {
      /* skip if uppercase, first or previous is uppercase */
      if ((name[i] == g_ascii_toupper (name[i]) &&
           i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
          (i > 2 && name[i]   == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
    }

  return g_string_free (symbol_name, FALSE);
}

static gboolean
parse_name (const gchar  *name,
            gchar       **namespace,
            gchar       **class_name)
{
  g_autofree gchar *mangled = NULL;
  g_auto(GStrv) parts = NULL;

  g_assert (namespace != NULL);
  g_assert (class_name != NULL);

  *namespace = NULL;
  *class_name = NULL;

  if (name == NULL)
    return FALSE;

  mangled = mangle_name (name);
  parts = g_strsplit (mangled, "_", 2);

  if (parts == NULL || parts[0] == NULL)
    return FALSE;

  *namespace = g_strndup (name, strlen (parts[0]));

  if (parts[1])
    *class_name = g_strndup (name + strlen (parts[0]), strlen (parts[1]));
  else
    *class_name = g_strdup ("");

  return TRUE;
}

static void
gbp_gobject_spec_set_name (GbpGobjectSpec *self,
                           const gchar    *name)
{
  g_autofree gchar *namespace = NULL;
  g_autofree gchar *class_name = NULL;

  g_assert (GBP_IS_GOBJECT_SPEC (self));

  g_free (self->name);
  self->name = g_strdup (name);

  if (parse_name (name, &namespace, &class_name))
    {
      g_free (self->namespace);
      g_free (self->class_name);

      self->namespace = g_steal_pointer (&namespace);
      self->class_name = g_steal_pointer (&class_name);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLASS_NAME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAMESPACE]);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);
}

static void
gbp_gobject_spec_set_namespace (GbpGobjectSpec *self,
                                const gchar    *namespace)
{
  g_assert (GBP_IS_GOBJECT_SPEC (self));

  g_free (self->namespace);
  self->namespace = g_strdup (namespace);

  gbp_gobject_spec_rebuild (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAMESPACE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);
}

static void
gbp_gobject_spec_set_parent_name (GbpGobjectSpec *self,
                                  const gchar    *parent_name)
{
  g_assert (GBP_IS_GOBJECT_SPEC (self));

  g_free (self->parent_name);
  self->parent_name = g_strdup (parent_name);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PARENT_NAME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);
}

static void
gbp_gobject_spec_finalize (GObject *object)
{
  GbpGobjectSpec *self = (GbpGobjectSpec *)object;

  g_clear_object (&self->properties);
  g_clear_object (&self->signals);
  g_clear_pointer (&self->class_name, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->namespace, g_free);
  g_clear_pointer (&self->parent_name, g_free);

  G_OBJECT_CLASS (gbp_gobject_spec_parent_class)->finalize (object);
}

static void
gbp_gobject_spec_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpGobjectSpec *self = GBP_GOBJECT_SPEC (object);

  switch (prop_id)
    {
    case PROP_CLASS_NAME:
      if (self->class_name)
        g_value_set_string (value, self->class_name);
      else
        g_value_set_static_string (value, "");
      break;

    case PROP_FINAL:
      g_value_set_boolean (value, self->final);
      break;

    case PROP_NAME:
      if (self->name)
        g_value_set_string (value, self->name);
      else
        g_value_set_static_string (value, "");
      break;

    case PROP_NAMESPACE:
      if (self->namespace)
        g_value_set_string (value, self->namespace);
      else
        g_value_set_static_string (value, "");
      break;

    case PROP_PARENT_NAME:
      if (self->parent_name)
        g_value_set_string (value, self->parent_name);
      else
        g_value_set_static_string (value, "GObject");
      break;

    case PROP_PROPERTIES:
      g_value_set_object (value, self->properties);
      break;

    case PROP_READY:
      g_value_set_boolean (value, gbp_gobject_spec_get_ready (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_spec_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpGobjectSpec *self = GBP_GOBJECT_SPEC (object);

  switch (prop_id)
    {
    case PROP_CLASS_NAME:
      gbp_gobject_spec_set_class_name (self, g_value_get_string (value));
      break;

    case PROP_FINAL:
      self->final = g_value_get_boolean (value);
      break;

    case PROP_NAME:
      gbp_gobject_spec_set_name (self, g_value_get_string (value));
      break;

    case PROP_NAMESPACE:
      gbp_gobject_spec_set_namespace (self, g_value_get_string (value));
      break;

    case PROP_PARENT_NAME:
      gbp_gobject_spec_set_parent_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_spec_class_init (GbpGobjectSpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_gobject_spec_finalize;
  object_class->get_property = gbp_gobject_spec_get_property;
  object_class->set_property = gbp_gobject_spec_set_property;

  properties [PROP_CLASS_NAME] =
    g_param_spec_string ("class-name",
                         "Class Name",
                         "Class Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FINAL] =
    g_param_spec_boolean ("final",
                          "Final",
                          "Final",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAMESPACE] =
    g_param_spec_string ("namespace",
                         "Namespace",
                         "Namespace",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PARENT_NAME] =
    g_param_spec_string ("parent-name",
                         "Parent Name",
                         "Parent Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROPERTIES] =
    g_param_spec_object ("properties",
                         "Properties",
                         "Properties",
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_READY] =
    g_param_spec_boolean ("ready",
                          "Ready",
                          "Ready",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_gobject_spec_init (GbpGobjectSpec *self)
{
  self->properties = g_list_store_new (GBP_TYPE_GOBJECT_PROPERTY);
  self->signals = g_list_store_new (GBP_TYPE_GOBJECT_SIGNAL);
  self->final = TRUE;
  self->parent_name = g_strdup ("GObject");
}

GListModel *
gbp_gobject_spec_get_properties (GbpGobjectSpec *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC (self), NULL);

  return G_LIST_MODEL (self->properties);
}

void
gbp_gobject_spec_add_property (GbpGobjectSpec     *self,
                               GbpGobjectProperty *property)
{
  g_return_if_fail (GBP_IS_GOBJECT_SPEC (self));
  g_return_if_fail (GBP_IS_GOBJECT_PROPERTY (property));

  g_list_store_append (self->properties, property);
}

void
gbp_gobject_spec_remove_property (GbpGobjectSpec     *self,
                                  GbpGobjectProperty *property)
{
  GListModel *model;
  guint n_items;
  guint i;

  g_return_if_fail (GBP_IS_GOBJECT_SPEC (self));
  g_return_if_fail (GBP_IS_GOBJECT_PROPERTY (property));

  model = G_LIST_MODEL (self->properties);
  n_items = g_list_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(GbpGobjectProperty) item = NULL;

      item = g_list_model_get_item (model, i);

      if (item == property)
        {
          g_list_store_remove (self->properties, i);
          return;
        }
    }
}

GListModel *
gbp_gobject_spec_get_signals (GbpGobjectSpec *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC (self), NULL);

  return G_LIST_MODEL (self->signals);
}

void
gbp_gobject_spec_add_signal (GbpGobjectSpec   *self,
                             GbpGobjectSignal *signal)
{
  g_return_if_fail (GBP_IS_GOBJECT_SPEC (self));
  g_return_if_fail (GBP_IS_GOBJECT_SIGNAL (signal));

  g_list_store_append (self->signals, signal);
}

void
gbp_gobject_spec_remove_signal (GbpGobjectSpec   *self,
                                GbpGobjectSignal *signal)
{
  GListModel *model;
  guint n_items;
  guint i;

  g_return_if_fail (GBP_IS_GOBJECT_SPEC (self));
  g_return_if_fail (GBP_IS_GOBJECT_SIGNAL (signal));

  model = G_LIST_MODEL (self->signals);
  n_items = g_list_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(GbpGobjectSignal) item = NULL;

      item = g_list_model_get_item (model, i);

      if (item == signal)
        {
          g_list_store_remove (self->signals, i);
          return;
        }
    }
}

GbpGobjectSpec *
gbp_gobject_spec_new (void)
{
  return g_object_new (GBP_TYPE_GOBJECT_SPEC, NULL);
}

gboolean
gbp_gobject_spec_get_ready (GbpGobjectSpec *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC (self), FALSE);

  if (!self->class_name || !*self->class_name ||
      !self->namespace || !*self->namespace ||
      !self->parent_name || !*self->parent_name)
    return FALSE;

  return TRUE;
}

const gchar *
gbp_gobject_spec_get_name (GbpGobjectSpec *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC (self), NULL);

  return self->name;
}

const gchar *
gbp_gobject_spec_get_namespace (GbpGobjectSpec *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC (self), NULL);

  return self->namespace;
}

const gchar *
gbp_gobject_spec_get_class_name (GbpGobjectSpec *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC (self), NULL);

  return self->class_name;
}

const gchar *
gbp_gobject_spec_get_parent_name (GbpGobjectSpec *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC (self), NULL);

  return self->parent_name;
}
