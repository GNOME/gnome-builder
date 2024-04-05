/* gbp-manuals-page.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <libide-webkit.h>

#include "manuals-navigatable.h"

G_BEGIN_DECLS

#define GBP_TYPE_MANUALS_PAGE (gbp_manuals_page_get_type())

G_DECLARE_FINAL_TYPE (GbpManualsPage, gbp_manuals_page, GBP, MANUALS_PAGE, IdeWebkitPage)

GbpManualsPage *gbp_manuals_page_new         (void);
void            gbp_manuals_page_navigate_to (GbpManualsPage     *self,
                                              ManualsNavigatable *navigatable);

G_END_DECLS
