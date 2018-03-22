/* ide-pausable.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-pausable"

#include "config.h"

#include "ide-pausable.h"

struct _IdePausable
{
  GObject  parent_instance;

  gchar   *title;
  gchar   *subtitle;

  guint    paused : 1;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_PAUSED,
  N_PROPS
};

enum {
  PAUSED,
  UNPAUSED,
  N_SIGNALS
};

G_DEFINE_TYPE (IdePausable, ide_pausable, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_pausable_finalize (GObject *object)
{
  IdePausable *self = (IdePausable *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);

  G_OBJECT_CLASS (ide_pausable_parent_class)->finalize (object);
}

static void
ide_pausable_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdePausable *self = IDE_PAUSABLE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, self->subtitle);
      break;

    case PROP_PAUSED:
      g_value_set_boolean (value, ide_pausable_get_paused (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pausable_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdePausable *self = IDE_PAUSABLE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      ide_pausable_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      ide_pausable_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_PAUSED:
      ide_pausable_set_paused (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pausable_class_init (IdePausableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_pausable_finalize;
  object_class->get_property = ide_pausable_get_property;
  object_class->set_property = ide_pausable_set_property;

  properties [PROP_PAUSED] =
    g_param_spec_boolean ("paused", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [PAUSED] =
    g_signal_new ("paused",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals [UNPAUSED] =
    g_signal_new ("unpaused",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
ide_pausable_init (IdePausable *self)
{
}

IdePausable *
ide_pausable_new (void)
{
  return g_object_new (IDE_TYPE_PAUSABLE, NULL);
}

const gchar *
ide_pausable_get_title (IdePausable *self)
{
  g_return_val_if_fail (IDE_IS_PAUSABLE (self), NULL);

  return self->title;
}

void
ide_pausable_set_title (IdePausable *self,
                        const gchar *title)
{
  if (g_strcmp0 (title, self->title) != 0)
    {
      g_free (self->title);
      self->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

const gchar *
ide_pausable_get_subtitle (IdePausable *self)
{
  g_return_val_if_fail (IDE_IS_PAUSABLE (self), NULL);

  return self->subtitle;
}

void
ide_pausable_set_subtitle (IdePausable *self,
                           const gchar *subtitle)
{
  if (g_strcmp0 (subtitle, self->subtitle) != 0)
    {
      g_free (self->subtitle);
      self->subtitle = g_strdup (subtitle);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
    }
}

gboolean
ide_pausable_get_paused (IdePausable *self)
{
  g_return_val_if_fail (IDE_IS_PAUSABLE (self), FALSE);

  return self->paused;
}

void
ide_pausable_set_paused (IdePausable *self,
                         gboolean     paused)
{
  g_return_if_fail (IDE_IS_PAUSABLE (self));

  paused = !!paused;

  if (self->paused != paused)
    {
      self->paused = paused;

      if (paused)
        g_signal_emit (self, signals [PAUSED], 0);
      else
        g_signal_emit (self, signals [UNPAUSED], 0);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAUSED]);
    }
}

void
ide_pausable_pause (IdePausable *self)
{
  g_return_if_fail (IDE_IS_PAUSABLE (self));

  ide_pausable_set_paused (self, TRUE);
}

void
ide_pausable_unpause (IdePausable *self)
{
  g_return_if_fail (IDE_IS_PAUSABLE (self));

  ide_pausable_set_paused (self, FALSE);
}
