/* gbp-gobject-spec.h
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

#ifndef GBP_GOBJECT_SPEC_H
#define GBP_GOBJECT_SPEC_H

#include <gio/gio.h>

#include "gbp-gobject-property.h"
#include "gbp-gobject-signal.h"

G_BEGIN_DECLS

#define GBP_TYPE_GOBJECT_SPEC (gbp_gobject_spec_get_type())

G_DECLARE_FINAL_TYPE (GbpGobjectSpec, gbp_gobject_spec, GBP, GOBJECT_SPEC, GObject)

GbpGobjectSpec *gbp_gobject_spec_new             (void);
GListModel     *gbp_gobject_spec_get_properties  (GbpGobjectSpec     *self);
GListModel     *gbp_gobject_spec_get_signals     (GbpGobjectSpec     *self);
void            gbp_gobject_spec_add_property    (GbpGobjectSpec     *self,
                                                  GbpGobjectProperty *property);
void            gbp_gobject_spec_remove_property (GbpGobjectSpec     *self,
                                                  GbpGobjectProperty *property);
void            gbp_gobject_spec_add_signal      (GbpGobjectSpec     *self,
                                                  GbpGobjectSignal   *signal);
void            gbp_gobject_spec_remove_signal   (GbpGobjectSpec     *self,
                                                  GbpGobjectSignal   *signal);
gboolean        gbp_gobject_spec_get_ready       (GbpGobjectSpec     *self);
const gchar    *gbp_gobject_spec_get_name        (GbpGobjectSpec     *self);
const gchar    *gbp_gobject_spec_get_namespace   (GbpGobjectSpec     *self);
const gchar    *gbp_gobject_spec_get_class_name  (GbpGobjectSpec     *self);
const gchar    *gbp_gobject_spec_get_parent_name (GbpGobjectSpec     *self);

G_END_DECLS

#endif /* GBP_GOBJECT_SPEC_H */
