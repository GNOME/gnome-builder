/* ide-editor-addin.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-addin"

#include "config.h"

#include "editor/ide-editor-addin.h"
#include "editor/ide-editor-private.h"

/**
 * SECTION:ide-editor-addin
 * @title: IdeEditorAddin
 * @short_description: Addins for the editor perspective
 *
 * The #IdeEditorAddin interface provides a simplified interface for
 * plugins that want to perform operations in, or extend, the editor
 * perspective.
 *
 * This differs from the #IdeWorkbenchAddin in that you are given access
 * to the editor perspective directly. This can be convenient if all you
 * need to do is add panels or perform view tracking of the current
 * focus view.
 */

G_DEFINE_INTERFACE (IdeEditorAddin, ide_editor_addin, G_TYPE_OBJECT)

static void
ide_editor_addin_default_init (IdeEditorAddinInterface *iface)
{
}

/**
 * ide_editor_addin_load:
 * @self: an #IdeEditorAddin
 * @perspective: an #IdeEditorPeprsective
 *
 * This method is called to load the addin.
 *
 * The addin should add any necessary UI components.
 *
 * Since: 3.26
 */
void
ide_editor_addin_load (IdeEditorAddin       *self,
                       IdeEditorPerspective *perspective)
{
  g_return_if_fail (IDE_IS_EDITOR_ADDIN (self));
  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (perspective));

  if (IDE_EDITOR_ADDIN_GET_IFACE (self)->load)
    IDE_EDITOR_ADDIN_GET_IFACE (self)->load (self, perspective);
}

/**
 * ide_editor_addin_unload:
 * @self: an #IdeEditorAddin
 * @perspective: an #IdeEditorPerspective
 *
 * This method is called to unload the addin.
 *
 * The addin is responsible for undoing anything it setup in load
 * and cancel any in-flight or pending tasks immediately.
 *
 * Since: 3.26
 */
void
ide_editor_addin_unload (IdeEditorAddin       *self,
                         IdeEditorPerspective *perspective)
{
  g_return_if_fail (IDE_IS_EDITOR_ADDIN (self));
  g_return_if_fail (IDE_IS_EDITOR_PERSPECTIVE (perspective));

  if (IDE_EDITOR_ADDIN_GET_IFACE (self)->unload)
    IDE_EDITOR_ADDIN_GET_IFACE (self)->unload (self, perspective);
}

/**
 * ide_editor_addin_view_set:
 * @self: an #IdeEditorAddin
 * @view: (nullable): an #IdeLayoutView or %NULL
 *
 * This function is called when the current view has changed in the
 * editor perspective. This could happen when the user focus another
 * view, either with the keyboard, mouse, touch, or by opening a new
 * buffer.
 *
 * Note that @view may not be an #IdeEditorView, so consumers of this
 * interface should take appropriate action based on the type.
 *
 * When the last view is removed, @view will be %NULL to indicate to the
 * addin that there is no active view.
 *
 * Since: 3.26
 */
void
ide_editor_addin_view_set (IdeEditorAddin *self,
                           IdeLayoutView  *view)
{
  g_return_if_fail (IDE_IS_EDITOR_ADDIN (self));
  g_return_if_fail (!view || IDE_IS_LAYOUT_VIEW (view));

  if (IDE_EDITOR_ADDIN_GET_IFACE (self)->view_set)
    IDE_EDITOR_ADDIN_GET_IFACE (self)->view_set (self, view);
}

/**
 * ide_editor_addin_find_by_module_name:
 * @editor: an #IdeEditorPerspective
 * @module_name: the module name of the addin
 *
 * This function allows locating an #IdeEditorAddin that is attached
 * to the #IdeEditorPerspective by the addin module name. The module name
 * should match the value specified in the ".plugin" module definition.
 *
 * Returns: (transfer none) (nullable): An #IdeEditorAddin or %NULL
 */
IdeEditorAddin *
ide_editor_addin_find_by_module_name (IdeEditorPerspective *editor,
                                      const gchar          *module_name)
{
  PeasExtension *ret = NULL;
  PeasPluginInfo *plugin_info;

  g_return_val_if_fail (IDE_IS_EDITOR_PERSPECTIVE (editor), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), module_name);

  if (plugin_info != NULL)
    ret = peas_extension_set_get_extension (editor->addins, plugin_info);
  else
    g_warning ("No such module found \"%s\"", module_name);

  return ret ? IDE_EDITOR_ADDIN (ret) : NULL;
}
