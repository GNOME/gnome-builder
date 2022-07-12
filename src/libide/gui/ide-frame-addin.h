/* ide-frame-addin.h
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

#pragma once

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include "ide-frame.h"
#include "ide-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_FRAME_ADDIN (ide_frame_addin_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeFrameAddin, ide_frame_addin, IDE, FRAME_ADDIN, GObject)

struct _IdeFrameAddinInterface
{
  GTypeInterface parent_iface;

  void (*load)     (IdeFrameAddin *self,
                    IdeFrame      *frame);
  void (*unload)   (IdeFrameAddin *self,
                    IdeFrame      *frame);
  void (*set_page) (IdeFrameAddin *self,
                    IdePage       *page);
};

IDE_AVAILABLE_IN_ALL
void           ide_frame_addin_load                (IdeFrameAddin *self,
                                                    IdeFrame      *frame);
IDE_AVAILABLE_IN_ALL
void           ide_frame_addin_unload              (IdeFrameAddin *self,
                                                    IdeFrame      *frame);
IDE_AVAILABLE_IN_ALL
void           ide_frame_addin_set_page            (IdeFrameAddin *self,
                                                    IdePage       *page);
IDE_AVAILABLE_IN_ALL
IdeFrameAddin *ide_frame_addin_find_by_module_name (IdeFrame      *frame,
                                                    const gchar   *module_name);

G_END_DECLS
