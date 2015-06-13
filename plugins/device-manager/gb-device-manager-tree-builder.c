/* gb-device-manager-tree-builder.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <ide.h>

#include "gb-device-manager-tree-builder.h"

struct _GbDeviceManagerTreeBuilder
{
  GbTreeBuilder parent_instance;
};

G_DEFINE_TYPE (GbDeviceManagerTreeBuilder, gb_device_manager_tree_builder, GB_TYPE_TREE_BUILDER)

static void
gb_device_manager_tree_builder_build_node (GbTreeBuilder *builder,
                                           GbTreeNode    *node)
{
  GObject *item;

  g_assert (GB_IS_TREE_BUILDER (builder));
  g_assert (GB_IS_TREE_NODE (node));

  item = gb_tree_node_get_item (node);

  if (IDE_IS_DEVICE_MANAGER (item))
    {
      g_autoptr(GPtrArray) devices = NULL;
      gsize i;

      devices = ide_device_manager_get_devices (IDE_DEVICE_MANAGER (item));

      for (i = 0; i < devices->len; i++)
        {
          IdeDevice *device;
          GbTreeNode *child;

          device = g_ptr_array_index (devices, i);

          child = g_object_new (GB_TYPE_TREE_NODE,
                                "item", device,
                                "icon-name", "computer-symbolic",
                                NULL);
          g_object_bind_property (device, "display-name", child, "text", G_BINDING_SYNC_CREATE);
          gb_tree_node_append (node, child);
        }
    }
  else if (IDE_IS_DEVICE (item))
    {
    }
}

static void
gb_device_manager_tree_builder_class_init (GbDeviceManagerTreeBuilderClass *klass)
{
  GbTreeBuilderClass *builder_class = GB_TREE_BUILDER_CLASS (klass);

  builder_class->build_node = gb_device_manager_tree_builder_build_node;
}

static void
gb_device_manager_tree_builder_init (GbDeviceManagerTreeBuilder *self)
{
}
