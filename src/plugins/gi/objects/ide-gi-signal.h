/* ide-gi-signal.h
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib.h>
#include <glib-object.h>

#include "./../ide-gi-blob.h"
#include "./../ide-gi-types.h"
#include "./../ide-gi-namespace.h"
#include "./../ide-gi-namespace-private.h"

#include "ide-gi-base.h"
#include "ide-gi-callback.h"
#include "ide-gi-parameter.h"

G_BEGIN_DECLS

struct _IdeGiSignal
{
  IDE_GI_BASE_FIELDS
};

void             ide_gi_signal_free                 (IdeGiBase      *base);
void             ide_gi_signal_dump                 (IdeGiSignal    *self,
                                                     guint           depth);
IdeGiBase       *ide_gi_signal_new                  (IdeGiNamespace *ns,
                                                     IdeGiBlobType   type,
                                                     gint32          offset);
IdeGiSignal     *ide_gi_signal_ref                  (IdeGiSignal    *self);
void             ide_gi_signal_unref                (IdeGiSignal    *self);

IdeGiSignalWhen  ide_gi_signal_get_run_when         (IdeGiSignal    *self);
gboolean         ide_gi_signal_is_no_recurse        (IdeGiSignal    *self);
gboolean         ide_gi_signal_is_detailed          (IdeGiSignal    *self);
gboolean         ide_gi_signal_is_action            (IdeGiSignal    *self);
gboolean         ide_gi_signal_is_no_hooks          (IdeGiSignal    *self);
gboolean         ide_gi_signal_has_class_closure    (IdeGiSignal    *self);
gboolean         ide_gi_signal_is_true_stops_emit   (IdeGiSignal    *self);

/* TODO: string or ref ? */
const gchar     *ide_gi_signal_get_vfunc            (IdeGiSignal    *self);

guint16           ide_gi_signal_get_n_parameters    (IdeGiSignal    *self);
IdeGiParameter   *ide_gi_signal_get_parameter       (IdeGiSignal    *self,
                                                     guint16         nth);
IdeGiParameter   *ide_gi_signal_get_return_value    (IdeGiSignal    *self);
IdeGiParameter   *ide_gi_signal_lookup_parameter    (IdeGiSignal    *self,
                                                     const gchar    *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiSignal, ide_gi_signal_unref)

G_END_DECLS
