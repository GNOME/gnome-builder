/* ide-scrubber-revealer.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SCRUBBER_REVEALER (ide_scrubber_revealer_get_type())

typedef enum
{
  IDE_SCRUBBER_REVEAL_POLICY_NEVER,
  IDE_SCRUBBER_REVEAL_POLICY_AUTO,
  IDE_SCRUBBER_REVEAL_POLICY_ALWAYS,
} IdeScrubberRevealPolicy;

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeScrubberRevealer, ide_scrubber_revealer, IDE, SCRUBBER_REVEALER, GtkWidget)

IDE_AVAILABLE_IN_ALL
GtkWidget               *ide_scrubber_revealer_new          (void);
IDE_AVAILABLE_IN_ALL
GtkWidget               *ide_scrubber_revealer_get_content  (IdeScrubberRevealer     *self);
IDE_AVAILABLE_IN_ALL
void                     ide_scrubber_revealer_set_content  (IdeScrubberRevealer     *self,
                                                             GtkWidget               *content);
IDE_AVAILABLE_IN_ALL
GtkWidget               *ide_scrubber_revealer_get_scrubber (IdeScrubberRevealer     *self);
IDE_AVAILABLE_IN_ALL
void                     ide_scrubber_revealer_set_scrubber (IdeScrubberRevealer     *self,
                                                             GtkWidget               *scrubber);
IDE_AVAILABLE_IN_ALL
IdeScrubberRevealPolicy  ide_scrubber_revealer_get_policy   (IdeScrubberRevealer     *self);
IDE_AVAILABLE_IN_ALL
void                     ide_scrubber_revealer_set_policy   (IdeScrubberRevealer     *self,
                                                             IdeScrubberRevealPolicy  policy);

G_END_DECLS
