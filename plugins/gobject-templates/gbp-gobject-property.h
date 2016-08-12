/* gbp-gobject-property.h
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

#ifndef GBP_GOBJECT_PROPERTY_H
#define GBP_GOBJECT_PROPERTY_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  GBP_GOBJECT_PROPERTY_BOOLEAN,
  GBP_GOBJECT_PROPERTY_BOXED,
  GBP_GOBJECT_PROPERTY_CHAR,
  GBP_GOBJECT_PROPERTY_DOUBLE,
  GBP_GOBJECT_PROPERTY_ENUM,
  GBP_GOBJECT_PROPERTY_FLAGS,
  GBP_GOBJECT_PROPERTY_FLOAT,
  GBP_GOBJECT_PROPERTY_INT,
  GBP_GOBJECT_PROPERTY_INT64,
  GBP_GOBJECT_PROPERTY_LONG,
  GBP_GOBJECT_PROPERTY_OBJECT,
  GBP_GOBJECT_PROPERTY_POINTER,
  GBP_GOBJECT_PROPERTY_STRING,
  GBP_GOBJECT_PROPERTY_UINT,
  GBP_GOBJECT_PROPERTY_UINT64,
  GBP_GOBJECT_PROPERTY_ULONG,
  GBP_GOBJECT_PROPERTY_UNICHAR,
  GBP_GOBJECT_PROPERTY_VARIANT,
} GbpGobjectPropertyKind;

#define GBP_TYPE_GOBJECT_PROPERTY (gbp_gobject_property_get_type())
#define GBP_TYPE_GOBJECT_PROPERTY_KIND (gbp_gobject_property_kind_get_type())

G_DECLARE_FINAL_TYPE (GbpGobjectProperty, gbp_gobject_property, GBP, GOBJECT_PROPERTY, GObject)

GType                   gbp_gobject_property_kind_get_type (void);
GbpGobjectProperty     *gbp_gobject_property_new           (void);
GbpGobjectPropertyKind  gbp_gobject_property_get_kind      (GbpGobjectProperty     *self);
void                    gbp_gobject_property_set_kind      (GbpGobjectProperty     *self,
                                                            GbpGobjectPropertyKind  kind);
const gchar            *gbp_gobject_property_get_name      (GbpGobjectProperty     *self);
void                    gbp_gobject_property_set_name      (GbpGobjectProperty     *self,
                                                            const gchar            *name);
const gchar            *gbp_gobject_property_get_default   (GbpGobjectProperty     *self);
void                    gbp_gobject_property_set_default   (GbpGobjectProperty     *self,
                                                            const gchar            *default_);
const gchar            *gbp_gobject_property_get_minimum   (GbpGobjectProperty     *self);
void                    gbp_gobject_property_set_minimum   (GbpGobjectProperty     *self,
                                                            const gchar            *minimum);
const gchar            *gbp_gobject_property_get_maximum   (GbpGobjectProperty     *self);
void                    gbp_gobject_property_set_maximum   (GbpGobjectProperty     *self,
                                                            const gchar            *maximum);
gboolean                gbp_gobject_property_get_readable  (GbpGobjectProperty     *self);
void                    gbp_gobject_property_set_readable  (GbpGobjectProperty     *self,
                                                            gboolean                readable);
gboolean                gbp_gobject_property_get_writable  (GbpGobjectProperty     *self);
void                    gbp_gobject_property_set_writable  (GbpGobjectProperty     *self,
                                                            gboolean                writable);

G_END_DECLS

#endif /* GBP_GOBJECT_PROPERTY_H */
