/* doap-person.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "doap-person.h"

struct _DoapPerson
{
  GObject parent_instance;

  gchar *email;
  gchar *name;
};

G_DEFINE_TYPE (DoapPerson, doap_person, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_EMAIL,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

DoapPerson *
doap_person_new (void)
{
  return g_object_new (DOAP_TYPE_PERSON, NULL);
}

const gchar *
doap_person_get_name (DoapPerson *self)
{
  g_return_val_if_fail (DOAP_IS_PERSON (self), NULL);

  return self->name;
}

void
doap_person_set_name (DoapPerson  *self,
                      const gchar *name)
{
  g_return_if_fail (DOAP_IS_PERSON (self));

  if (g_strcmp0 (self->name, name) != 0)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

const gchar *
doap_person_get_email (DoapPerson *self)
{
  g_return_val_if_fail (DOAP_IS_PERSON (self), NULL);

  return self->email;
}

void
doap_person_set_email (DoapPerson  *self,
                       const gchar *email)
{
  g_return_if_fail (DOAP_IS_PERSON (self));

  if (g_strcmp0 (self->email, email) != 0)
    {
      g_free (self->email);
      self->email = g_strdup (email);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EMAIL]);
    }
}

static void
doap_person_finalize (GObject *object)
{
  DoapPerson *self = (DoapPerson *)object;

  g_clear_pointer (&self->email, g_free);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (doap_person_parent_class)->finalize (object);
}

static void
doap_person_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  DoapPerson *self = DOAP_PERSON (object);

  switch (prop_id)
    {
    case PROP_EMAIL:
      g_value_set_string (value, doap_person_get_email (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, doap_person_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
doap_person_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  DoapPerson *self = DOAP_PERSON (object);

  switch (prop_id)
    {
    case PROP_EMAIL:
      doap_person_set_email (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      doap_person_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
doap_person_class_init (DoapPersonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = doap_person_finalize;
  object_class->get_property = doap_person_get_property;
  object_class->set_property = doap_person_set_property;

  properties [PROP_EMAIL] =
    g_param_spec_string ("email",
                         "Email",
                         "The email of the person.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the person.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
doap_person_init (DoapPerson *self)
{
}
