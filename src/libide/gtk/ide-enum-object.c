/* ide-enum-object.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-enum-object"

#include "config.h"

#include "ide-enum-object.h"

struct _IdeEnumObject
{
  GObject parent_instance;
  char *description;
  char *nick;
  char *title;
};

enum {
  PROP_0,
  PROP_DESCRIPTION,
  PROP_NICK,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeEnumObject, ide_enum_object, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_enum_object_dispose (GObject *object)
{
  IdeEnumObject *self = (IdeEnumObject *)object;

  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->nick, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_enum_object_parent_class)->dispose (object);
}

static void
ide_enum_object_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeEnumObject *self = IDE_ENUM_OBJECT (object);

  switch (prop_id)
    {
    case PROP_DESCRIPTION:
      g_value_set_string (value, self->description);
      break;

    case PROP_NICK:
      g_value_set_string (value, self->nick);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_enum_object_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeEnumObject *self = IDE_ENUM_OBJECT (object);

  switch (prop_id)
    {
    case PROP_DESCRIPTION:
      self->description = g_value_dup_string (value);
      break;

    case PROP_NICK:
      self->nick = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_enum_object_class_init (IdeEnumObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_enum_object_dispose;
  object_class->get_property = ide_enum_object_get_property;
  object_class->set_property = ide_enum_object_set_property;

  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL, NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_NICK] =
    g_param_spec_string ("nick", NULL, NULL, NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_enum_object_init (IdeEnumObject *self)
{
}

const char *
ide_enum_object_get_description (IdeEnumObject *self)
{
  g_return_val_if_fail (IDE_IS_ENUM_OBJECT (self), NULL);

  return self->description;
}

const char *
ide_enum_object_get_title (IdeEnumObject *self)
{
  g_return_val_if_fail (IDE_IS_ENUM_OBJECT (self), NULL);

  return self->title;
}

const char *
ide_enum_object_get_nick (IdeEnumObject *self)
{
  g_return_val_if_fail (IDE_IS_ENUM_OBJECT (self), NULL);

  return self->nick;
}

IdeEnumObject *
ide_enum_object_new (const char *nick,
                     const char *title,
                     const char *description)
{
  return g_object_new (IDE_TYPE_ENUM_OBJECT,
                       "nick", nick,
                       "title", title,
                       "description", description,
                       NULL);
}
