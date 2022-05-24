/* ide-frame-addin.c
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

#define G_LOG_DOMAIN "ide-frame-addin"

#include "config.h"

#include "ide-frame-addin.h"

/**
 * SECTION:ide-frame-addin
 * @title: IdeFrameAddin
 * @short_description: addins created for every #IdeFrame
 */

G_DEFINE_INTERFACE (IdeFrameAddin, ide_frame_addin, G_TYPE_OBJECT)

static void
ide_frame_addin_default_init (IdeFrameAddinInterface *iface)
{
}

/**
 * ide_frame_addin_load:
 * @self: An #IdeFrameAddin
 * @frame: An #IdeFrame
 *
 * This function should be implemented by #IdeFrameAddin plugins
 * in #IdeFrameAddinInterface.
 *
 * This virtual method is called when the plugin should load itself.
 * A new instance of the plugin is created for every #IdeFrame
 * that is created in Builder.
 */
void
ide_frame_addin_load (IdeFrameAddin *self,
                      IdeFrame      *frame)
{
  g_return_if_fail (IDE_IS_FRAME_ADDIN (self));
  g_return_if_fail (IDE_IS_FRAME (frame));

  if (IDE_FRAME_ADDIN_GET_IFACE (self)->load)
    IDE_FRAME_ADDIN_GET_IFACE (self)->load (self, frame);
}

/**
 * ide_frame_addin_unload:
 * @self: An #IdeFrameAddin
 * @frame: An #IdeFrame
 *
 * This function should be implemented by #IdeFrameAddin plugins
 * in #IdeFrameAddinInterface.
 *
 * This virtual method is called when the plugin should unload itself.
 * It should revert anything performed via ide_frame_addin_load().
 */
void
ide_frame_addin_unload (IdeFrameAddin *self,
                        IdeFrame      *frame)
{
  g_return_if_fail (IDE_IS_FRAME_ADDIN (self));
  g_return_if_fail (IDE_IS_FRAME (frame));

  if (IDE_FRAME_ADDIN_GET_IFACE (self)->unload)
    IDE_FRAME_ADDIN_GET_IFACE (self)->unload (self, frame);
}

/**
 * ide_frame_addin_set_page:
 * @self: an #IdeFrameAddin
 * @page: (nullable): An #IdePage or %NULL.
 *
 * This virtual method is called whenever the active page changes
 * in the #IdePage. Plugins may want to alter what controls
 * are displayed on the frame based on the current page.
 */
void
ide_frame_addin_set_page (IdeFrameAddin *self,
                          IdePage       *page)
{
  g_return_if_fail (IDE_IS_FRAME_ADDIN (self));
  g_return_if_fail (!page || IDE_IS_PAGE (page));

  if (IDE_FRAME_ADDIN_GET_IFACE (self)->set_page)
    IDE_FRAME_ADDIN_GET_IFACE (self)->set_page (self, page);
}
