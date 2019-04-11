/* dspy-name-marquee.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

#include "dspy-name.h"

G_BEGIN_DECLS

#define DSPY_TYPE_NAME_MARQUEE (dspy_name_marquee_get_type())

G_DECLARE_FINAL_TYPE (DspyNameMarquee, dspy_name_marquee, DSPY, NAME_MARQUEE, GtkBin)

GtkWidget *dspy_name_marquee_new      (void);
DspyName  *dspy_name_marquee_get_name (DspyNameMarquee *self);
void       dspy_name_marquee_set_name (DspyNameMarquee *self,
                                       DspyName        *name);

G_END_DECLS
