/* ide-source-view.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-source-view"

#include "config.h"

#include "ide-source-view.h"

struct _IdeSourceView
{
  GtkSourceView source_view;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE (IdeSourceView, ide_source_view, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties [N_PROPS];

static void
ide_source_view_dispose (GObject *object)
{
  G_OBJECT_CLASS (ide_source_view_parent_class)->dispose (object);
}

static void
ide_source_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_source_view_parent_class)->finalize (object);
}

static void
ide_source_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_class_init (IdeSourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_source_view_dispose;
  object_class->finalize = ide_source_view_finalize;
  object_class->get_property = ide_source_view_get_property;
  object_class->set_property = ide_source_view_set_property;
}

static void
ide_source_view_init (IdeSourceView *self)
{
}

void
ide_source_view_scroll_to_insert (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextView *view;
  GtkTextMark *mark;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  view = GTK_TEXT_VIEW (self);
  buffer = gtk_text_view_get_buffer (view);
  mark = gtk_text_buffer_get_insert (buffer);

  /* TODO: use margin to implement  "scroll offset" */
  gtk_text_view_scroll_to_mark (view, mark, .5, FALSE, .0, .0);
}
