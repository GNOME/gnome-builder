/* gbp-vcsui-clone-widget.h
 *
 * Copyright 2018-2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-gui.h>

G_BEGIN_DECLS

#define GBP_TYPE_VCSUI_CLONE_WIDGET (gbp_vcsui_clone_widget_get_type())

G_DECLARE_FINAL_TYPE (GbpVcsuiCloneWidget, gbp_vcsui_clone_widget, GBP, VCSUI_CLONE_WIDGET, GtkWidget)

GbpVcsuiCloneWidget *gbp_vcsui_clone_widget_new     (void);
const gchar         *gbp_vcsui_clone_widget_get_uri (GbpVcsuiCloneWidget *self);
void                 gbp_vcsui_clone_widget_set_uri (GbpVcsuiCloneWidget *self,
                                                     const gchar         *uri);
void                 gbp_vcsui_clone_widget_clone   (GbpVcsuiCloneWidget *self);

G_END_DECLS