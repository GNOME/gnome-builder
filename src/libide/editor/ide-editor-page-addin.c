/* ide-editor-page-addin.c
 *
 * Copyright 2015-2022 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-editor-page-addin"

#include "config.h"

#include <libide-plugins.h>

#include "ide-editor-page-addin.h"
#include "ide-editor-page-private.h"

G_DEFINE_INTERFACE (IdeEditorPageAddin, ide_editor_page_addin, G_TYPE_OBJECT)

static GActionGroup *
ide_editor_page_addin_real_ref_action_group (IdeEditorPageAddin *addin)
{
  if (G_IS_ACTION_GROUP (addin))
    return g_object_ref (G_ACTION_GROUP (addin));

  return NULL;
}

static void
ide_editor_page_addin_default_init (IdeEditorPageAddinInterface *iface)
{
  iface->ref_action_group = ide_editor_page_addin_real_ref_action_group;
}

void
ide_editor_page_addin_load (IdeEditorPageAddin *self,
                            IdeEditorPage      *page)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE_ADDIN (self));
  g_return_if_fail (IDE_IS_EDITOR_PAGE (page));

  if (IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->load)
    IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->load (self, page);
}

void
ide_editor_page_addin_unload (IdeEditorPageAddin *self,
                              IdeEditorPage      *page)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE_ADDIN (self));
  g_return_if_fail (IDE_IS_EDITOR_PAGE (page));

  if (IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->unload)
    IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->unload (self, page);
}

void
ide_editor_page_addin_language_changed (IdeEditorPageAddin *self,
                                        const gchar        *language_id)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE_ADDIN (self));

  if (IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->language_changed)
    IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->language_changed (self, language_id);
}

void
ide_editor_page_addin_frame_set (IdeEditorPageAddin *self,
                                 IdeFrame           *frame)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE_ADDIN (self));
  g_return_if_fail (IDE_IS_FRAME (frame));

  if (IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->frame_set)
    IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->frame_set (self, frame);
}

/**
 * ide_editor_page_addin_ref_action_group:
 * @self: a #IdeEditorPageAddin
 *
 * Returns: (transfer full) (nullable): a #GActionGroup or %NULL
 */
GActionGroup *
ide_editor_page_addin_ref_action_group (IdeEditorPageAddin *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE_ADDIN (self), NULL);

  if (IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->ref_action_group)
    return IDE_EDITOR_PAGE_ADDIN_GET_IFACE (self)->ref_action_group (self);

  return NULL;
}

/**
 * ide_editor_page_addin_find_by_module_name:
 * @page: an #IdeEditorPage
 * @module_name: the module name which provides the addin
 *
 * This function will locate the #IdeEditorPageAddin that was registered
 * by the addin named @module_name (which should match the module_name
 * provided in the .plugin file).
 *
 * If no module was found or that module does not implement the
 * #IdeEditorPageAddinInterface, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeEditorPageAddin or %NULL
 */
IdeEditorPageAddin *
ide_editor_page_addin_find_by_module_name (IdeEditorPage *page,
                                           const gchar   *module_name)
{
  GObject *ret = NULL;
  PeasPluginInfo *plugin_info;

  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (page), NULL);
  g_return_val_if_fail (page->addins != NULL, NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), module_name);

  if (plugin_info != NULL)
    ret = ide_extension_set_adapter_get_extension (page->addins, plugin_info);
  else
    g_warning ("No addin could be found matching module \"%s\"", module_name);

  return ret ? IDE_EDITOR_PAGE_ADDIN (ret) : NULL;
}
