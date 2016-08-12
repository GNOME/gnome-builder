/* gbp-gobject-property.c
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

#define G_LOG_DOMAIN "gbp-gobject-property"

#include "gbp-gobject-property.h"

struct _GbpGobjectProperty
{
  GObject parent_instance;

  GbpGobjectPropertyKind kind;

  gchar *ctype;
  gchar *name;
  gchar *default_;
  gchar *minimum;
  gchar *maximum;

  guint readable : 1;
  guint writable : 1;
  guint construct_only : 1;
};

enum {
  PROP_0,
  PROP_CNAME,
  PROP_CONSTRUCT_ONLY,
  PROP_CTYPE,
  PROP_DEFAULT,
  PROP_GTYPE,
  PROP_KIND,
  PROP_MAXIMUM,
  PROP_MINIMUM,
  PROP_NAME,
  PROP_READABLE,
  PROP_WRITABLE,
  N_PROPS
};

G_DEFINE_TYPE (GbpGobjectProperty, gbp_gobject_property, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
gbp_gobject_property_set_ctype (GbpGobjectProperty *self,
                                const gchar        *ctype)
{
  g_assert (GBP_IS_GOBJECT_PROPERTY (self));

  if (g_strcmp0 (ctype, self->ctype) != 0)
    {
      g_free (self->ctype);
      self->ctype = g_strdup (ctype);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CTYPE]);
    }
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

static gchar *
gbp_gobject_property_get_gtype (GbpGobjectProperty *self)
{
  g_autofree gchar *mangled = NULL;
  g_autofree gchar *nsupper = NULL;
  g_autofree gchar *clsupper = NULL;
  g_auto(GStrv) parts = NULL;

  g_assert (GBP_IS_GOBJECT_PROPERTY (self));

  if (self->ctype == NULL)
    return NULL;

  mangled = mangle_name (self->ctype);

  if (mangled == NULL || *mangled == '\0')
    return NULL;

  parts = g_strsplit (mangled, "_", 2);

  if (!parts[0] || !parts[1])
    return NULL;

  nsupper = g_utf8_strup (parts[0], -1);
  clsupper = g_utf8_strup (parts[1], -1);

  return g_strdup_printf ("%s_TYPE_%s", nsupper, clsupper);
}

static void
gbp_gobject_property_finalize (GObject *object)
{
  GbpGobjectProperty *self = (GbpGobjectProperty *)object;

  g_clear_pointer (&self->ctype, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->default_, g_free);
  g_clear_pointer (&self->minimum, g_free);
  g_clear_pointer (&self->maximum, g_free);

  G_OBJECT_CLASS (gbp_gobject_property_parent_class)->finalize (object);
}

static void
gbp_gobject_property_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpGobjectProperty *self = GBP_GOBJECT_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_CNAME:
      g_value_take_string (value, g_strdelimit (g_strdup (self->name ?: ""), "-", '_'));
      break;

    case PROP_CTYPE:
      g_value_set_string (value, self->ctype ?: "");
      break;

    case PROP_CONSTRUCT_ONLY:
      g_value_set_boolean (value, self->construct_only);
      break;

    case PROP_GTYPE:
      g_value_take_string (value, gbp_gobject_property_get_gtype (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_DEFAULT:
      g_value_set_string (value, self->default_);
      break;

    case PROP_KIND:
      g_value_set_enum (value, self->kind);
      break;

    case PROP_MINIMUM:
      g_value_set_string (value, self->minimum);
      break;

    case PROP_MAXIMUM:
      g_value_set_string (value, self->maximum);
      break;

    case PROP_READABLE:
      g_value_set_boolean (value, self->readable);
      break;

    case PROP_WRITABLE:
      g_value_set_boolean (value, self->writable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_property_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpGobjectProperty *self = GBP_GOBJECT_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_CONSTRUCT_ONLY:
      self->construct_only = g_value_get_boolean (value);
      break;

    case PROP_CTYPE:
      gbp_gobject_property_set_ctype (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      gbp_gobject_property_set_name (self, g_value_get_string (value));
      break;

    case PROP_DEFAULT:
      gbp_gobject_property_set_default (self, g_value_get_string (value));
      break;

    case PROP_KIND:
      gbp_gobject_property_set_kind (self, g_value_get_enum (value));
      break;

    case PROP_MINIMUM:
      gbp_gobject_property_set_minimum (self, g_value_get_string (value));
      break;

    case PROP_MAXIMUM:
      gbp_gobject_property_set_maximum (self, g_value_get_string (value));
      break;

    case PROP_READABLE:
      gbp_gobject_property_set_readable (self, g_value_get_boolean (value));
      break;

    case PROP_WRITABLE:
      gbp_gobject_property_set_writable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_property_class_init (GbpGobjectPropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_gobject_property_finalize;
  object_class->get_property = gbp_gobject_property_get_property;
  object_class->set_property = gbp_gobject_property_set_property;

  properties [PROP_CNAME] =
    g_param_spec_string ("cname",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CTYPE] =
    g_param_spec_string ("ctype",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_GTYPE] =
    g_param_spec_string ("gtype",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       NULL,
                       NULL,
                       GBP_TYPE_GOBJECT_PROPERTY_KIND,
                       GBP_GOBJECT_PROPERTY_STRING,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONSTRUCT_ONLY] =
    g_param_spec_boolean ("construct-only",
                          NULL,
                          NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEFAULT] =
    g_param_spec_string ("default",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MINIMUM] =
    g_param_spec_string ("minimum",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MAXIMUM] =
    g_param_spec_string ("maximum",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_READABLE] =
    g_param_spec_boolean ("readable",
                          NULL,
                          NULL,
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_WRITABLE] =
    g_param_spec_boolean ("writable",
                          NULL,
                          NULL,
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_gobject_property_init (GbpGobjectProperty *self)
{
  self->readable = TRUE;
  self->writable = TRUE;
  self->kind = GBP_GOBJECT_PROPERTY_STRING;
  self->default_ = g_strdup ("NULL");
}

GbpGobjectProperty *
gbp_gobject_property_new (void)
{
  return g_object_new (GBP_TYPE_GOBJECT_PROPERTY, NULL);
}

GbpGobjectPropertyKind
gbp_gobject_property_get_kind (GbpGobjectProperty     *self)
{
  return self->kind;
}

void
gbp_gobject_property_set_kind (GbpGobjectProperty     *self,
                               GbpGobjectPropertyKind  kind)
{
  if (kind != self->kind)
    {
      self->kind = kind;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KIND]);
    }
}

const gchar *
gbp_gobject_property_get_name (GbpGobjectProperty *self)
{
  return self->name;
}

void
gbp_gobject_property_set_name (GbpGobjectProperty *self,
                               const gchar        *name)
{
  if (g_strcmp0 (name, self->name) != 0)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

const gchar *
gbp_gobject_property_get_default (GbpGobjectProperty *self)
{
  return self->default_;
}

void
gbp_gobject_property_set_default (GbpGobjectProperty *self,
                                  const gchar        *default_)
{
  if (g_strcmp0 (default_, self->default_) != 0)
    {
      g_free (self->default_);
      self->default_ = g_strdup (default_);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEFAULT]);
    }
}

const gchar *
gbp_gobject_property_get_minimum (GbpGobjectProperty *self)
{
  return self->minimum;
}

void
gbp_gobject_property_set_minimum (GbpGobjectProperty *self,
                                  const gchar        *minimum)
{
  if (g_strcmp0 (minimum, self->minimum) != 0)
    {
      g_free (self->minimum);
      self->minimum = g_strdup (minimum);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MINIMUM]);
    }
}

const gchar *
gbp_gobject_property_get_maximum (GbpGobjectProperty *self)
{
  return self->maximum;
}

void
gbp_gobject_property_set_maximum (GbpGobjectProperty *self,
                                  const gchar        *maximum)
{
  if (g_strcmp0 (maximum, self->maximum) != 0)
    {
      g_free (self->maximum);
      self->maximum = g_strdup (maximum);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAXIMUM]);
    }
}

gboolean
gbp_gobject_property_get_readable (GbpGobjectProperty *self)
{
  return self->readable;
}

void
gbp_gobject_property_set_readable (GbpGobjectProperty *self,
                                   gboolean            readable)
{
  readable = !!readable;

  if (self->readable != readable)
    {
      self->readable = readable;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READABLE]);
    }
}

gboolean
gbp_gobject_property_get_writable (GbpGobjectProperty *self)
{
  return self->writable;
}

void
gbp_gobject_property_set_writable (GbpGobjectProperty *self,
                                   gboolean            writable)
{
  writable = !!writable;

  if (self->writable != writable)
    {
      self->writable = writable;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WRITABLE]);
    }
}

GType
gbp_gobject_property_kind_get_type (void)
{
  static GType type_id = 0;

  if (g_once_init_enter (&type_id))
    {
      static const GEnumValue values[] = {
        { GBP_GOBJECT_PROPERTY_BOOLEAN, "GBP_GOBJECT_PROPERTY_BOOLEAN", "boolean" },
        { GBP_GOBJECT_PROPERTY_BOXED, "GBP_GOBJECT_PROPERTY_BOXED", "boxed" },
        { GBP_GOBJECT_PROPERTY_CHAR, "GBP_GOBJECT_PROPERTY_CHAR", "char" },
        { GBP_GOBJECT_PROPERTY_DOUBLE, "GBP_GOBJECT_PROPERTY_DOUBLE", "double" },
        { GBP_GOBJECT_PROPERTY_ENUM, "GBP_GOBJECT_PROPERTY_ENUM", "enum" },
        { GBP_GOBJECT_PROPERTY_FLAGS, "GBP_GOBJECT_PROPERTY_FLAGS", "flags" },
        { GBP_GOBJECT_PROPERTY_FLOAT, "GBP_GOBJECT_PROPERTY_FLOAT", "float" },
        { GBP_GOBJECT_PROPERTY_INT, "GBP_GOBJECT_PROPERTY_INT", "int" },
        { GBP_GOBJECT_PROPERTY_INT64, "GBP_GOBJECT_PROPERTY_INT64", "int64" },
        { GBP_GOBJECT_PROPERTY_LONG, "GBP_GOBJECT_PROPERTY_LONG", "long" },
        { GBP_GOBJECT_PROPERTY_OBJECT, "GBP_GOBJECT_PROPERTY_OBJECT", "object" },
        { GBP_GOBJECT_PROPERTY_POINTER, "GBP_GOBJECT_PROPERTY_POINTER", "pointer" },
        { GBP_GOBJECT_PROPERTY_STRING, "GBP_GOBJECT_PROPERTY_STRING", "string" },
        { GBP_GOBJECT_PROPERTY_UINT, "GBP_GOBJECT_PROPERTY_UINT", "uint" },
        { GBP_GOBJECT_PROPERTY_UINT64, "GBP_GOBJECT_PROPERTY_UINT64", "uint64" },
        { GBP_GOBJECT_PROPERTY_ULONG, "GBP_GOBJECT_PROPERTY_ULONG", "ulong" },
        { GBP_GOBJECT_PROPERTY_UNICHAR, "GBP_GOBJECT_PROPERTY_UNICHAR", "unichar" },
        { GBP_GOBJECT_PROPERTY_VARIANT, "GBP_GOBJECT_PROPERTY_VARIANT", "variant" },
        { 0 }
      };
      GType _type_id = g_enum_register_static ("GbpGobjectPropertyKind", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
