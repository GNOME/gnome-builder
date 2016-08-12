/* gbp-gobject-signal-editor.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef GBP_GOBJECT_SIGNAL_EDITOR_H
#define GBP_GOBJECT_SIGNAL_EDITOR_H

#include <gtk/gtk.h>

#include "gbp-gobject-signal.h"

G_BEGIN_DECLS

#define GBP_TYPE_GOBJECT_SIGNAL_EDITOR (gbp_gobject_signal_editor_get_type())

G_DECLARE_FINAL_TYPE (GbpGobjectSignalEditor, gbp_gobject_signal_editor, GBP, GOBJECT_SIGNAL_EDITOR, GtkBin)

GtkWidget        *gbp_gobject_signal_editor_new        (void);
GbpGobjectSignal *gbp_gobject_signal_editor_get_signal (GbpGobjectSignalEditor *self);
void              gbp_gobject_signal_editor_set_signal (GbpGobjectSignalEditor *self,
                                                        GbpGobjectSignal       *signal);

G_END_DECLS

#endif /* GBP_GOBJECT_SIGNAL_EDITOR_H */
