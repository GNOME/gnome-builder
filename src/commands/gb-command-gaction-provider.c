/* gb-command-gaction-provider.c
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

#include "gb-command-gaction-provider.h"

struct _GbCommandGactionProviderPrivate
{
  GbWorkbench *workbench;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandGactionProvider,
                            gb_command_gaction_provider,
                            GB_TYPE_COMMAND_PROVIDER)

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbCommandProvider *
gb_command_gaction_provider_new (GbWorkbench *workbench)
{
  return g_object_new (GB_TYPE_COMMAND_GACTION_PROVIDER,
                       "workbench", workbench,
                       NULL);
}

GbWorkbench *
gb_command_gaction_provider_get_workbench (GbCommandGactionProvider *provider)
{
  g_return_val_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider), NULL);

  return provider->priv->workbench;
}

void
gb_command_gaction_provider_set_workbench (GbCommandGactionProvider *provider,
                                           GbWorkbench              *workbench)
{
  GbCommandGactionProviderPrivate *priv;

  g_return_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider));
  g_return_if_fail (!workbench || GB_IS_WORKBENCH (workbench));

  priv = provider->priv;

  if (workbench != priv->workbench)
    {
      if (priv->workbench)
        {
          g_object_remove_weak_pointer (G_OBJECT (priv->workbench),
                                        (gpointer *)&priv->workbench);
          priv->workbench = NULL;
        }

      if (workbench)
        {
          priv->workbench = workbench;
          g_object_add_weak_pointer (G_OBJECT (workbench),
                                     (gpointer *)&priv->workbench);
        }

      g_object_notify_by_pspec (G_OBJECT (provider),
                                gParamSpecs [PROP_WORKBENCH]);
    }
}

static void
gb_command_gaction_provider_dispose (GObject *object)
{
  GbCommandGactionProvider *provider = (GbCommandGactionProvider *)object;

  gb_command_gaction_provider_set_workbench (provider, NULL);

  G_OBJECT_CLASS (gb_command_gaction_provider_parent_class)->dispose (object);
}

static void
gb_command_gaction_provider_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbCommandGactionProvider *self = GB_COMMAND_GACTION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_WORKBENCH:
      g_value_set_object (value,
                          gb_command_gaction_provider_get_workbench (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_gaction_provider_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbCommandGactionProvider *self = GB_COMMAND_GACTION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_WORKBENCH:
      gb_command_gaction_provider_set_workbench (self,
                                                 g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_gaction_provider_class_init (GbCommandGactionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gb_command_gaction_provider_dispose;
  object_class->get_property = gb_command_gaction_provider_get_property;
  object_class->set_property = gb_command_gaction_provider_set_property;

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("The workbench containing the actions."),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_WORKBENCH,
                                   gParamSpecs [PROP_WORKBENCH]);
}

static void
gb_command_gaction_provider_init (GbCommandGactionProvider *self)
{
  self->priv = gb_command_gaction_provider_get_instance_private (self);
}
