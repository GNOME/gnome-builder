/* ide-buffer-addin.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-buffer-addin"

#include <libpeas/peas.h>

#include "buffers/ide-buffer-addin.h"
#include "buffers/ide-buffer-private.h"

/**
 * SECTION:ide-buffer-addin
 * @title: IdeBufferAddin
 * @short_description: addins for #IdeBuffer
 *
 * The #IdeBufferAddin allows a plugin to register an object that will be
 * created with every #IdeBuffer. It can register extra features with the
 * buffer or extend it as necessary.
 *
 * Once use of #IdeBufferAddin is to add a spellchecker to the buffer that
 * may be used by views to show the misspelled words. This is preferrable
 * to adding a spellchecker in each view because it allows for multiple
 * views to share one spellcheker on the underlying buffer.
 */

G_DEFINE_INTERFACE (IdeBufferAddin, ide_buffer_addin, G_TYPE_OBJECT)

static void
ide_buffer_addin_default_init (IdeBufferAddinInterface *iface)
{
}

/**
 * ide_buffer_addin_load:
 * @self: an #IdeBufferAddin
 * @buffer: an #IdeBuffer
 *
 * This calls the load virtual function of #IdeBufferAddin to request
 * that the addin load itself.
 *
 * Since: 3.26
 */
void
ide_buffer_addin_load (IdeBufferAddin *self,
                       IdeBuffer      *buffer)
{
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->load)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->load (self, buffer);
}

/**
 * ide_buffer_addin_unload:
 * @self: an #IdeBufferAddin
 * @buffer: an #IdeBuffer
 *
 * This calls the unload virtual function of #IdeBufferAddin to request
 * that the addin unload itself.
 *
 * The addin should cancel any in-flight operations and attempt to drop
 * references to the buffer or any other machinery as soon as possible.
 *
 * Since: 3.26
 */
void
ide_buffer_addin_unload (IdeBufferAddin *self,
                         IdeBuffer      *buffer)
{
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->unload)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->unload (self, buffer);
}

/**
 * ide_buffer_addin_find_by_module_name:
 * @buffer: an #IdeBuffer
 * @module_name: the module name of the addin
 *
 * Locates an addin attached to the #IdeBuffer by the name of the module
 * that provides the addin.
 *
 * Returns: (transfer none) (nullable): An #IdeBufferAddin or %NULL
 *
 * Since: 3.26
 */
IdeBufferAddin *
ide_buffer_addin_find_by_module_name (IdeBuffer   *buffer,
                                      const gchar *module_name)
{
  PeasPluginInfo *plugin_info;
  PeasExtensionSet *set;
  PeasExtension *ret = NULL;

  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  set = _ide_buffer_get_addins (buffer);

  /* Addins might not be loaded */
  if (set == NULL)
    return NULL;

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), module_name);

  if (plugin_info != NULL)
    ret = peas_extension_set_get_extension (set, plugin_info);
  else
    g_warning ("Failed to locate addin named %s", module_name);

  return ret ? IDE_BUFFER_ADDIN (ret) : NULL;
}
