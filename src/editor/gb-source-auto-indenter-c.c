/* gb-source-auto-indenter-c.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "gb-source-auto-indenter-c.h"

struct _GbSourceAutoIndenterCPrivate
{
  gpointer foo;
};

enum
{
  PROP_0,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceAutoIndenterC, gb_source_auto_indenter_c,
                            GB_TYPE_SOURCE_AUTO_INDENTER)

static GParamSpec *gParamSpecs [LAST_PROP];

GbSourceAutoIndenter *
gb_source_auto_indenter_c_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_AUTO_INDENTER_C, NULL);
}

static gchar *
gb_source_auto_indenter_c_query (GbSourceAutoIndenter *indenter,
                                 GtkTextView          *view,
                                 GtkTextBuffer        *buffer,
                                 GtkTextIter          *iter)
{
  GbSourceAutoIndenterC *c = (GbSourceAutoIndenterC *)indenter;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER_C (c), NULL);

  g_printerr ("QUERY:\n");

  return NULL;
}

static void
gb_source_auto_indenter_c_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbSourceAutoIndenterC *c = GB_SOURCE_AUTO_INDENTER_C (object);

  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gb_source_auto_indenter_c_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbSourceAutoIndenterC *c = GB_SOURCE_AUTO_INDENTER_C (object);

  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gb_source_auto_indenter_c_class_init (GbSourceAutoIndenterCClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbSourceAutoIndenterClass *indenter_class = GB_SOURCE_AUTO_INDENTER_CLASS (klass);

  object_class->get_property = gb_source_auto_indenter_c_get_property;
  object_class->set_property = gb_source_auto_indenter_c_set_property;

  indenter_class->query = gb_source_auto_indenter_c_query;
}

static void
gb_source_auto_indenter_c_init (GbSourceAutoIndenterC *c)
{
  c->priv = gb_source_auto_indenter_c_get_instance_private (c);
}
