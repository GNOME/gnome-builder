/* ide-log-item.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-log-item"

#include "config.h"

#include "ide-log-item-private.h"

enum {
  PROP_0,
  PROP_CREATED_AT,
  PROP_DOMAIN,
  PROP_MESSAGE,
  PROP_SEVERITY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeLogItem, ide_log_item, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_log_item_finalize (GObject *object)
{
  IdeLogItem *self = (IdeLogItem *)object;

  g_clear_pointer (&self->message, g_free);
  g_clear_pointer (&self->created_at, g_date_time_unref);

  G_OBJECT_CLASS (ide_log_item_parent_class)->finalize (object);
}

static void
ide_log_item_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeLogItem *self = IDE_LOG_ITEM (object);

  switch (prop_id)
    {
    case PROP_DOMAIN:
      g_value_set_static_string (value, ide_log_item_get_domain (self));
      break;

    case PROP_CREATED_AT:
      g_value_set_boxed (value, ide_log_item_get_created_at (self));
      break;

    case PROP_MESSAGE:
      g_value_set_string (value, ide_log_item_get_message (self));
      break;

    case PROP_SEVERITY:
      g_value_set_uint (value, ide_log_item_get_severity (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_log_item_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeLogItem *self = IDE_LOG_ITEM (object);

  switch (prop_id)
    {
    case PROP_DOMAIN:
      self->domain = g_intern_string (g_value_get_string (value));
      break;

    case PROP_MESSAGE:
      self->message = g_value_dup_string (value);
      break;

    case PROP_CREATED_AT:
      self->created_at = g_value_dup_boxed (value);
      break;

    case PROP_SEVERITY:
      self->severity = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_log_item_class_init (IdeLogItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_log_item_finalize;
  object_class->get_property = ide_log_item_get_property;
  object_class->set_property = ide_log_item_set_property;

  properties [PROP_MESSAGE] =
    g_param_spec_string ("message", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CREATED_AT] =
    g_param_spec_boxed ("created-at", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DOMAIN] =
    g_param_spec_string ("domain", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEVERITY] =
    g_param_spec_uint ("severity", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_log_item_init (IdeLogItem *self)
{
}

IdeLogItem *
_ide_log_item_new (GLogLevelFlags  severity,
                   const char     *domain,
                   const char     *message,
                   GDateTime      *created_at)
{
  g_autoptr(GDateTime) now = NULL;

  g_return_val_if_fail (domain != NULL, NULL);
  g_return_val_if_fail (message != NULL, NULL);

  if (created_at == NULL)
    created_at = now = g_date_time_new_now_local ();

  return g_object_new (IDE_TYPE_LOG_ITEM,
                       "domain", domain,
                       "message", message,
                       "created-at", created_at,
                       "severity", severity,
                       NULL);
}

/**
 * ide_log_item_get_message:
 * @self: a #IdeLogItem
 *
 * Gets the log message.
 *
 * Returns: A string containing the log message
 *
 * Since: 44
 */
const char *
ide_log_item_get_message (IdeLogItem *self)
{
  g_return_val_if_fail (IDE_IS_LOG_ITEM (self), NULL);

  return self->message;
}

/**
 * ide_log_item_get_created_at:
 * @self: a #IdeLogItem
 *
 * Gets the time the log item was created.
 *
 * Returns: (transfer none): a #GDateTime
 *
 * Since: 44
 */
GDateTime *
ide_log_item_get_created_at (IdeLogItem *self)
{
  g_return_val_if_fail (IDE_IS_LOG_ITEM (self), NULL);

  return self->created_at;
}

/**
 * ide_log_item_get_domain:
 * @self: a #IdeLogItem
 *
 * Get the domain for the log item.
 *
 * Since: 44
 */
const char *
ide_log_item_get_domain (IdeLogItem *self)
{
  g_return_val_if_fail (IDE_IS_LOG_ITEM (self), NULL);

  return self->domain;
}

/**
 * ide_log_item_get_severity
 * @self: a #IdeLogItem
 *
 * Gets the log item severity.
 *
 * Since: 44
 */
GLogLevelFlags
ide_log_item_get_severity (IdeLogItem *self)
{
  g_return_val_if_fail (IDE_IS_LOG_ITEM (self), 0);

  return self->severity;
}
