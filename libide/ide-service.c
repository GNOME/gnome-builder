/* ide-service.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-service.h"

typedef struct
{
  guint running : 1;
} IdeServicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeService, ide_service, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  PROP_RUNNING,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

gboolean
ide_service_get_running (IdeService *service)
{
  IdeServicePrivate *priv = ide_service_get_instance_private (service);

  g_return_val_if_fail (IDE_IS_SERVICE (service), FALSE);

  return priv->running;
}

const gchar *
ide_service_get_name (IdeService *service)
{
  g_return_val_if_fail (IDE_IS_SERVICE (service), NULL);

  if (IDE_SERVICE_GET_CLASS (service)->get_name)
    return IDE_SERVICE_GET_CLASS (service)->get_name (service);

  return NULL;
}

void
ide_service_start (IdeService *service)
{
  IdeServicePrivate *priv = ide_service_get_instance_private (service);

  g_return_if_fail (IDE_IS_SERVICE (service));

  if (!priv->running)
    {
      if (IDE_SERVICE_GET_CLASS (service)->start)
        IDE_SERVICE_GET_CLASS (service)->start (service);

      priv->running = TRUE;
    }
}

void
ide_service_stop (IdeService *service)
{
  IdeServicePrivate *priv = ide_service_get_instance_private (service);

  g_return_if_fail (IDE_IS_SERVICE (service));

  if (priv->running)
    {
      if (IDE_SERVICE_GET_CLASS (service)->stop)
        IDE_SERVICE_GET_CLASS (service)->stop (service);

      priv->running = FALSE;
    }
}

static void
ide_service_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeService *self = IDE_SERVICE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ide_service_get_name (self));
      break;

    case PROP_RUNNING:
      g_value_set_boolean (value, ide_service_get_running (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_service_class_init (IdeServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_service_get_property;

  gParamSpecs [PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The name of the service."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NAME,
                                   gParamSpecs [PROP_NAME]);

  gParamSpecs [PROP_RUNNING] =
    g_param_spec_boolean ("running",
                          _("Running"),
                          _("If the service is running."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_RUNNING,
                                   gParamSpecs [PROP_RUNNING]);
}

static void
ide_service_init (IdeService *self)
{
}
