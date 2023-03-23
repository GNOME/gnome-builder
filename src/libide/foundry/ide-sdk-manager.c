/* ide-sdk-manager.c
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

#define G_LOG_DOMAIN "ide-sdk-manager"

#include "config.h"

#include <gtk/gtk.h>
#include <libpeas.h>

#include "ide-sdk.h"
#include "ide-sdk-manager.h"
#include "ide-sdk-provider.h"

struct _IdeSdkManager
{
  GObject              parent_instance;
  GListStore          *providers;
  GtkFlattenListModel *sdks;
  PeasExtensionSet    *addins;
};

static GType
ide_sdk_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_SDK;
}

static guint
ide_sdk_manager_get_n_items (GListModel *model)
{
  IdeSdkManager *self = IDE_SDK_MANAGER (model);

  return self->sdks ? g_list_model_get_n_items (G_LIST_MODEL (self->sdks)) : 0;
}

static gpointer
ide_sdk_manager_get_item (GListModel *model,
                          guint       position)
{
  IdeSdkManager *self = IDE_SDK_MANAGER (model);

  return self->sdks ? g_list_model_get_item (G_LIST_MODEL (self->sdks), position) : NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_sdk_manager_get_item_type;
  iface->get_n_items = ide_sdk_manager_get_n_items;
  iface->get_item = ide_sdk_manager_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeSdkManager, ide_sdk_manager, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_N_ITEMS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_sdk_manager_extension_added_cb (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    GObject    *exten,
                                    gpointer          user_data)
{
  IdeSdkProvider *provider = (IdeSdkProvider *)exten;
  IdeSdkManager *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SDK_PROVIDER (provider));
  g_assert (IDE_IS_SDK_MANAGER (self));

  g_list_store_append (self->providers, provider);
}

static void
ide_sdk_manager_extension_removed_cb (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject    *exten,
                                      gpointer          user_data)
{
  IdeSdkProvider *provider = (IdeSdkProvider *)exten;
  IdeSdkManager *self = user_data;
  guint n_items;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SDK_PROVIDER (provider));
  g_assert (IDE_IS_SDK_MANAGER (self));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->providers));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeSdkProvider) element = g_list_model_get_item (G_LIST_MODEL (self->providers), i);

      if (element == provider)
        {
          g_list_store_remove (self->providers, i);
          break;
        }
    }
}

static void
ide_sdk_manager_items_changed_cb (IdeSdkManager *self,
                                  guint          position,
                                  guint          removed,
                                  guint          added,
                                  GListModel    *model)
{
  g_assert (IDE_IS_SDK_MANAGER (self));
  g_assert (G_IS_LIST_MODEL (model));

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);

  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ITEMS]);
}

static void
ide_sdk_manager_constructed (GObject *object)
{
  IdeSdkManager *self = (IdeSdkManager *)object;

  G_OBJECT_CLASS (ide_sdk_manager_parent_class)->constructed (object);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_SDK_PROVIDER,
                                         NULL);
  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_sdk_manager_extension_added_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_sdk_manager_extension_removed_cb),
                    self);
  peas_extension_set_foreach (self->addins,
                              ide_sdk_manager_extension_added_cb,
                              self);
}

static void
ide_sdk_manager_dispose (GObject *object)
{
  IdeSdkManager *self = (IdeSdkManager *)object;

  g_clear_object (&self->sdks);
  g_clear_object (&self->addins);
  g_clear_object (&self->providers);

  G_OBJECT_CLASS (ide_sdk_manager_parent_class)->dispose (object);
}

static void
ide_sdk_manager_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeSdkManager *self = IDE_SDK_MANAGER (object);

  switch (prop_id)
    {
    case PROP_N_ITEMS:
      g_value_set_uint (value, g_list_model_get_n_items (G_LIST_MODEL (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_sdk_manager_class_init (IdeSdkManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_sdk_manager_constructed;
  object_class->dispose = ide_sdk_manager_dispose;
  object_class->get_property = ide_sdk_manager_get_property;

  properties [PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL, 0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_sdk_manager_init (IdeSdkManager *self)
{
  self->providers = g_list_store_new (G_TYPE_LIST_MODEL);
  self->sdks = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->providers)));

  g_signal_connect_object (self->sdks,
                           "items-changed",
                           G_CALLBACK (ide_sdk_manager_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

/**
 * ide_sdk_manager_get_default:
 *
 * Gets the #IdeSdkManager instance.
 *
 * Returns: (transfer none): an #IdeSdkManager
 */
IdeSdkManager *
ide_sdk_manager_get_default (void)
{
  static IdeSdkManager *instance;

  if (g_once_init_enter (&instance))
    {
      IdeSdkManager *self = g_object_new (IDE_TYPE_SDK_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&instance);
      g_once_init_leave (&instance, self);
    }

  return instance;
}
