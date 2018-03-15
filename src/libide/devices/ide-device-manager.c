/* ide-device-manager.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-device-manager"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "application/ide-application.h"
#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-pipeline.h"
#include "devices/ide-deploy-strategy.h"
#include "devices/ide-device.h"
#include "devices/ide-device-manager.h"
#include "devices/ide-device-private.h"
#include "devices/ide-device-provider.h"
#include "local/ide-local-device.h"
#include "plugins/ide-extension-util.h"
#include "util/ide-posix.h"

struct _IdeDeviceManager
{
  IdeObject parent_instance;

  /*
   * The currently selected device. Various subsystems will track this
   * to update necessary changes for the device type. For example, the
   * build pipeline will need to adjust things based on the current
   * device to ensure we are building for the right architecture.
   */
  IdeDevice *device;

  /*
   * The devices that have been registered by IdeDeviceProvier plugins.
   * It always has at least one device, the "local" device (IdeLocalDevice).
   */
  GPtrArray *devices;

  /*
   * The providers that are registered in plugins supporting IdeDeviceProvider.
   */
  PeasExtensionSet *providers;

  /*
   * Our menu that contains our list of devices for the user to select. This
   * is "per-IdeContext" so that it is not global to the system (which would
   * result in duplicates for each workbench opened).
   */
  GMenu *menu;
  GMenu *menu_section;

  /*
   * Our progress in a deployment. Simplifies binding to the progress bar
   * in the omnibar.
   */
  gdouble progress;
};

typedef struct
{
  GPtrArray        *strategies;
  IdeBuildPipeline *pipeline;
} DeployState;

static void list_model_init_interface        (GListModelInterface *iface);
static void ide_device_manager_action_device (IdeDeviceManager    *self,
                                              GVariant            *param);
static void ide_device_manager_action_deploy (IdeDeviceManager    *self,
                                              GVariant            *param);
static void ide_device_manager_deploy_tick   (GTask               *task);

DZL_DEFINE_ACTION_GROUP (IdeDeviceManager, ide_device_manager, {
  { "device", ide_device_manager_action_device, "s", "'local'" },
  { "deploy", ide_device_manager_action_deploy },
})

G_DEFINE_TYPE_WITH_CODE (IdeDeviceManager, ide_device_manager, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP,
                                                ide_device_manager_init_action_group)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_init_interface))

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_PROGRESS,
  N_PROPS
};

enum {
  DEPLOY_STARTED,
  DEPLOY_FINISHED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
deploy_state_free (DeployState *state)
{
  g_clear_object (&state->pipeline);
  g_clear_pointer (&state->strategies, g_ptr_array_unref);
  g_slice_free (DeployState, state);
}

static void
ide_device_manager_provider_device_added_cb (IdeDeviceManager  *self,
                                             IdeDevice         *device,
                                             IdeDeviceProvider *provider)
{
  g_autoptr(GMenuItem) menu_item = NULL;
  const gchar *display_name;
  const gchar *icon_name;
  const gchar *device_id;
  guint position;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (IDE_IS_DEVICE (device));
  g_assert (!provider || IDE_IS_DEVICE_PROVIDER (provider));

  device_id = ide_device_get_id (device);
  icon_name = ide_device_get_icon_name (device);
  display_name = ide_device_get_display_name (device);

  IDE_TRACE_MSG ("Discovered device %s", device_id);

  /* First add the device to the array, we'll notify observers later */
  position = self->devices->len;
  g_ptr_array_add (self->devices, g_object_ref (device));

  /* Now add a new menu item to our selection model */
  menu_item = g_menu_item_new (display_name, NULL);
  g_menu_item_set_attribute (menu_item, "id", "s", device_id);
  g_menu_item_set_attribute (menu_item, "verb-icon-name", "s", icon_name ?: "computer-symbolic");
  g_menu_item_set_action_and_target_value (menu_item,
                                           "device-manager.device",
                                           g_variant_new_string (device_id));
  g_menu_append_item (self->menu_section, menu_item);

  /* Now notify about the new device */
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  IDE_EXIT;
}

static void
ide_device_manager_provider_device_removed_cb (IdeDeviceManager  *self,
                                               IdeDevice         *device,
                                               IdeDeviceProvider *provider)
{
  const gchar *device_id;
  GMenu *menu;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (IDE_IS_DEVICE (device));
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  device_id = ide_device_get_id (device);

  menu = self->menu_section;
  n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));

  for (guint i = 0; i < n_items; i++)
    {
      g_autofree gchar *id = NULL;

      if (g_menu_model_get_item_attribute (G_MENU_MODEL (menu), i, "id", "s", &id) &&
          g_strcmp0 (id, device_id) == 0)
        {
          g_menu_remove (menu, i);
          break;
        }
    }

  for (guint i = 0; i < self->devices->len; i++)
    {
      IdeDevice *element = g_ptr_array_index (self->devices, i);

      if (element == device)
        {
          g_ptr_array_remove_index (self->devices, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }

  IDE_EXIT;
}

static void
ide_device_manager_provider_load_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeDeviceProvider *provider = (IdeDeviceProvider *)object;
  g_autoptr(IdeDeviceManager) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DEVICE_MANAGER (self));

  if (!ide_device_provider_load_finish (provider, result, &error))
    g_warning ("%s failed to load: %s",
               G_OBJECT_TYPE_NAME (provider),
               error->message);

  IDE_EXIT;
}

static void
ide_device_manager_provider_added_cb (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      PeasExtension    *exten,
                                      gpointer          user_data)
{
  IdeDeviceManager *self = user_data;
  IdeDeviceProvider *provider = (IdeDeviceProvider *)exten;
  g_autoptr(GPtrArray) devices = NULL;

  IDE_ENTRY;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  g_signal_connect_object (provider,
                           "device-added",
                           G_CALLBACK (ide_device_manager_provider_device_added_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (provider,
                           "device-removed",
                           G_CALLBACK (ide_device_manager_provider_device_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  devices = ide_device_provider_get_devices (provider);

  for (guint i = 0; i < devices->len; i++)
    {
      IdeDevice *device = g_ptr_array_index (devices, i);

      g_assert (IDE_IS_DEVICE (device));

      ide_device_manager_provider_device_added_cb (self, device, provider);
    }

  ide_device_provider_load_async (provider,
                                  NULL,
                                  ide_device_manager_provider_load_cb,
                                  g_object_ref (self));

  IDE_EXIT;
}

static void
ide_device_manager_provider_removed_cb (PeasExtensionSet *set,
                                        PeasPluginInfo   *plugin_info,
                                        PeasExtension    *exten,
                                        gpointer          user_data)
{
  IdeDeviceManager *self = user_data;
  IdeDeviceProvider *provider = (IdeDeviceProvider *)exten;
  g_autoptr(GPtrArray) devices = NULL;

  IDE_ENTRY;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  devices = ide_device_provider_get_devices (provider);

  for (guint i = 0; i < devices->len; i++)
    {
      IdeDevice *removed_device = g_ptr_array_index (devices, i);

      for (guint j = 0; j < self->devices->len; j++)
        {
          IdeDevice *device = g_ptr_array_index (self->devices, j);

          if (device == removed_device)
            {
              g_ptr_array_remove_index (self->devices, j);
              g_list_model_items_changed (G_LIST_MODEL (self), j, 1, 0);
              break;
            }
        }
    }

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager_provider_device_added_cb),
                                        self);

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager_provider_device_removed_cb),
                                        self);

  IDE_EXIT;
}

static void
ide_device_manager_add_providers (IdeDeviceManager *self)
{
  IdeContext *context;

  g_assert (IDE_IS_DEVICE_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            IDE_TYPE_DEVICE_PROVIDER,
                                            "context", context,
                                            NULL);

  g_signal_connect (self->providers,
                    "extension-added",
                    G_CALLBACK (ide_device_manager_provider_added_cb),
                    self);

  g_signal_connect (self->providers,
                    "extension-removed",
                    G_CALLBACK (ide_device_manager_provider_removed_cb),
                    self);

  peas_extension_set_foreach (self->providers,
                              (PeasExtensionSetForeachFunc)ide_device_manager_provider_added_cb,
                              self);
}

static void
ide_device_manager_add_local (IdeDeviceManager *self)
{
  g_autoptr(IdeDevice) device = NULL;
  g_autofree gchar *arch = NULL;
  IdeContext *context;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  device = g_object_new (IDE_TYPE_LOCAL_DEVICE,
                         "context", context,
                         NULL);
  ide_device_manager_provider_device_added_cb (self, device, NULL);

  arch = ide_get_system_arch ();

  /*
   * If we're running on 64-bit intel, also include a 32-bit device
   * that allows the user to build/configure the pipeline for i386.
   */
  if (g_str_equal (arch, "x86_64"))
    {
#if 0
      g_autoptr(IdeDevice) legacy_device = NULL;

      legacy_device = g_object_new (IDE_TYPE_LOCAL_DEVICE,
                                    "arch", "i386",
                                    "context", context,
                                    NULL);
      ide_device_manager_provider_device_added_cb (self, legacy_device, NULL);
#endif
    }
}

static GType
ide_device_manager_get_item_type (GListModel *list_model)
{
  return IDE_TYPE_DEVICE;
}

static guint
ide_device_manager_get_n_items (GListModel *list_model)
{
  IdeDeviceManager *self = (IdeDeviceManager *)list_model;

  g_assert (IDE_IS_DEVICE_MANAGER (self));

  return self->devices->len;
}

gpointer
ide_device_manager_get_item (GListModel *list_model,
                             guint       position)
{
  IdeDeviceManager *self = (IdeDeviceManager *)list_model;

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (position < self->devices->len);

  return g_object_ref (g_ptr_array_index (self->devices, position));
}

static void
ide_device_manager_constructed (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));

  G_OBJECT_CLASS (ide_device_manager_parent_class)->constructed (object);

  ide_device_manager_add_local (self);
  ide_device_manager_add_providers (self);
}

static void
ide_device_manager_dispose (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;

  if (self->devices->len > 0)
    g_ptr_array_remove_range (self->devices, 0, self->devices->len);

  g_clear_object (&self->device);
  g_clear_object (&self->providers);

  G_OBJECT_CLASS (ide_device_manager_parent_class)->dispose (object);
}

static void
ide_device_manager_finalize (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;

  g_clear_pointer (&self->devices, g_ptr_array_unref);
  g_clear_object (&self->menu);
  g_clear_object (&self->menu_section);

  G_OBJECT_CLASS (ide_device_manager_parent_class)->finalize (object);
}

static void
ide_device_manager_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeDeviceManager *self = IDE_DEVICE_MANAGER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, ide_device_manager_get_device (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, ide_device_manager_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_device_manager_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeDeviceManager *self = IDE_DEVICE_MANAGER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      ide_device_manager_set_device (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_device_manager_class_init (IdeDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_device_manager_constructed;
  object_class->dispose = ide_device_manager_dispose;
  object_class->finalize = ide_device_manager_finalize;
  object_class->get_property = ide_device_manager_get_property;
  object_class->set_property = ide_device_manager_set_property;

  /**
   * IdeDeviceManager:device:
   *
   * The "device" property indicates the currently selected device by the
   * user. This is the device we will try to deploy to when running, and
   * execute the application on.
   *
   * Since: 3.28
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The currently selected device to build for",
                         IDE_TYPE_DEVICE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * IdeDeviceManager:progress:
   *
   * The "progress" property is updated with a value between 0.0 and 1.0 while
   * the deployment is in progress.
   *
   * Since: 3.28
   */
  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "Deployment progress",
                         0.0, 1.0, 0.0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [DEPLOY_STARTED] =
    g_signal_new ("deploy-started",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [DEPLOY_FINISHED] =
    g_signal_new ("deploy-finished",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
list_model_init_interface (GListModelInterface *iface)
{
  iface->get_item_type = ide_device_manager_get_item_type;
  iface->get_n_items = ide_device_manager_get_n_items;
  iface->get_item = ide_device_manager_get_item;
}

static void
ide_device_manager_init (IdeDeviceManager *self)
{
  self->devices = g_ptr_array_new_with_free_func (g_object_unref);

  self->menu = g_menu_new ();
  self->menu_section = g_menu_new ();
  g_menu_append_section (self->menu, _("Devices"), G_MENU_MODEL (self->menu_section));
}

/**
 * ide_device_manager_get_device_by_id:
 * @self: an #IdeDeviceManager
 * @device_id: The device identifier string.
 *
 * Fetches the first device that matches the device identifier @device_id.
 *
 * Returns: (transfer none): An #IdeDevice or %NULL.
 */
IdeDevice *
ide_device_manager_get_device_by_id (IdeDeviceManager *self,
                                     const gchar      *device_id)
{
  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), NULL);

  for (guint i = 0; i < self->devices->len; i++)
    {
      IdeDevice *device;
      const gchar *id;

      device = g_ptr_array_index (self->devices, i);
      id = ide_device_get_id (device);

      if (0 == g_strcmp0 (id, device_id))
        return device;
    }

  return NULL;
}

/**
 * ide_device_manager_get_device:
 * @self: a #IdeDeviceManager
 *
 * Gets the currently selected device.
 * Usually, this is an #IdeLocalDevice.
 *
 * Returns: (transfer none) (not nullable): an #IdeDevice
 *
 * Since: 3.28
 */
IdeDevice *
ide_device_manager_get_device (IdeDeviceManager *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), NULL);
  g_return_val_if_fail (self->devices->len > 0, NULL);

  if (self->device == NULL)
    {
      for (guint i = 0; i < self->devices->len; i++)
        {
          IdeDevice *device = g_ptr_array_index (self->devices, i);

          if (IDE_IS_LOCAL_DEVICE (device))
            return device;
        }

      g_assert_not_reached ();
    }

  return self->device;
}

/**
 * ide_device_manager_set_device:
 * @self: an #IdeDeviceManager
 * @device: (nullable): an #IdeDevice or %NULL
 *
 * Sets the #IdeDeviceManager:device property, which is the currently selected
 * device. Builder uses this to determine how to build the current project for
 * the devices architecture and operating system.
 *
 * If @device is %NULL, the local device will be used.
 *
 * Since: 3.28
 */
void
ide_device_manager_set_device (IdeDeviceManager *self,
                               IdeDevice        *device)
{
  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (!device || IDE_IS_DEVICE (device));

  if (g_set_object (&self->device, device))
    {
      const gchar *device_id = NULL;

      if (device != NULL)
        device_id = ide_device_get_id (device);

      if (device_id == NULL)
        device_id = "local";

      ide_device_manager_set_action_state (self, "device", g_variant_new_string (device_id));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEVICE]);
    }
}

static void
ide_device_manager_action_device (IdeDeviceManager *self,
                                  GVariant         *param)
{
  const gchar *device_id;
  IdeDevice *device;

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  if (!(device_id = g_variant_get_string (param, NULL)))
    device_id = "local";

  if ((device = ide_device_manager_get_device_by_id (self, device_id)))
    ide_device_manager_set_device (self, device);
}

static void
log_deploy_error (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_DEVICE_MANAGER (object));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_device_manager_deploy_finish (IDE_DEVICE_MANAGER (object), result, &error))
    ide_object_warning (object, "%s", error->message);
}

static void
ide_device_manager_action_deploy (IdeDeviceManager *self,
                                  GVariant         *param)
{
  IdeBuildPipeline *pipeline;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (IDE_IS_DEVICE_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_context_get_build_manager (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (!ide_build_pipeline_is_ready (pipeline))
    ide_context_warning (context, _("Cannot deploy to device, build pipeline is not initialized"));
  else
    ide_device_manager_deploy_async (self, pipeline, NULL, log_deploy_error, NULL);
}

static void
deploy_progress_cb (goffset  current_num_bytes,
                    goffset  total_num_bytes,
                    gpointer user_data)
{
  IdeDeviceManager *self = user_data;
  gdouble progress = 0.0;

  g_assert (IDE_IS_DEVICE_MANAGER (self));

  if (total_num_bytes > 0)
    progress = current_num_bytes / total_num_bytes;

  self->progress = CLAMP (progress, 0.0, 1.0);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);
}

static void
collect_strategies (PeasExtensionSet *set,
                    PeasPluginInfo   *plugin_info,
                    PeasExtension    *exten,
                    gpointer          user_data)
{
  GPtrArray *strategies = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEPLOY_STRATEGY (exten));
  g_assert (strategies != NULL);

  g_ptr_array_add (strategies, g_object_ref (exten));
}

static void
ide_device_manager_deploy_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeDeployStrategy *strategy = (IdeDeployStrategy *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_DEPLOY_STRATEGY (strategy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_deploy_strategy_deploy_finish (strategy, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_device_manager_deploy_load_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeDeployStrategy *strategy = (IdeDeployStrategy *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  IdeDeviceManager *self;
  DeployState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_DEPLOY_STRATEGY (strategy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_deploy_strategy_load_finish (strategy, result, &error))
    {
      g_debug ("Deploy strategy failed to load: %s", error->message);
      ide_device_manager_deploy_tick (task);
      IDE_EXIT;
    }

  /* Okay, we found a match. Now deploy to the device. */

  self = g_task_get_source_object (task);
  state = g_task_get_task_data (task);

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (state != NULL);
  g_assert (state->strategies != NULL);
  g_assert (IDE_IS_BUILD_PIPELINE (state->pipeline));

  ide_deploy_strategy_deploy_async (strategy,
                                    state->pipeline,
                                    deploy_progress_cb,
                                    g_object_ref (self),
                                    g_object_unref,
                                    g_task_get_cancellable (task),
                                    ide_device_manager_deploy_cb,
                                    g_object_ref (task));

  IDE_EXIT;
}

static void
ide_device_manager_deploy_tick (GTask *task)
{
  g_autoptr(IdeDeployStrategy) strategy = NULL;
  DeployState *state;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (state->strategies != NULL);
  g_assert (IDE_IS_BUILD_PIPELINE (state->pipeline));

  if (state->strategies->len == 0)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Failed to locate deployment strategy for device");
      IDE_EXIT;
    }

  strategy = g_object_ref (g_ptr_array_index (state->strategies, 0));
  g_ptr_array_remove_index (state->strategies, 0);

  ide_deploy_strategy_load_async (strategy,
                                  state->pipeline,
                                  g_task_get_cancellable (task),
                                  ide_device_manager_deploy_load_cb,
                                  g_object_ref (task));

  IDE_EXIT;
}

static void
ide_device_manager_deploy_completed (IdeDeviceManager *self,
                                     GParamSpec       *pspec,
                                     GTask            *task)
{
  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (G_IS_TASK (task));

  if (self->progress < 1.0)
    {
      self->progress = 1.0;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);
    }

  g_signal_emit (self, signals [DEPLOY_FINISHED], 0);
}

/**
 * ide_device_manager_deploy_async:
 * @self: a #IdeDeviceManager
 * @pipeline: an #IdeBuildPipeline
 * @cancellable: a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback
 * @user_data: closure data for @callback
 *
 * Requests that the application be deployed to the device. This may need to
 * be done before running the application so that the device has the most
 * up to date build.
 *
 * Since: 3.28
 */
void
ide_device_manager_deploy_async (IdeDeviceManager    *self,
                                 IdeBuildPipeline    *pipeline,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(PeasExtensionSet) set = NULL;
  g_autoptr(GTask) task = NULL;
  DeployState *state;
  IdeContext *context;
  IdeDevice *device;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->progress = 0.0;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);

  g_signal_emit (self, signals [DEPLOY_STARTED], 0);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_device_manager_deploy_async);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_device_manager_deploy_completed),
                           self,
                           G_CONNECT_SWAPPED);

  if (!(device = ide_build_pipeline_get_device (pipeline)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Missing device in pipeline");
      IDE_EXIT;
    }

  if (IDE_IS_LOCAL_DEVICE (device))
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  state = g_slice_new0 (DeployState);
  state->pipeline = g_object_ref (pipeline);
  state->strategies = g_ptr_array_new_with_free_func (g_object_unref);
  g_task_set_task_data (task, state, (GDestroyNotify)deploy_state_free);

  context = ide_object_get_context (IDE_OBJECT (self));
  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_DEPLOY_STRATEGY,
                                "context", context,
                                NULL);
  peas_extension_set_foreach (set, collect_strategies, state->strategies);

  ide_device_manager_deploy_tick (task);

  IDE_EXIT;
}

/**
 * ide_device_manager_deploy_finish:
 * @self: a #IdeDeviceManager
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a request to deploy the application to the device.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set
 *
 * Since: 3.28
 */
gboolean
ide_device_manager_deploy_finish (IdeDeviceManager  *self,
                                  GAsyncResult      *result,
                                  GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

gdouble
ide_device_manager_get_progress (IdeDeviceManager *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), 0.0);

  return self->progress;
}

GMenu *
_ide_device_manager_get_menu (IdeDeviceManager *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), NULL);

  return self->menu;
}
