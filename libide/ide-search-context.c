/* ide-search-context.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ide-search-context.h"

typedef struct
{
  void *foo;
} IdeSearchContextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSearchContext, ide_search_context, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

IdeSearchContext *
ide_search_context_new (void)
{
  return g_object_new (IDE_TYPE_SEARCH_CONTEXT, NULL);
}

static void
ide_search_context_finalize (GObject *object)
{
  IdeSearchContext *self = (IdeSearchContext *)object;
  IdeSearchContextPrivate *priv = ide_search_context_get_instance_private (self);

  G_OBJECT_CLASS (ide_search_context_parent_class)->finalize (object);
}

static void
ide_search_context_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeSearchContext *self = IDE_SEARCH_CONTEXT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_context_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeSearchContext *self = IDE_SEARCH_CONTEXT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_context_class_init (IdeSearchContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_search_context_finalize;
  object_class->get_property = ide_search_context_get_property;
  object_class->set_property = ide_search_context_set_property;
}

static void
ide_search_context_init (IdeSearchContext *self)
{
}
