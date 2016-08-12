/* gbp-gobject-signal.c
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

#define G_LOG_DOMAIN "gbp-gobject-signal"

#include "gbp-gobject-signal.h"

struct _GbpGobjectSignal
{
  GObject parent_instance;

  gchar *name;
};

G_DEFINE_TYPE (GbpGobjectSignal, gbp_gobject_signal, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_gobject_signal_finalize (GObject *object)
{
  GbpGobjectSignal *self = (GbpGobjectSignal *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gbp_gobject_signal_parent_class)->finalize (object);
}

static void
gbp_gobject_signal_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpGobjectSignal *self = GBP_GOBJECT_SIGNAL (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_signal_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbpGobjectSignal *self = GBP_GOBJECT_SIGNAL (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_free (self->name);
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_signal_class_init (GbpGobjectSignalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_gobject_signal_finalize;
  object_class->get_property = gbp_gobject_signal_get_property;
  object_class->set_property = gbp_gobject_signal_set_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_gobject_signal_init (GbpGobjectSignal *self)
{
}
