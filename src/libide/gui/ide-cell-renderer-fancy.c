/* ide-cell-renderer-fancy.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-cell-renderer-fancy"

#include "config.h"

#include "ide-cell-renderer-fancy.h"

#define TITLE_SPACING 3

struct _IdeCellRendererFancy
{
  GtkCellRenderer parent_instance;

  gchar *title;
  gchar *body;
};

enum {
  PROP_0,
  PROP_BODY,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_TYPE (IdeCellRendererFancy, ide_cell_renderer_fancy, GTK_TYPE_CELL_RENDERER)

static GParamSpec *properties [N_PROPS];

static PangoLayout *
get_layout (IdeCellRendererFancy *self,
            GtkWidget            *widget,
            const gchar          *text,
            gboolean              is_title,
            GtkCellRendererState  flags)
{
  PangoLayout *l;
  PangoAttrList *attrs;
  GtkStyleContext *style = gtk_widget_get_style_context (widget);
  GtkStateFlags state = gtk_style_context_get_state (style);
  GdkRGBA rgba;

  l = gtk_widget_create_pango_layout (widget, text);

  if (text == NULL || *text == 0)
    return l;

  attrs = pango_attr_list_new ();

  gtk_style_context_get_color (style, state, &rgba);
  pango_attr_list_insert (attrs,
                          pango_attr_foreground_new (rgba.red * 65535,
                                                     rgba.green * 65535,
                                                     rgba.blue * 65535));

  if (is_title)
    {
      pango_attr_list_insert (attrs, pango_attr_scale_new (0.8333));
      pango_attr_list_insert (attrs, pango_attr_foreground_alpha_new (65536 * 0.5));
    }

  pango_layout_set_attributes (l, attrs);
  pango_attr_list_unref (attrs);

  return l;
}

static GtkSizeRequestMode
ide_cell_renderer_fancy_get_request_mode (GtkCellRenderer *cell)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
ide_cell_renderer_fancy_get_preferred_width (GtkCellRenderer *cell,
                                             GtkWidget       *widget,
                                             gint            *min_width,
                                             gint            *nat_width)
{
  IdeCellRendererFancy *self = (IdeCellRendererFancy *)cell;
  PangoLayout *body;
  PangoLayout *title;
  gint body_width = 0;
  gint title_width = 0;
  gint dummy;
  gint xpad;
  gint ypad;

  if (min_width == NULL)
    min_width = &dummy;

  if (nat_width == NULL)
    nat_width = &dummy;

  g_assert (IDE_IS_CELL_RENDERER_FANCY (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (min_width != NULL);
  g_assert (nat_width != NULL);

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  body = get_layout (self, widget, self->body, FALSE, 0);
  title = get_layout (self, widget, self->title, TRUE, 0);

  pango_layout_set_width (body, -1);
  pango_layout_set_width (title, -1);

  pango_layout_get_pixel_size (body, &body_width, NULL);
  pango_layout_get_pixel_size (title, &title_width, NULL);

  *min_width = xpad * 2;
  *nat_width = (xpad * 2) + MAX (title_width, body_width);

  g_object_unref (body);
  g_object_unref (title);
}

static void
ide_cell_renderer_fancy_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                        GtkWidget       *widget,
                                                        gint             width,
                                                        gint            *min_height,
                                                        gint            *nat_height)
{
  IdeCellRendererFancy *self = (IdeCellRendererFancy *)cell;
  PangoLayout *body;
  PangoLayout *title;
  GtkAllocation alloc;
  gint body_height = 0;
  gint title_height = 0;
  gint xpad;
  gint ypad;
  gint dummy;

  if (min_height == NULL)
    min_height = &dummy;

  if (nat_height == NULL)
    nat_height = &dummy;

  g_assert (IDE_IS_CELL_RENDERER_FANCY (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (min_height != NULL);
  g_assert (nat_height != NULL);

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  /*
   * HACK: @width is the min_width returned in our get_preferred_width()
   *       function. That results in pretty bad values here, so we will
   *       do this by assuming we are the onl widget in the tree view.
   *
   *       This makes this cell very much not usable for generic situations,
   *       but it does make it so we can do text wrapping without resorting
   *       to GtkListBox *for our exact usecase only*.
   *
   *       The problem here is that we require the widget to already be
   *       realized and allocated and that we are the only renderer
   *       within the only column (and also, in a treeview) without
   *       exotic styling.
   *
   *       If we get something absurdly small (like 50) that is because we
   *       are hitting our minimum size of (xpad * 2). So this works around
   *       the issue and tries to get something reasonable with wrapping
   *       at the 200px mark (our ~default width for panels).
   *
   *       Furthermore, we need to queue a resize when the column size
   *       changes (as it will from resizing the widget). So the tree
   *       view must also call gtk_tree_view_column_queue_resize().
   */
  gtk_widget_get_allocation (widget, &alloc);
  if (alloc.width > width)
    width = alloc.width - (xpad * 2);
  else if (alloc.width < 50)
    width = 200;

  body = get_layout (self, widget, self->body, FALSE, 0);
  title = get_layout (self, widget, self->title, TRUE, 0);

  pango_layout_set_width (body, width * PANGO_SCALE);
  pango_layout_set_width (title, width * PANGO_SCALE);
  pango_layout_get_pixel_size (title, NULL, &title_height);
  pango_layout_get_pixel_size (body, NULL, &body_height);
  *min_height = *nat_height = (ypad * 2) + title_height + TITLE_SPACING + body_height;

  g_object_unref (body);
  g_object_unref (title);
}

static void
ide_cell_renderer_fancy_render (GtkCellRenderer      *renderer,
                                cairo_t              *cr,
                                GtkWidget            *widget,
                                const GdkRectangle   *bg_area,
                                const GdkRectangle   *cell_area,
                                GtkCellRendererState  flags)
{
  IdeCellRendererFancy *self = (IdeCellRendererFancy *)renderer;
  PangoLayout *body;
  PangoLayout *title;
  gint xpad;
  gint ypad;
  gint height;

  g_assert (IDE_IS_CELL_RENDERER_FANCY (self));
  g_assert (cr != NULL);
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (bg_area != NULL);
  g_assert (cell_area != NULL);

  gtk_cell_renderer_get_padding (renderer, &xpad, &ypad);

  body = get_layout (self, widget, self->body, FALSE, flags);
  title = get_layout (self, widget, self->title, TRUE, flags);

  pango_layout_set_width (title, (cell_area->width - (xpad * 2)) * PANGO_SCALE);
  pango_layout_set_width (body, (cell_area->width - (xpad * 2)) * PANGO_SCALE);

  cairo_move_to (cr, cell_area->x + xpad, cell_area->y + ypad);
  pango_cairo_show_layout (cr, title);

  pango_layout_get_pixel_size (title, NULL, &height);
  cairo_move_to (cr, cell_area->x + xpad, cell_area->y +ypad + + height + TITLE_SPACING);
  pango_cairo_show_layout (cr, body);

  g_object_unref (body);
  g_object_unref (title);
}

static void
ide_cell_renderer_fancy_finalize (GObject *object)
{
  IdeCellRendererFancy *self = (IdeCellRendererFancy *)object;

  g_clear_pointer (&self->body, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_cell_renderer_fancy_parent_class)->finalize (object);
}

static void
ide_cell_renderer_fancy_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeCellRendererFancy *self = IDE_CELL_RENDERER_FANCY (object);

  switch (prop_id)
    {
    case PROP_BODY:
      g_value_set_string (value, self->body);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_cell_renderer_fancy_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeCellRendererFancy *self = IDE_CELL_RENDERER_FANCY (object);

  switch (prop_id)
    {
    case PROP_BODY:
      ide_cell_renderer_fancy_set_body (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_cell_renderer_fancy_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_cell_renderer_fancy_class_init (IdeCellRendererFancyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->finalize = ide_cell_renderer_fancy_finalize;
  object_class->get_property = ide_cell_renderer_fancy_get_property;
  object_class->set_property = ide_cell_renderer_fancy_set_property;

  cell_class->get_request_mode = ide_cell_renderer_fancy_get_request_mode;
  cell_class->get_preferred_width = ide_cell_renderer_fancy_get_preferred_width;
  cell_class->get_preferred_height_for_width = ide_cell_renderer_fancy_get_preferred_height_for_width;
  cell_class->render = ide_cell_renderer_fancy_render;

  /* Note that we do not emit notify for these properties */

  properties [PROP_BODY] =
    g_param_spec_string ("body",
                         "Body",
                         "The body of the renderer",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the renderer",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_cell_renderer_fancy_init (IdeCellRendererFancy *self)
{
}

const gchar *
ide_cell_renderer_fancy_get_title (IdeCellRendererFancy *self)
{
  return self->title;
}

/**
 * ide_cell_renderer_fancy_take_title:
 * @self: a #IdeCellRendererFancy
 * @title: (transfer full) (nullable): the new title
 *
 * Like ide_cell_renderer_fancy_set_title() but takes ownership
 * of @title, saving a string copy.
 *
 * Since: 3.32
 */
void
ide_cell_renderer_fancy_take_title (IdeCellRendererFancy *self,
                                    gchar                *title)
{
  if (self->title != title)
    {
      g_free (self->title);
      self->title = title;
    }
}

void
ide_cell_renderer_fancy_set_title (IdeCellRendererFancy *self,
                                   const gchar          *title)
{
  ide_cell_renderer_fancy_take_title (self, g_strdup (title));
}

const gchar *
ide_cell_renderer_fancy_get_body (IdeCellRendererFancy *self)
{
  return self->body;
}

void
ide_cell_renderer_fancy_set_body (IdeCellRendererFancy *self,
                                  const gchar          *body)
{
  if (self->body != body)
    {
      g_free (self->body);
      self->body = g_strdup (body);
    }
}
