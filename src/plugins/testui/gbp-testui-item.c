/* gbp-testui-item.c
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

#define G_LOG_DOMAIN "gbp-testui-item"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-testui-item.h"

struct _GbpTestuiItem
{
  GObject  parent_instance;
  gpointer instance;
};

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_EXPANDED_ICON_NAME,
  PROP_INSTANCE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpTestuiItem, gbp_testui_item, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static const char *
gbp_testui_item_get_icon_name (GbpTestuiItem *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_ITEM (self));

  if (IDE_IS_TEST_MANAGER (self->instance))
    return "folder-symbolic";

  if (IDE_IS_TEST (self->instance))
    return ide_test_get_icon_name (self->instance);

  return NULL;
}

static const char *
gbp_testui_item_get_expanded_icon_name (GbpTestuiItem *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_ITEM (self));

  if (IDE_IS_TEST_MANAGER (self->instance))
    return "folder-open-symbolic";

  return gbp_testui_item_get_icon_name (self);
}

static const char *
gbp_testui_item_get_title (GbpTestuiItem *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_ITEM (self));

  if (IDE_IS_TEST_MANAGER (self->instance))
    return _("Unit Tests");

  if (IDE_IS_TEST (self->instance))
    return ide_test_get_title (self->instance);

  return NULL;
}

static void
gbp_testui_item_notify_icon_name_cb (GbpTestuiItem *self,
                                     GParamSpec    *pspec,
                                     IdeTest       *test)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_ITEM (self));
  g_assert (IDE_IS_TEST (test));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
}

static void
gbp_testui_item_set_instance (GbpTestuiItem *self,
                              gpointer       instance)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_ITEM (self));
  g_assert (G_IS_OBJECT (instance));

  self->instance = g_object_ref (instance);

  if (IDE_IS_TEST (instance))
    g_signal_connect_object (instance,
                             "notify::icon-name",
                             G_CALLBACK (gbp_testui_item_notify_icon_name_cb),
                             self,
                             G_CONNECT_SWAPPED);
}

static void
gbp_testui_item_dispose (GObject *object)
{
  GbpTestuiItem *self = (GbpTestuiItem *)object;

  g_clear_object (&self->instance);

  G_OBJECT_CLASS (gbp_testui_item_parent_class)->dispose (object);
}

static void
gbp_testui_item_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbpTestuiItem *self = GBP_TESTUI_ITEM (object);

  switch (prop_id)
    {
    case PROP_INSTANCE:
      g_value_set_object (value, self->instance);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, gbp_testui_item_get_icon_name (self));
      break;

    case PROP_EXPANDED_ICON_NAME:
      g_value_set_string (value, gbp_testui_item_get_expanded_icon_name (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gbp_testui_item_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_testui_item_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbpTestuiItem *self = GBP_TESTUI_ITEM (object);

  switch (prop_id)
    {
    case PROP_INSTANCE:
      gbp_testui_item_set_instance (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_testui_item_class_init (GbpTestuiItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_testui_item_dispose;
  object_class->get_property = gbp_testui_item_get_property;
  object_class->set_property = gbp_testui_item_set_property;

  properties [PROP_INSTANCE] =
    g_param_spec_object ("instance", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_EXPANDED_ICON_NAME] =
    g_param_spec_string ("expanded-icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_testui_item_init (GbpTestuiItem *self)
{
}

GbpTestuiItem *
gbp_testui_item_new (gpointer instance)
{
  g_return_val_if_fail (instance != NULL, NULL);
  g_return_val_if_fail (G_IS_OBJECT (instance), NULL);

  g_assert (IDE_IS_TEST (instance) ||
            IDE_IS_TEST_MANAGER (instance));

  return g_object_new (GBP_TYPE_TESTUI_ITEM,
                       "instance", instance,
                       NULL);
}

gpointer
gbp_testui_item_map_func (gpointer item,
                          gpointer user_data)
{
  gpointer ret = gbp_testui_item_new (item);
  g_object_unref (item);
  return ret;
}

GListModel *
gbp_testui_item_create_child_model (gpointer item,
                                    gpointer user_data)
{
  GbpTestuiItem *self = item;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (GBP_IS_TESTUI_ITEM (self), NULL);

  if (IDE_IS_TEST_MANAGER (self->instance))
    {
      GListModel *tests = ide_test_manager_list_tests (self->instance);
      GtkMapListModel *map = gtk_map_list_model_new (g_object_ref (tests),
                                                     gbp_testui_item_map_func,
                                                     NULL, NULL);
      return G_LIST_MODEL (ide_cached_list_model_new (G_LIST_MODEL (map)));
    }

  /* TODO: We could insert information about the test run here, like
   *       a list of passed or failed tests if we have a protocol to
   *       extract that from the unit test.
   */

  return NULL;
}

gpointer
gbp_testui_item_get_instance (GbpTestuiItem *self)
{
  g_return_val_if_fail (GBP_IS_TESTUI_ITEM (self), NULL);

  return self->instance;
}
