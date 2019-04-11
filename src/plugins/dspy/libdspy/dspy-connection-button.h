/* dspy-connection-button.h
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

#include "dspy-connection.h"

G_BEGIN_DECLS

#define DSPY_TYPE_CONNECTION_BUTTON (dspy_connection_button_get_type())

G_DECLARE_DERIVABLE_TYPE (DspyConnectionButton, dspy_connection_button, DSPY, CONNECTION_BUTTON, GtkRadioButton)

struct _DspyConnectionButtonClass
{
  GtkRadioButtonClass parent_class;
  
  /*< private >*/
  gpointer _reserved[8];
};

GtkWidget      *dspy_connection_button_new            (void);
DspyConnection *dspy_connection_button_get_connection (DspyConnectionButton *self);
void            dspy_connection_button_set_connection (DspyConnectionButton *self,
                                                       DspyConnection       *connection);

G_END_DECLS
