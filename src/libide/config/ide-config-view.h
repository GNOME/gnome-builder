/* ide-config-view.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>

#include "ide-version-macros.h"

#include "config/ide-configuration.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIG_VIEW (ide_config_view_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeConfigView, ide_config_view, IDE, CONFIG_VIEW, GtkBin)

IDE_AVAILABLE_IN_3_32
GtkWidget        *ide_config_view_new        (void);
IDE_AVAILABLE_IN_3_32
void              ide_config_view_set_config (IdeConfigView    *self,
                                              IdeConfiguration *configuration);
IDE_AVAILABLE_IN_3_32
IdeConfiguration *ide_config_view_get_config (IdeConfigView    *self);

G_END_DECLS
