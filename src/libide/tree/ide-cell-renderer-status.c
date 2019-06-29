/* ide-cell-renderer-status.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-cell-renderer-status"

#include "config.h"

#include <math.h>

#include "ide-cell-renderer-status.h"

#define CELL_HEIGHT 16
#define CELL_WIDTH  16
#define RPAD        8
#define LPAD        3

struct _IdeCellRendererStatus
{
  GtkCellRenderer  parent_instance;
  IdeTreeNodeFlags flags;
};

enum {
  PROP_0,
  PROP_FLAGS,
  N_PROPS
};

G_DEFINE_TYPE (IdeCellRendererStatus, ide_cell_renderer_status, GTK_TYPE_CELL_RENDERER)

static GParamSpec *properties [N_PROPS];

static void
ide_cell_renderer_status_get_preferred_height (GtkCellRenderer *cell,
                                               GtkWidget       *widget,
                                               gint            *min_size,
                                               gint            *nat_size)
{
  g_assert (IDE_IS_CELL_RENDERER_STATUS (cell));
  g_assert (GTK_IS_WIDGET (widget));

  if (min_size)
    *min_size = CELL_HEIGHT;

  if (nat_size)
    *nat_size = CELL_HEIGHT;
}

static void
ide_cell_renderer_status_get_preferred_width (GtkCellRenderer *cell,
                                              GtkWidget       *widget,
                                              gint            *min_size,
                                              gint            *nat_size)
{
  g_assert (IDE_IS_CELL_RENDERER_STATUS (cell));
  g_assert (GTK_IS_WIDGET (widget));

  if (min_size)
    *min_size = LPAD + CELL_WIDTH + RPAD;

  if (nat_size)
    *nat_size = LPAD + CELL_WIDTH + RPAD;
}

static void
ide_cell_renderer_status_render (GtkCellRenderer      *cell,
                                 cairo_t              *cr,
                                 GtkWidget            *widget,
                                 const GdkRectangle   *bg_area,
                                 const GdkRectangle   *cell_area,
                                 GtkCellRendererState  state)
{
  IdeCellRendererStatus *self = (IdeCellRendererStatus *)cell;
  GtkStyleContext *style_context;
  GdkRGBA color;

  g_assert (IDE_IS_CELL_RENDERER_STATUS (self));
  g_assert (cr != NULL);
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (bg_area != NULL);
  g_assert (cell_area != NULL);

  if (self->flags == 0)
    return;

  style_context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (style_context);

  if (state & GTK_CELL_RENDERER_SELECTED)
    gtk_style_context_set_state (style_context,
                                 gtk_style_context_get_state (style_context) & GTK_STATE_FLAG_SELECTED);
  gtk_style_context_get_color (style_context,
                               gtk_style_context_get_state (style_context),
                               &color);
  gdk_cairo_set_source_rgba (cr, &color);

  cairo_arc (cr,
             cell_area->x + cell_area->width - RPAD - (CELL_WIDTH/2),
             cell_area->y + (cell_area->height / 2),
             3,
             0,
             M_PI * 2);

  if (self->flags & IDE_TREE_NODE_FLAGS_ADDED)
    cairo_fill_preserve (cr);

  cairo_stroke (cr);

  gtk_style_context_restore (style_context);
}

static void
ide_cell_renderer_status_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeCellRendererStatus *self = IDE_CELL_RENDERER_STATUS (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      g_value_set_uint (value, self->flags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_cell_renderer_status_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeCellRendererStatus *self = IDE_CELL_RENDERER_STATUS (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      self->flags = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_cell_renderer_status_class_init (IdeCellRendererStatusClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *renderer_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->get_property = ide_cell_renderer_status_get_property;
  object_class->set_property = ide_cell_renderer_status_set_property;

  renderer_class->get_preferred_height = ide_cell_renderer_status_get_preferred_height;
  renderer_class->get_preferred_width = ide_cell_renderer_status_get_preferred_width;
  renderer_class->render = ide_cell_renderer_status_render;

  properties [PROP_FLAGS] =
    g_param_spec_uint ("flags",
                       "Flags",
                       "The flags for the state",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_cell_renderer_status_init (IdeCellRendererStatus *self)
{
}

GtkCellRenderer *
ide_cell_renderer_status_new (void)
{
  return g_object_new (IDE_TYPE_CELL_RENDERER_STATUS, NULL);
}

void
ide_cell_renderer_status_set_flags (IdeCellRendererStatus *self,
                                    IdeTreeNodeFlags       flags)
{
  g_return_if_fail (IDE_IS_CELL_RENDERER_STATUS (self));

  self->flags = flags;
}
