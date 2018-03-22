/* ide-editor-view-addin.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-editor-view-addin"

#include "config.h"

#include "editor/ide-editor-private.h"
#include "editor/ide-editor-view-addin.h"

G_DEFINE_INTERFACE (IdeEditorViewAddin, ide_editor_view_addin, G_TYPE_OBJECT)

static void
ide_editor_view_addin_default_init (IdeEditorViewAddinInterface *iface)
{
}

void
ide_editor_view_addin_load (IdeEditorViewAddin *self,
                            IdeEditorView      *view)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW_ADDIN (self));
  g_return_if_fail (IDE_IS_EDITOR_VIEW (view));

  if (IDE_EDITOR_VIEW_ADDIN_GET_IFACE (self)->load)
    IDE_EDITOR_VIEW_ADDIN_GET_IFACE (self)->load (self, view);
}

void
ide_editor_view_addin_unload (IdeEditorViewAddin *self,
                              IdeEditorView      *view)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW_ADDIN (self));
  g_return_if_fail (IDE_IS_EDITOR_VIEW (view));

  if (IDE_EDITOR_VIEW_ADDIN_GET_IFACE (self)->unload)
    IDE_EDITOR_VIEW_ADDIN_GET_IFACE (self)->unload (self, view);
}

void
ide_editor_view_addin_language_changed (IdeEditorViewAddin *self,
                                        const gchar        *language_id)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW_ADDIN (self));

  if (IDE_EDITOR_VIEW_ADDIN_GET_IFACE (self)->language_changed)
    IDE_EDITOR_VIEW_ADDIN_GET_IFACE (self)->language_changed (self, language_id);
}

void
ide_editor_view_addin_stack_set (IdeEditorViewAddin *self,
                                 IdeLayoutStack     *stack)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW_ADDIN (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (stack));

  if (IDE_EDITOR_VIEW_ADDIN_GET_IFACE (self)->stack_set)
    IDE_EDITOR_VIEW_ADDIN_GET_IFACE (self)->stack_set (self, stack);
}

/**
 * ide_editor_view_addin_find_by_module_name:
 * @view: an #IdeEditorView
 * @module_name: the module name which provides the addin
 *
 * This function will locate the #IdeEditorViewAddin that was registered
 * by the addin named @module_name (which should match the module_name
 * provided in the .plugin file).
 *
 * If no module was found or that module does not implement the
 * #IdeEditorViewAddinInterface, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeEditorViewAddin or %NULL
 *
 * Since: 3.26
 */
IdeEditorViewAddin *
ide_editor_view_addin_find_by_module_name (IdeEditorView *view,
                                           const gchar   *module_name)
{
  PeasExtension *ret = NULL;
  PeasPluginInfo *plugin_info;

  g_return_val_if_fail (IDE_IS_EDITOR_VIEW (view), NULL);
  g_return_val_if_fail (view->addins != NULL, NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), module_name);

  if (plugin_info != NULL)
    ret = ide_extension_set_adapter_get_extension (view->addins, plugin_info);
  else
    g_warning ("No addin could be found matching module \"%s\"", module_name);

  return ret ? IDE_EDITOR_VIEW_ADDIN (ret) : NULL;
}
