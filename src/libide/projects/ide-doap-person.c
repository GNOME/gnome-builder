/* ide-doap-person.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-doap-person"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-doap-person.h"

struct _IdeDoapPerson
{
  GObject parent_instance;

  gchar *email;
  gchar *name;
};

G_DEFINE_FINAL_TYPE (IdeDoapPerson, ide_doap_person, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_EMAIL,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

IdeDoapPerson *
ide_doap_person_new (void)
{
  return g_object_new (IDE_TYPE_DOAP_PERSON, NULL);
}

const gchar *
ide_doap_person_get_name (IdeDoapPerson *self)
{
  g_return_val_if_fail (IDE_IS_DOAP_PERSON (self), NULL);

  return self->name;
}

void
ide_doap_person_set_name (IdeDoapPerson *self,
                          const gchar   *name)
{
  g_return_if_fail (IDE_IS_DOAP_PERSON (self));

  if (g_set_str (&self->name, name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

const gchar *
ide_doap_person_get_email (IdeDoapPerson *self)
{
  g_return_val_if_fail (IDE_IS_DOAP_PERSON (self), NULL);

  return self->email;
}

void
ide_doap_person_set_email (IdeDoapPerson *self,
                           const gchar   *email)
{
  g_return_if_fail (IDE_IS_DOAP_PERSON (self));

  if (g_set_str (&self->email, email))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EMAIL]);
    }
}

static void
ide_doap_person_finalize (GObject *object)
{
  IdeDoapPerson *self = (IdeDoapPerson *)object;

  g_clear_pointer (&self->email, g_free);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (ide_doap_person_parent_class)->finalize (object);
}

static void
ide_doap_person_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeDoapPerson *self = IDE_DOAP_PERSON (object);

  switch (prop_id)
    {
    case PROP_EMAIL:
      g_value_set_string (value, ide_doap_person_get_email (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_doap_person_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_doap_person_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeDoapPerson *self = IDE_DOAP_PERSON (object);

  switch (prop_id)
    {
    case PROP_EMAIL:
      ide_doap_person_set_email (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      ide_doap_person_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_doap_person_class_init (IdeDoapPersonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_doap_person_finalize;
  object_class->get_property = ide_doap_person_get_property;
  object_class->set_property = ide_doap_person_set_property;

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
ide_doap_person_init (IdeDoapPerson *self)
{
}
