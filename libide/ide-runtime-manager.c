/* ide-runtime-manager.c
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

#define G_LOG_DOMAIN "ide-runtime-manager"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-runtime.h"
#include "ide-runtime-manager.h"
#include "ide-runtime-provider.h"

struct _IdeRuntimeManager
{
  IdeObject         parent_instance;
  PeasExtensionSet *extensions;
  GPtrArray        *runtimes;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeRuntimeManager, ide_runtime_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ide_runtime_manager_extension_added (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     PeasExtension    *exten,
                                     gpointer          user_data)
{
  IdeRuntimeManager *self = user_data;
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));

  ide_runtime_provider_load (provider, self);
}

static void
ide_runtime_manager_extension_removed (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeRuntimeManager *self = user_data;
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));

  ide_runtime_provider_unload (provider, self);
}

static void
ide_runtime_manager_constructed (GObject *object)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)object;
  IdeContext *context;

  G_OBJECT_CLASS (ide_runtime_manager_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_RUNTIME_PROVIDER,
                                             NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (ide_runtime_manager_extension_added),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (ide_runtime_manager_extension_removed),
                    self);

  peas_extension_set_foreach (self->extensions,
                              ide_runtime_manager_extension_added,
                              self);

  ide_runtime_manager_add (self, ide_runtime_new (context, "host", _("Host operating system")));
}

void
_ide_runtime_manager_unload (IdeRuntimeManager *self)
{
  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));

  g_clear_object (&self->extensions);
}

static void
ide_runtime_manager_dispose (GObject *object)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)object;

  g_clear_object (&self->extensions);
  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_runtime_manager_parent_class)->dispose (object);
}

static void
ide_runtime_manager_class_init (IdeRuntimeManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_runtime_manager_constructed;
  object_class->dispose = ide_runtime_manager_dispose;
}

static void
ide_runtime_manager_init (IdeRuntimeManager *self)
{
  self->runtimes = g_ptr_array_new_with_free_func (g_object_unref);
}

static GType
ide_runtime_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_RUNTIME;
}

static guint
ide_runtime_manager_get_n_items (GListModel *model)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)model;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), 0);

  return self->runtimes->len;
}

static gpointer
ide_runtime_manager_get_item (GListModel *model,
                              guint       position)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)model;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), NULL);
  g_return_val_if_fail (position < self->runtimes->len, NULL);

  return g_object_ref (g_ptr_array_index (self->runtimes, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_runtime_manager_get_item_type;
  iface->get_n_items = ide_runtime_manager_get_n_items;
  iface->get_item = ide_runtime_manager_get_item;
}

void
ide_runtime_manager_add (IdeRuntimeManager *self,
                         IdeRuntime        *runtime)
{
  guint idx;

  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));
  g_return_if_fail (IDE_IS_RUNTIME (runtime));

  idx = self->runtimes->len;
  g_ptr_array_add (self->runtimes, g_object_ref (runtime));
  g_list_model_items_changed (G_LIST_MODEL (self), idx, 0, 1);
}

void
ide_runtime_manager_remove (IdeRuntimeManager *self,
                            IdeRuntime        *runtime)
{
  guint i;

  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));
  g_return_if_fail (IDE_IS_RUNTIME (runtime));

  for (i = 0; i < self->runtimes->len; i++)
    {
      IdeRuntime *item = g_ptr_array_index (self->runtimes, i);

      if (runtime == item)
        {
          g_ptr_array_remove_index (self->runtimes, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }
}

/**
 * ide_runtime_manager_get_runtime:
 * @self: An #IdeRuntimeManager
 * @id: the identifier of the runtime
 *
 * Gets the runtime by it's internal identifier.
 *
 * Returns: (transfer none): An #IdeRuntime.
 */
IdeRuntime *
ide_runtime_manager_get_runtime (IdeRuntimeManager *self,
                                 const gchar       *id)
{
  guint i;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  for (i = 0; i < self->runtimes->len; i++)
    {
      IdeRuntime *runtime = g_ptr_array_index (self->runtimes, i);
      const gchar *runtime_id;

      runtime_id = ide_runtime_get_id (runtime);

      if (g_strcmp0 (runtime_id, id) == 0)
        return runtime;
    }

  return NULL;
}
