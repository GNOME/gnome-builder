/* dspy-method-view.h
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

#include <dazzle.h>

#include "dspy-method-invocation.h"

G_BEGIN_DECLS

#define DSPY_TYPE_METHOD_VIEW (dspy_method_view_get_type())

G_DECLARE_DERIVABLE_TYPE (DspyMethodView, dspy_method_view, DSPY, METHOD_VIEW, DzlBin)

struct _DspyMethodViewClass
{
  DzlBinClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

GtkWidget            *dspy_method_view_new            (void);
DspyMethodInvocation *dspy_method_view_get_invocation (DspyMethodView       *self);
void                  dspy_method_view_set_invocation (DspyMethodView       *self,
                                                       DspyMethodInvocation *invocation);

G_END_DECLS
