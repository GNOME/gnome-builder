/* ide-layout-stack-addin.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-layout-stack-addin"

#include "layout/ide-layout-stack-addin.h"

G_DEFINE_INTERFACE (IdeLayoutStackAddin, ide_layout_stack_addin, G_TYPE_OBJECT)

static void
ide_layout_stack_addin_real_load (IdeLayoutStackAddin *self,
                                  IdeLayoutStack      *stack)
{
  g_assert (IDE_IS_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

}

static void
ide_layout_stack_addin_real_unload (IdeLayoutStackAddin *self,
                                    IdeLayoutStack      *stack)
{
  g_assert (IDE_IS_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

}

static void
ide_layout_stack_addin_real_set_view (IdeLayoutStackAddin *self,
                                      IdeLayoutView       *view)
{
  g_assert (IDE_IS_LAYOUT_STACK_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

}

static void
ide_layout_stack_addin_default_init (IdeLayoutStackAddinInterface *iface)
{
  iface->load = ide_layout_stack_addin_real_load;
  iface->unload = ide_layout_stack_addin_real_unload;
  iface->set_view = ide_layout_stack_addin_real_set_view;
}

/**
 * ide_layout_stack_addin_load:
 * @self: An #IdeLayoutStackAddin
 * @stack: An #IdeLayoutStack
 *
 * This function should be implemented by #IdeLayoutStackAddin plugins
 * in #IdeLayoutStackAddinInterface.
 *
 * This virtual method is called when the plugin should load itself.
 * A new instance of the plugin is created for every #IdeLayoutStack
 * that is created in Builder.
 */
void
ide_layout_stack_addin_load (IdeLayoutStackAddin *self,
                             IdeLayoutStack      *stack)
{
  g_return_if_fail (IDE_IS_LAYOUT_STACK_ADDIN (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (stack));

  IDE_LAYOUT_STACK_ADDIN_GET_IFACE (self)->load (self, stack);
}

/**
 * ide_layout_stack_addin_unload:
 * @self: An #IdeLayoutStackAddin
 * @stack: An #IdeLayoutStack
 *
 * This function should be implemented by #IdeLayoutStackAddin plugins
 * in #IdeLayoutStackAddinInterface.
 *
 * This virtual method is called when the plugin should unload itself.
 * It should revert anything performed via ide_layout_stack_addin_load().
 */
void
ide_layout_stack_addin_unload (IdeLayoutStackAddin *self,
                               IdeLayoutStack      *stack)
{
  g_return_if_fail (IDE_IS_LAYOUT_STACK_ADDIN (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (stack));

  IDE_LAYOUT_STACK_ADDIN_GET_IFACE (self)->unload (self, stack);
}

/**
 * ide_layout_stack_addin_set_view:
 * @self: A #IdeLayoutStackAddin
 * @view: (nullable): An #IdeLayoutView or %NULL.
 *
 * This virtual method is called whenever the active view changes
 * in the #IdeLayoutView. Plugins may want to alter what controls
 * are displayed on the stack based on the current view.
 */
void
ide_layout_stack_addin_set_view (IdeLayoutStackAddin *self,
                                 IdeLayoutView       *view)
{
  g_return_if_fail (IDE_IS_LAYOUT_STACK_ADDIN (self));
  g_return_if_fail (!view || IDE_IS_LAYOUT_VIEW (view));

  IDE_LAYOUT_STACK_ADDIN_GET_IFACE (self)->set_view (self, view);
}
