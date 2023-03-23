/* ide-buffer-addin.c
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

#define G_LOG_DOMAIN "ide-buffer-addin"

#include "config.h"

#include <libide-threading.h>
#include <libpeas.h>

#include "ide-buffer.h"
#include "ide-buffer-addin.h"
#include "ide-buffer-addin-private.h"
#include "ide-buffer-private.h"

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
ide_buffer_addin_real_settle_async (IdeBufferAddin      *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buffer_addin_real_settle_async);
  ide_task_set_priority (task, G_PRIORITY_HIGH);
  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_buffer_addin_real_settle_finish (IdeBufferAddin  *self,
                                     GAsyncResult    *result,
                                     GError         **error)
{
  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_buffer_addin_default_init (IdeBufferAddinInterface *iface)
{
  iface->settle_async = ide_buffer_addin_real_settle_async;
  iface->settle_finish = ide_buffer_addin_real_settle_finish;
}

/**
 * ide_buffer_addin_load:
 * @self: an #IdeBufferAddin
 * @buffer: an #IdeBuffer
 *
 * This calls the load virtual function of #IdeBufferAddin to request
 * that the addin load itself.
 */
void
ide_buffer_addin_load (IdeBufferAddin *self,
                       IdeBuffer      *buffer)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
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
 */
void
ide_buffer_addin_unload (IdeBufferAddin *self,
                         IdeBuffer      *buffer)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->unload)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->unload (self, buffer);
}

/**
 * ide_buffer_addin_file_loaded:
 * @self: a #IdeBufferAddin
 * @buffer: an #IdeBuffer
 * @file: a #GFile
 *
 * This function is called for an addin after a file has been loaded from disk.
 *
 * It is not guaranteed that this function will be called for addins that were
 * loaded after the buffer already loaded a file.
 */
void
ide_buffer_addin_file_loaded (IdeBufferAddin *self,
                              IdeBuffer      *buffer,
                              GFile          *file)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (G_IS_FILE (file));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->file_loaded)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->file_loaded (self, buffer, file);
}

/**
 * ide_buffer_addin_save_file:
 * @self: a #IdeBufferAddin
 * @buffer: an #IdeBuffer
 * @file: a #GFile
 *
 * This function gives a chance for plugins to modify the buffer right before
 * writing to disk.
 */
void
ide_buffer_addin_save_file (IdeBufferAddin *self,
                            IdeBuffer      *buffer,
                            GFile          *file)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (G_IS_FILE (file));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->save_file)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->save_file (self, buffer, file);
}

/**
 * ide_buffer_addin_file_saved:
 * @self: a #IdeBufferAddin
 * @buffer: an #IdeBuffer
 * @file: a #GFile
 *
 * This function is called for an addin after a file has been saved to disk.
 */
void
ide_buffer_addin_file_saved (IdeBufferAddin *self,
                             IdeBuffer      *buffer,
                             GFile          *file)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (G_IS_FILE (file));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->file_saved)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->file_saved (self, buffer, file);
}

/**
 * ide_buffer_addin_language_set:
 * @self: an #IdeBufferAddin
 * @buffer: an #IdeBuffer
 * @language_id: the GtkSourceView language identifier
 *
 * This vfunc is called when the source language in the buffer changes. This
 * will only be delivered to addins that support multiple languages.
 */
void
ide_buffer_addin_language_set (IdeBufferAddin *self,
                               IdeBuffer      *buffer,
                               const gchar    *language_id)
{
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->language_set)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->language_set (self, buffer, language_id);
}

/**
 * ide_buffer_addin_change_settled:
 * @self: an #IdeBufferAddin
 * @buffer: an #ideBuffer
 *
 * This function is called when the buffer has settled after a number of
 * changes provided by the user. It is a convenient way to know when you
 * should perform more background work without having to coalesce work
 * yourself.
 */
void
ide_buffer_addin_change_settled (IdeBufferAddin *self,
                                 IdeBuffer      *buffer)
{
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->change_settled)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->change_settled (self, buffer);
}

/**
 * ide_buffer_addin_style_scheme_changed:
 * @self: an #IdeBufferAddin
 * @buffer: an #IdeBuffer
 *
 * This function is called when the #GtkSourceStyleScheme of the #IdeBuffer
 * has changed.
 */
void
ide_buffer_addin_style_scheme_changed (IdeBufferAddin *self,
                                       IdeBuffer      *buffer)
{
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (IDE_BUFFER_ADDIN_GET_IFACE (self)->style_scheme_changed)
    IDE_BUFFER_ADDIN_GET_IFACE (self)->style_scheme_changed (self, buffer);
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
 */
IdeBufferAddin *
ide_buffer_addin_find_by_module_name (IdeBuffer   *buffer,
                                      const gchar *module_name)
{
  PeasPluginInfo *plugin_info;
  IdeExtensionSetAdapter *set;
  GObject *ret = NULL;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  set = _ide_buffer_get_addins (buffer);

  /* Addins might not be loaded */
  if (set == NULL)
    return NULL;

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), module_name);

  if (plugin_info != NULL)
    ret = ide_extension_set_adapter_get_extension (set, plugin_info);
  else
    g_warning ("Failed to locate addin named %s", module_name);

  return ret ? IDE_BUFFER_ADDIN (ret) : NULL;
}

void
_ide_buffer_addin_load_cb (IdeExtensionSetAdapter *set,
                           PeasPluginInfo         *plugin_info,
                           GObject          *exten,
                           gpointer                user_data)
{
  IdeBuffer *buffer = user_data;

  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (exten));
  g_return_if_fail (IDE_IS_BUFFER (user_data));

  ide_buffer_addin_load (IDE_BUFFER_ADDIN (exten), buffer);

  if (ide_buffer_get_state (buffer) == IDE_BUFFER_STATE_READY &&
      !ide_buffer_get_is_temporary (buffer))
    {
      IdeBufferFileLoad closure = {
        .buffer = buffer,
        .file = ide_buffer_get_file (buffer),
      };

      _ide_buffer_addin_file_loaded_cb (set, plugin_info, exten, &closure);
    }

}

void
_ide_buffer_addin_unload_cb (IdeExtensionSetAdapter *set,
                             PeasPluginInfo         *plugin_info,
                             GObject          *exten,
                             gpointer                user_data)
{
  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (exten));
  g_return_if_fail (IDE_IS_BUFFER (user_data));

  ide_buffer_addin_unload (IDE_BUFFER_ADDIN (exten), IDE_BUFFER (user_data));
}

void
_ide_buffer_addin_file_loaded_cb (IdeExtensionSetAdapter *set,
                                  PeasPluginInfo         *plugin_info,
                                  GObject          *exten,
                                  gpointer                user_data)
{
  IdeBufferFileLoad *load = user_data;

  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (exten));
  g_return_if_fail (load != NULL);
  g_return_if_fail (IDE_IS_BUFFER (load->buffer));
  g_return_if_fail (G_IS_FILE (load->file));

  ide_buffer_addin_file_loaded (IDE_BUFFER_ADDIN (exten), load->buffer, load->file);
}

void
_ide_buffer_addin_save_file_cb (IdeExtensionSetAdapter *set,
                                PeasPluginInfo         *plugin_info,
                                GObject          *exten,
                                gpointer                user_data)
{
  IdeBufferFileSave *save = user_data;

  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (exten));
  g_return_if_fail (save != NULL);
  g_return_if_fail (IDE_IS_BUFFER (save->buffer));
  g_return_if_fail (G_IS_FILE (save->file));

  ide_buffer_addin_save_file (IDE_BUFFER_ADDIN (exten), save->buffer, save->file);
}

void
_ide_buffer_addin_file_saved_cb (IdeExtensionSetAdapter *set,
                                 PeasPluginInfo         *plugin_info,
                                 GObject          *exten,
                                 gpointer                user_data)
{
  IdeBufferFileSave *save = user_data;

  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (exten));
  g_return_if_fail (save != NULL);
  g_return_if_fail (IDE_IS_BUFFER (save->buffer));
  g_return_if_fail (G_IS_FILE (save->file));

  ide_buffer_addin_file_saved (IDE_BUFFER_ADDIN (exten), save->buffer, save->file);
}

void
_ide_buffer_addin_language_set_cb (IdeExtensionSetAdapter *set,
                                   PeasPluginInfo         *plugin_info,
                                   GObject          *exten,
                                   gpointer                user_data)
{
  IdeBufferLanguageSet *lang = user_data;

  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (exten));
  g_return_if_fail (lang != NULL);
  g_return_if_fail (IDE_IS_BUFFER (lang->buffer));

  ide_buffer_addin_language_set (IDE_BUFFER_ADDIN (exten), lang->buffer, lang->language_id);
}

void
_ide_buffer_addin_change_settled_cb (IdeExtensionSetAdapter *set,
                                     PeasPluginInfo         *plugin_info,
                                     GObject          *exten,
                                     gpointer                user_data)
{
  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (exten));
  g_return_if_fail (IDE_IS_BUFFER (user_data));

  ide_buffer_addin_change_settled (IDE_BUFFER_ADDIN (exten), IDE_BUFFER (user_data));
}

void
_ide_buffer_addin_style_scheme_changed_cb (IdeExtensionSetAdapter *set,
                                           PeasPluginInfo         *plugin_info,
                                           GObject          *exten,
                                           gpointer                user_data)
{
  g_return_if_fail (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_return_if_fail (plugin_info != NULL);
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (exten));
  g_return_if_fail (IDE_IS_BUFFER (user_data));

  ide_buffer_addin_style_scheme_changed (IDE_BUFFER_ADDIN (exten), IDE_BUFFER (user_data));
}

void
ide_buffer_addin_settle_async (IdeBufferAddin      *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_ADDIN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUFFER_ADDIN_GET_IFACE (self)->settle_async (self, cancellable, callback, user_data);
}

gboolean
ide_buffer_addin_settle_finish (IdeBufferAddin  *self,
                                GAsyncResult    *result,
                                GError         **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_BUFFER_ADDIN_GET_IFACE (self)->settle_finish (self, result, error);
}
