/* gb-command-provider.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-command-provider.h"

struct _GbCommandProviderPrivate
{
  gint priority;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandProvider, gb_command_provider,
                            G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PRIORITY,
  LAST_PROP
};

enum {
  LOOKUP,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

GbCommandProvider *
gb_command_provider_new (void)
{
  return g_object_new (GB_TYPE_COMMAND_PROVIDER, NULL);
}

gint
gb_command_provider_get_priority (GbCommandProvider *provider)
{
  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), 0);

  return provider->priv->priority;
}

void
gb_command_provider_set_priority (GbCommandProvider *provider,
                                  gint               priority)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));

  if (provider->priv->priority != priority)
    {
      provider->priv->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (provider),
                                gParamSpecs [PROP_PRIORITY]);
    }
}

GAction *
gb_command_provider_lookup (GbCommandProvider  *provider,
                            const gchar        *command_text,
                            GVariant          **parameter)
{
  GAction *ret = NULL;

  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), NULL);
  g_return_val_if_fail (command_text, NULL);

  if (parameter)
    *parameter = NULL;

  g_signal_emit (provider, gSignals [LOOKUP], 0, command_text, provider, &ret);

  return ret;
}

static void
gb_command_provider_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbCommandProvider *self = GB_COMMAND_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      g_value_set_int (value, gb_command_provider_get_priority (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_provider_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbCommandProvider *self = GB_COMMAND_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      gb_command_provider_set_priority (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_provider_class_init (GbCommandProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gb_command_provider_get_property;
  object_class->set_property = gb_command_provider_set_property;

  /**
   * GbCommandProvider:priority:
   *
   * The priority property denotes the order in which providers should be
   * queried. During the lookup process, providers are queried in order of
   * priority to parse the command text and resolve a GAction and optional
   * parameters.
   */
  gParamSpecs [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      _("Priority"),
                      _("The priority of the command provider."),
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PRIORITY,
                                   gParamSpecs [PROP_PRIORITY]);

  /**
   * GbCommandProvider::lookup:
   * @command_text: (in): the command line text to be processed.
   * @parameter: (out): a location to store any parsed parameters.
   *
   * This signal is emitted when a request to parse the command text is
   * received. Only the first handler will respond to the action. The
   * callee should return a GAction if successful, otherwise %NULL.
   * 
   * If successful, the callee can set @parameter, to specify the
   * parameters that should be passed to the resulting action.
   */
  gSignals [LOOKUP] =
    g_signal_new ("lookup",
                  GB_TYPE_COMMAND_PROVIDER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbCommandProviderClass, lookup),
                  g_signal_accumulator_first_wins,
                  NULL,
                  NULL,
                  G_TYPE_ACTION,
                  2,
                  G_TYPE_STRING,
                  G_TYPE_POINTER);
}

static void
gb_command_provider_init (GbCommandProvider *self)
{
  self->priv = gb_command_provider_get_instance_private (self);
}
