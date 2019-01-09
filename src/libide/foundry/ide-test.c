/* ide-test.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-test"

#include "config.h"

#include "ide-foundry-enums.h"

#include "ide-test.h"
#include "ide-test-private.h"
#include "ide-test-provider.h"

typedef struct
{
  /* Unowned references */
  IdeTestProvider *provider;

  /* Owned references */
  gchar *display_name;
  gchar *group;
  gchar *id;

  IdeTestStatus status;
} IdeTestPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeTest, ide_test, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_GROUP,
  PROP_ID,
  PROP_STATUS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeTest *
ide_test_new (void)
{
  return g_object_new (IDE_TYPE_TEST, NULL);
}

static void
ide_test_finalize (GObject *object)
{
  IdeTest *self = (IdeTest *)object;
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  priv->provider = NULL;

  g_clear_pointer (&priv->group, g_free);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->display_name, g_free);

  G_OBJECT_CLASS (ide_test_parent_class)->finalize (object);
}

static void
ide_test_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  IdeTest *self = IDE_TEST (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_test_get_id (self));
      break;

    case PROP_GROUP:
      g_value_set_string (value, ide_test_get_group (self));
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_test_get_display_name (self));
      break;

    case PROP_STATUS:
      g_value_set_enum (value, ide_test_get_status (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  IdeTest *self = IDE_TEST (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      ide_test_set_group (self, g_value_get_string (value));
      break;

    case PROP_ID:
      ide_test_set_id (self, g_value_get_string (value));
      break;

    case PROP_DISPLAY_NAME:
      ide_test_set_display_name (self, g_value_get_string (value));
      break;

    case PROP_STATUS:
      ide_test_set_status (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_class_init (IdeTestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_test_finalize;
  object_class->get_property = ide_test_get_property;
  object_class->set_property = ide_test_set_property;

  /**
   * IdeTest:display_name:
   *
   * The "display-name" property contains the display name of the test as
   * the user is expected to read in UI elements.
   *
   * Since: 3.32
   */
  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Name",
                         "The display_name of the unit test",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTest:id:
   *
   * The "id" property contains the unique identifier of the test.
   *
   * Since: 3.32
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The unique identifier of the test",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTest:group:
   *
   * The "group" property contains the name of the gruop the test belongs
   * to, if any.
   *
   * Since: 3.32
   */
  properties [PROP_GROUP] =
    g_param_spec_string ("group",
                         "Group",
                         "The name of the group the test belongs to, if any",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTest::status:
   *
   * The "status" property contains the status of the test, updated by
   * providers when they have run the test.
   *
   * Since: 3.32
   */
  properties [PROP_STATUS] =
    g_param_spec_enum ("status",
                       "Status",
                       "The status of the test",
                       IDE_TYPE_TEST_STATUS,
                       IDE_TEST_STATUS_NONE,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_test_init (IdeTest *self)
{
}

IdeTestProvider *
_ide_test_get_provider (IdeTest *self)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEST (self), NULL);

  return priv->provider;
}

void
_ide_test_set_provider (IdeTest         *self,
                        IdeTestProvider *provider)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEST (self));
  g_return_if_fail (!provider || IDE_IS_TEST_PROVIDER (provider));

  priv->provider = provider;
}

/**
 * ide_test_get_display_name:
 * @self: An #IdeTest
 *
 * Gets the "display-name" property of the test.
 *
 * Returns: (nullable): The display_name of the test or %NULL
 *
 * Since: 3.32
 */
const gchar *
ide_test_get_display_name (IdeTest *self)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEST (self), NULL);

  return priv->display_name;
}

/**
 * ide_test_set_display_name:
 * @self: An #IdeTest
 * @display_name: (nullable): The display_name of the test, or %NULL to unset
 *
 * Sets the "display-name" property of the unit test.
 *
 * Since: 3.32
 */
void
ide_test_set_display_name (IdeTest     *self,
                           const gchar *display_name)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEST (self));

  if (g_strcmp0 (display_name, priv->display_name) != 0)
    {
      g_free (priv->display_name);
      priv->display_name = g_strdup (display_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
    }
}

/**
 * ide_test_get_group:
 * @self: a #IdeTest
 *
 * Gets the "group" property.
 *
 * The group name is used to group tests together.
 *
 * Returns: (nullable): The group name or %NULL.
 *
 * Since: 3.32
 */
const gchar *
ide_test_get_group (IdeTest *self)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEST (self), NULL);

  return priv->group;
}

/**
 * ide_test_set_group:
 * @self: a #IdeTest
 * @group: (nullable): the name of the group or %NULL
 *
 * Sets the #IdeTest:group property.
 *
 * The group property is used to group related tests together.
 *
 * Since: 3.32
 */
void
ide_test_set_group (IdeTest     *self,
                    const gchar *group)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEST (self));

  if (g_strcmp0 (group, priv->group) != 0)
    {
      g_free (priv->group);
      priv->group = g_strdup (group);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GROUP]);
    }
}

/**
 * ide_test_get_id:
 * @self: a #IdeTest
 *
 * Gets the #IdeTest:id property.
 *
 * Returns: (nullable): The id of the test, or %NULL if it has not been set.
 *
 * Since: 3.32
 */
const gchar *
ide_test_get_id (IdeTest *self)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEST (self), NULL);

  return priv->id;
}

/**
 * ide_test_set_id:
 * @self: a #IdeTest
 * @id: (nullable): the id of the test or %NULL
 *
 * Sets the #IdeTest:id property.
 *
 * The id property is used to uniquely identify the test.
 *
 * Since: 3.32
 */
void
ide_test_set_id (IdeTest     *self,
                 const gchar *id)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEST (self));

  if (g_strcmp0 (id, priv->id) != 0)
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

IdeTestStatus
ide_test_get_status (IdeTest *self)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEST (self), 0);

  return priv->status;
}

void
ide_test_set_status (IdeTest       *self,
                     IdeTestStatus  status)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEST (self));

  if (priv->status != status)
    {
      priv->status = status;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STATUS]);
    }
}

const gchar *
ide_test_get_icon_name (IdeTest *self)
{
  IdeTestPrivate *priv = ide_test_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEST (self), NULL);

  switch (priv->status)
    {
    case IDE_TEST_STATUS_NONE:
      return "builder-unit-tests-symbolic";

    case IDE_TEST_STATUS_RUNNING:
      return "builder-unit-tests-running-symbolic";

    case IDE_TEST_STATUS_FAILED:
      return "builder-unit-tests-fail-symbolic";

    case IDE_TEST_STATUS_SUCCESS:
      return "builder-unit-tests-pass-symbolic";

    default:
      g_return_val_if_reached (NULL);
    }
}
