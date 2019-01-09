/* gstyle-palette-widget.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "gstyle-palette-widget"

#include <cairo/cairo.h>
#include <dazzle.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <math.h>

#include "gstyle-css-provider.h"
#include "gstyle-color.h"
#include "gstyle-color-widget.h"
#include "gstyle-utils.h"

#include "gstyle-palette-widget.h"

/*
 * Each palette are refed twice, once when added to the #GListStore,
 * once when they are binded to the display widget (#GtkListBox or #GtkFlowBox ).
 * Both references are removed when the #GstylePalette is removed from the #GstylePaletteWidget.
 */

struct _GstylePaletteWidget
{
  GtkBin                           parent_instance;

  GstyleCssProvider               *default_provider;
  GListStore                      *palettes;
  GstylePalette                   *selected_palette;

  GtkWidget                       *placeholder_box;
  GtkWidget                       *placeholder;
  GtkStack                        *view_stack;
  GtkWidget                       *listbox;
  GtkWidget                       *flowbox;

  GstyleColor                     *dnd_color;
  gint                             dnd_child_index;
  GdkPoint                         dnd_last_pos;
  guint                            dnd_last_time;
  gdouble                          dnd_speed;
  gboolean                         is_on_drag;

  GstylePaletteWidgetViewMode      view_mode;
  GstylePaletteWidgetSortMode      sort_mode;

  GstylePaletteWidgetDndLockFlags  dnd_lock : 2;
  guint                            current_has_load : 1;
  guint                            current_has_save : 1;

  guint                            dnd_draw_highlight : 1;
  guint                            is_dnd_at_end : 1;
};

G_DEFINE_TYPE (GstylePaletteWidget, gstyle_palette_widget, GTK_TYPE_BIN)

#define GSTYLE_DND_SPEED_THRESHOLD 50
#define DND_INDEX_START (G_MININT)

#define GSTYLE_COLOR_FUZZY_SEARCH_MAX_LEN 20

#define GSTYLE_COLOR_WIDGET_SWATCH_WIDTH 64
#define GSTYLE_COLOR_WIDGET_SWATCH_HEIGHT 64

static guint unsaved_palette_count = 0;

enum {
  PROP_0,
  PROP_DND_LOCK,
  PROP_PLACEHOLDER,
  PROP_SELECTED_PALETTE_ID,
  PROP_VIEW_MODE,
  PROP_SORT_MODE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum {
  ACTIVATED,
  PALETTE_ADDED,
  PALETTE_REMOVED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

typedef struct _CursorInfo
{
  GstyleColorWidget *child;
  gint               index;
  gint               dest_x;
  gint               dest_y;
  gint               nb_col;
} CursorInfo;

static gint
flowbox_get_nb_col (GstylePaletteWidget *self,
                    GtkFlowBox          *flowbox)
{
  GtkFlowBoxChild *child;
  GtkAllocation alloc;
  guint min_per_line;
  guint max_per_line;
  gint previous_x = -1;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GTK_IS_FLOW_BOX (flowbox));
  g_assert (gtk_flow_box_get_homogeneous (flowbox) == TRUE);

  /* Child width is not perfectly constant, so we need some tricks */
  min_per_line = gtk_flow_box_get_min_children_per_line (flowbox);
  if (min_per_line < 1)
    min_per_line = 1;

  max_per_line = gtk_flow_box_get_max_children_per_line (flowbox);
  if (max_per_line < 1)
    max_per_line = 20;

  for (guint i = 0; i <= max_per_line; ++i)
    {
      child = gtk_flow_box_get_child_at_index (flowbox, i);
      if (child != NULL)
        {
          gtk_widget_get_allocation (GTK_WIDGET (child), &alloc);
          if (alloc.x > previous_x)
            {
              previous_x = alloc.x;
              continue;
            }
        }

      return i;
    }

  return -1;
}

/* We make the assumption that the list is in order of apparition,
 * in vertical orientation, with all children visible and
 * homogenous set.
 */
static GtkFlowBoxChild *
flowbox_get_child_at_xy (GstylePaletteWidget *self,
                         gint                 x,
                         gint                 y,
                         gint                *index,
                         gint                *nb_col)
{
  GtkFlowBox *flowbox;
  GtkFlowBoxChild *child;
  GtkAllocation alloc;
  gint row_spacing;
  guint right;
  gint bottom;
  guint index_y = 0;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (index != NULL);
  g_assert (nb_col != NULL);

  *index = -1;
  if (x == -1 || y == -1)
    return NULL;

  flowbox = GTK_FLOW_BOX (self->flowbox);

  g_assert (gtk_flow_box_get_homogeneous (flowbox) == TRUE);

  child = gtk_flow_box_get_child_at_index (flowbox, 0);
  if (child == NULL)
    return NULL;

  gtk_widget_get_allocation (GTK_WIDGET (child), &alloc);
  row_spacing = gtk_flow_box_get_row_spacing (flowbox);

  *nb_col = flowbox_get_nb_col (self, flowbox);
  index_y = (y / (alloc.height + row_spacing) * (*nb_col));
  for (guint i = index_y; i < (index_y + *nb_col); ++i)
    {
      child = gtk_flow_box_get_child_at_index (flowbox, i);
      if (child != NULL)
        {
          gtk_widget_get_allocation (GTK_WIDGET (child), &alloc);
          right = alloc.x + alloc.width;
          bottom = alloc.y + alloc.height;
          if (alloc.x <= x && x < right && alloc.y <= y && y < bottom)
            {
              *index = i;
              return child;
            }
        }
    }

  return NULL;
}

/* x,y are in #GstylePaletteWidget coordinates.
 * if the cursor is on the placeholder, %FALSE is returned.
 */
static gboolean
dnd_get_index_from_cursor (GstylePaletteWidget *self,
                           gint                 x,
                           gint                 y,
                           CursorInfo          *info)
{
  GtkBin *bin_child;
  GtkAllocation alloc;
  gint len;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (info != NULL);

  if (self->view_mode == GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST)
    {
      gtk_widget_translate_coordinates (GTK_WIDGET (self), self->listbox, x, y, &info->dest_x, &info->dest_y);
      bin_child = GTK_BIN (gtk_list_box_get_row_at_y (GTK_LIST_BOX (self->listbox), info->dest_y));
      if (bin_child == NULL)
        {
          /* No child mean we are at list start or at list end */
          len = gstyle_palette_get_len (self->selected_palette);
          if (len == 0)
            return FALSE;

          bin_child = GTK_BIN (gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), 0));
          gtk_widget_get_allocated_size (GTK_WIDGET (bin_child), &alloc, NULL);
          if (info->dest_y < alloc.y)
            {
              info->index = 0;
              info->child = GSTYLE_COLOR_WIDGET (gtk_bin_get_child (GTK_BIN (bin_child)));
              return TRUE;
            }

          bin_child = GTK_BIN (gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), len - 1));
        }

      info->index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (bin_child));
      info->child = GSTYLE_COLOR_WIDGET (gtk_bin_get_child (GTK_BIN (bin_child)));
    }
  else
    {
      gtk_widget_translate_coordinates (GTK_WIDGET (self), self->flowbox, x, y, &info->dest_x, &info->dest_y);
      bin_child = GTK_BIN (flowbox_get_child_at_xy (self, info->dest_x, info->dest_y, &info->index, &info->nb_col));
      if (bin_child == NULL)
        {
          len = gstyle_palette_get_len (self->selected_palette);
          if (len == 0)
            return FALSE;

          bin_child = GTK_BIN (gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (self->flowbox), 0));
          gtk_widget_get_allocated_size (GTK_WIDGET (bin_child), &alloc, NULL);
          if (info->dest_x < alloc.x && info->dest_y < alloc.y + alloc.height)
            {
              info->index = 0;
              info->child = GSTYLE_COLOR_WIDGET (gtk_bin_get_child (GTK_BIN (bin_child)));
              return TRUE;
            }

          bin_child = GTK_BIN (gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (self->flowbox), len - 1));
          gtk_widget_get_allocated_size (GTK_WIDGET (bin_child), &alloc, NULL);
          info->dest_x = alloc.x + alloc.width;
        }

      info->index = gtk_flow_box_child_get_index (GTK_FLOW_BOX_CHILD (bin_child));
      info->child = GSTYLE_COLOR_WIDGET (gtk_bin_get_child (GTK_BIN (bin_child)));
    }

  return TRUE;
}

/* Returns: The insertion index
 * x and y are in #GstylePaletteWidget coordinates.
 * if either x or y are equal to -1, then the highlight
 * is removed, and -1 is returned in this case.
 */
static gint
dnd_highlight_set_from_cursor (GstylePaletteWidget *self,
                               gint                 x,
                               gint                 y)
{
  GtkAllocation alloc;
  CursorInfo info;
  gboolean highlight;
  gboolean redraw = FALSE;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));

  if (x == -1 || y == -1)
    {
      highlight = FALSE;
      info.index = -1;
    }
  else if (dnd_get_index_from_cursor (self, x, y, &info))
    {
      gtk_widget_get_allocation (GTK_WIDGET (info.child), &alloc);
      if (self->view_mode == GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST)
        {
          gint len;

          if (info.dest_y > (alloc.y + alloc.height * 0.80))
            info.index += 1;
          else if (info.dest_y > (alloc.y + alloc.height * 0.20))
            info.index = -1;

          len = gstyle_palette_get_len (self->selected_palette);
          self->is_dnd_at_end = (info.index == len);
        }
      else
        {
          /* Check if we in the rightmost column */
          self->is_dnd_at_end = ((info.index + 1) % info.nb_col == 0 && info.index != 0);

          if (info.dest_x > (alloc.x + alloc.width * 0.80))
            info.index += 1;
          else if (info.dest_x > (alloc.x + alloc.width * 0.20))
            info.index = -1;
        }

      highlight = TRUE;
    }
  else
    {
      self->is_dnd_at_end = FALSE;
      info.index = gstyle_palette_get_len (self->selected_palette);
      highlight = TRUE;
    }

  if (self->dnd_draw_highlight != highlight || self->dnd_child_index != info.index)
    redraw = TRUE;

  self->dnd_child_index = info.index;
  self->dnd_draw_highlight = highlight;

  if (redraw)
    {
      if (self->view_mode == GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST)
        gtk_widget_queue_draw (self->listbox);
      else
        gtk_widget_queue_draw (self->flowbox);
    }

  return info.index;
}

/* TODO: set the palette to need-attention if drop ( mean you need to save */
/* TODO: take into account the case when the src and dst are from the same palette */
/* TODO: do dnd from the flowbox */

static gboolean
gstyle_palette_widget_on_drag_motion (GtkWidget      *widget,
                                      GdkDragContext *context,
                                      gint            x,
                                      gint            y,
                                      guint           time)
{
  GstylePaletteWidget *self = GSTYLE_PALETTE_WIDGET (widget);
  GdkAtom target;
  GdkDragAction drag_action;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (target == gdk_atom_intern_static_string ("GSTYLE_COLOR_WIDGET") &&
      (self->dnd_lock & GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DROP) == 0)
    {
      drag_action = gdk_drag_context_get_actions (context);
      if (drag_action & GDK_ACTION_MOVE)
        {
          dnd_highlight_set_from_cursor (self, x, y);
          gdk_drag_status (context, GDK_ACTION_MOVE, time);
          return TRUE;
        }
      else if (drag_action & GDK_ACTION_COPY)
        {
          dnd_highlight_set_from_cursor (self, x, y);
          gdk_drag_status (context, GDK_ACTION_COPY, time);
          return TRUE;
        }
    }

  dnd_highlight_set_from_cursor (self, -1, -1);
  gdk_drag_status (context, 0, time);
  return FALSE;
}

static void
gstyle_palette_widget_on_drag_leave (GtkWidget      *widget,
                                     GdkDragContext *context,
                                     guint           time)
{
  GstylePaletteWidget *self = GSTYLE_PALETTE_WIDGET (widget);

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  self->dnd_draw_highlight = FALSE;

  if (self->view_mode == GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST)
    gtk_widget_queue_draw (self->listbox);
  else
    gtk_widget_queue_draw (self->flowbox);
}

static gboolean
gstyle_palette_widget_on_drag_drop (GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    guint             time)
{
  GstylePaletteWidget *self = GSTYLE_PALETTE_WIDGET (widget);
  GdkAtom target;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (target != gdk_atom_intern_static_string ("GSTYLE_COLOR_WIDGET") ||
      (self->dnd_lock & GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DROP) != 0)
    {
      dnd_highlight_set_from_cursor (self, -1, -1);
      return FALSE;
    }

  gtk_drag_get_data (widget, context, target, time);
  return TRUE;
}

static void
gstyle_palette_widget_on_drag_data_received (GtkWidget        *widget,
                                             GdkDragContext   *context,
                                             gint              x,
                                             gint              y,
                                             GtkSelectionData *data,
                                             guint             info,
                                             guint             time)
{
  GstylePaletteWidget *self = GSTYLE_PALETTE_WIDGET (widget);
  GstyleColor **src_color;
  GstyleColor *color;
  gboolean delete;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  if (gtk_selection_data_get_target (data) == gdk_atom_intern_static_string ("GSTYLE_COLOR_WIDGET"))
    {
      /* TODO: check if the color widget is coming from a PaletteWidget container */
      src_color = (void*)gtk_selection_data_get_data (data);
      color = gstyle_color_copy (*src_color);
      gstyle_palette_add_at_index (self->selected_palette,
                                   color,
                                   self->dnd_child_index,
                                   NULL);

      g_object_unref (color);
      delete = (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE);
      gtk_drag_finish (context, TRUE, delete, time);
    }
  else
    gtk_drag_finish (context, FALSE, FALSE, time);

  dnd_highlight_set_from_cursor (self, -1, -1);
}

static GPtrArray *
fuzzy_search_lookup (GstylePaletteWidget *self,
                     DzlFuzzyMutableIndex               *fuzzy,
                     const gchar         *key)
{
  g_autoptr (GArray) results = NULL;
  GPtrArray *ar = NULL;
  DzlFuzzyMutableIndexMatch *match;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (fuzzy != NULL);

  results = dzl_fuzzy_mutable_index_match (fuzzy, key, 1);
  if (results!= NULL && results->len > 0)
    {
      match = &g_array_index (results, DzlFuzzyMutableIndexMatch, 0);
      if (g_strcmp0 (match->key, key))
        ar = match->value;
    }

  return ar;
}

/**
 * gstyle_palette_widget_fuzzy_parse_color_string:
 * @color_string: color name to search for
 *
 * Returns: (transfer full) (element-type GstyleColor): a #GPtrArray of #GstyleColor for a fuzzy search.
 */
GPtrArray *
gstyle_palette_widget_fuzzy_parse_color_string (GstylePaletteWidget *self,
                                                const gchar         *color_string)
{
  g_autoptr (GArray) fuzzy_results = NULL;
  DzlFuzzyMutableIndex *fuzzy;
  GPtrArray *results;
  GPtrArray *ar, *ar_list;
  DzlFuzzyMutableIndexMatch *match;
  GstylePalette *palette;
  GstyleColor *color, *new_color;
  const gchar *name;
  gint nb_palettes;
  gint nb_colors;
  gint len;

  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), NULL);

  fuzzy = dzl_fuzzy_mutable_index_new (TRUE);
  ar_list = g_ptr_array_new_with_free_func ((GDestroyNotify)g_ptr_array_unref);
  nb_palettes = gstyle_palette_widget_get_n_palettes (self);
  if (nb_palettes == 0)
    return NULL;

  for (gint n = 0; n < nb_palettes; ++n)
    {
      palette = gstyle_palette_widget_get_palette_at_index (self, n);
      nb_colors = gstyle_palette_get_len (palette);
      for (gint i = 0; i < nb_colors; ++i)
        {
          color = (GstyleColor *)gstyle_palette_get_color_at_index (palette, i);
          name = gstyle_color_get_name (color);
          ar = fuzzy_search_lookup (self, fuzzy, name);
          if (ar == NULL)
            {
              ar = g_ptr_array_new ();
              g_ptr_array_add (ar_list, ar);
              dzl_fuzzy_mutable_index_insert (fuzzy, name, ar);
              g_ptr_array_add (ar, color);
            }
          else if (!gstyle_utils_is_array_contains_same_color (ar, color))
            g_ptr_array_add (ar, color);
        }
    }

  results = g_ptr_array_new_with_free_func (g_object_unref);
  fuzzy_results = dzl_fuzzy_mutable_index_match (fuzzy, color_string, GSTYLE_COLOR_FUZZY_SEARCH_MAX_LEN);
  len = MIN (GSTYLE_COLOR_FUZZY_SEARCH_MAX_LEN, fuzzy_results->len);
  for (gint n = 0; n < len; ++n)
    {
      match = &g_array_index (fuzzy_results, DzlFuzzyMutableIndexMatch, n);
      ar = match->value;
      for (gint i = 0; i < ar->len; ++i)
        {
          color = g_ptr_array_index (ar, i);
          new_color = gstyle_color_copy (color);
          g_ptr_array_add (results, new_color);
        }
    }

  dzl_fuzzy_mutable_index_unref (fuzzy);
  g_ptr_array_free (ar_list, TRUE);
  return results;
}

/**
 * gstyle_palette_widget_set_placeholder:
 * @self: a #GstylePaletteWidget.
 * @placeholder: a #GtkWidget or %NULL.
 *
 * Set a placeholder to show when no palettes are loaded.
 *
 */
void
gstyle_palette_widget_set_placeholder (GstylePaletteWidget *self,
                                       GtkWidget           *placeholder)
{
  g_return_if_fail (GSTYLE_IS_PALETTE_WIDGET (self));
  g_return_if_fail (GTK_IS_WIDGET (self) || placeholder == NULL);

  if (self->placeholder != placeholder)
    {
      if (self->placeholder != NULL)
        gtk_container_remove (GTK_CONTAINER (self->placeholder_box), self->placeholder);

      self->placeholder = placeholder;
      if (placeholder != NULL)
        {
          gtk_container_add (GTK_CONTAINER (self->placeholder_box), placeholder);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PLACEHOLDER]);

          if (self->selected_palette == NULL)
            gtk_stack_set_visible_child_name (self->view_stack, "placeholder");
        }
      else
        gstyle_palette_widget_set_view_mode (self, self->view_mode);
    }
}

/**
 * gstyle_palette_widget_get_placeholder:
 * @self: a #GstylePaletteWidget.
 *
 * Get the current placeholder GtkWidget.
 *
 * Returns: (transfer none): a #GtkObject.
 *
 */
GtkWidget *
gstyle_palette_widget_get_placeholder (GstylePaletteWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), NULL);

  return self->placeholder;
}

static void
gstyle_palette_widget_add_actions (GstylePaletteWidget *self)
{
  GSimpleActionGroup *actions_group;
  GPropertyAction *action;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));

  actions_group = g_simple_action_group_new ();

  action = g_property_action_new ("view-mode", self, "view-mode");
  g_action_map_add_action (G_ACTION_MAP (actions_group), G_ACTION (action));
  action = g_property_action_new ("sort-mode", self, "sort-mode");
  g_action_map_add_action (G_ACTION_MAP (actions_group), G_ACTION (action));

  gtk_widget_insert_action_group (GTK_WIDGET (self), "gstyle-palettes-prefs", G_ACTION_GROUP (actions_group));
}

static gint
gstyle_palette_widget_get_palette_position (GstylePaletteWidget *self,
                                            GstylePalette       *palette)
{
  gint len;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GSTYLE_IS_PALETTE (palette));

  len = g_list_model_get_n_items (G_LIST_MODEL (self->palettes));
  for (gint n = 0; n < len; ++n)
    {
      g_autoptr (GstylePalette) model_palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), n);

      if (palette == model_palette)
        return n;
    }

  return -1;
}

static GtkWidget *
create_palette_list_item (gpointer item,
                          gpointer user_data)
{
  GstyleColor *color = (GstyleColor *)item;
  GtkWidget *row;

  g_assert (GSTYLE_IS_COLOR (color));

  row = g_object_new (GSTYLE_TYPE_COLOR_WIDGET,
                      "color", color,
                      "visible", TRUE,
                      "halign", GTK_ALIGN_FILL,
                      NULL);

  return row;
}

static GtkWidget *
create_palette_flow_item (gpointer item,
                          gpointer user_data)
{
  GstyleColor *color = (GstyleColor *)item;
  g_autofree gchar *color_string = NULL;
  g_autofree gchar *tooltip = NULL;
  const gchar *name;
  GtkWidget *swatch;

  g_assert (GSTYLE_IS_COLOR (color));

  name = gstyle_color_get_name (color);
  if (gstyle_str_empty0 (name))
    tooltip = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_ORIGINAL);
  else
    {
      color_string = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_ORIGINAL);
      tooltip = g_strdup_printf ("%s (%s)", name, color_string);
    }

  swatch = g_object_new (GSTYLE_TYPE_COLOR_WIDGET,
                         "color", color,
                         "visible", TRUE,
                         "name-visible", FALSE,
                         "fallback-name-kind", GSTYLE_COLOR_KIND_RGB_HEX6,
                         "fallback-name-visible", TRUE,
                         "tooltip-text", tooltip,
                         "width-request", GSTYLE_COLOR_WIDGET_SWATCH_WIDTH,
                         "height-request", GSTYLE_COLOR_WIDGET_SWATCH_HEIGHT,
                         NULL);

  return swatch;
}

static void
bind_palette (GstylePaletteWidget *self,
              GstylePalette       *palette)
{
  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (palette == NULL || GSTYLE_IS_PALETTE (palette));
  g_assert (palette == NULL || gstyle_palette_widget_get_palette_position (self, palette) != -1);

  if (self->view_mode == GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST)
    {
      gtk_flow_box_bind_model (GTK_FLOW_BOX (self->flowbox), NULL, NULL, NULL, NULL);
      if (palette != NULL)
        {
          gtk_list_box_bind_model (GTK_LIST_BOX (self->listbox), G_LIST_MODEL (palette),
                                   create_palette_list_item, self, NULL);
          gtk_stack_set_visible_child_name (self->view_stack, "list");
        }
      else
        gtk_list_box_bind_model (GTK_LIST_BOX (self->listbox), NULL, NULL, NULL, NULL);
    }
  else
    {
      gtk_list_box_bind_model (GTK_LIST_BOX (self->listbox), NULL, NULL, NULL, NULL);
      if (palette != NULL)
        {
          gtk_flow_box_bind_model (GTK_FLOW_BOX (self->flowbox),  G_LIST_MODEL (palette),
                                   create_palette_flow_item, self, NULL);
          gtk_stack_set_visible_child_name (self->view_stack, "flow");
        }
      else
        gtk_flow_box_bind_model (GTK_FLOW_BOX (self->flowbox), NULL, NULL, NULL, NULL);
    }

  self->selected_palette = palette;
}

static const gchar *
gstyle_palette_widget_get_selected_palette_id (GstylePaletteWidget *self)
{
  GstylePalette *palette;

  palette = gstyle_palette_widget_get_selected_palette (self);
  if (palette != NULL)
    return gstyle_palette_get_id (palette);
  else
    return "\0";
}

static void
gstyle_palette_widget_set_selected_palette_by_id (GstylePaletteWidget *self,
                                                  const gchar         *palette_id)
{
  const gchar *current_palette_id;
  gint len;

  len = g_list_model_get_n_items (G_LIST_MODEL (self->palettes));
  for (gint n = 0; n < len; ++n)
    {
      g_autoptr (GstylePalette) palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), n);

      current_palette_id = gstyle_palette_get_id (palette);
      if (g_strcmp0 (current_palette_id, palette_id) == 0)
        gstyle_palette_widget_show_palette (self, palette);
    }
}

/**
 * gstyle_palette_widget_get_selected_palette:
 * @self: a #GstylePaletteWidget
 *
 * Return the selected #GstylePalette.
 *
 * Returns: (transfer none) (allow-none): a #GstylePalette or %NULL if none are selected or loaded.
 *
 */
GstylePalette *
gstyle_palette_widget_get_selected_palette (GstylePaletteWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), FALSE);

  return self->selected_palette;
}

/**
 * gstyle_palette_widget_show_palette:
 * @self: a #GstylePaletteWidget
 * @palette: a #GstylePalette
 *
 * Show @palette in the widget.
 *
 * Returns: %TRUE on succes, %FALSE if @palette is not in the list.
 *
 */
gboolean
gstyle_palette_widget_show_palette (GstylePaletteWidget *self,
                                    GstylePalette       *palette)
{
  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), FALSE);
  g_return_val_if_fail (GSTYLE_IS_PALETTE (palette), FALSE);

  if (self->selected_palette != palette)
    {
      if (gstyle_palette_widget_get_palette_position (self, palette) == -1)
        return FALSE;

      bind_palette (self, palette);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTED_PALETTE_ID]);
    }

  return TRUE;
}

static void
gstyle_palette_widget_color_swatch_activated (GstylePaletteWidget *self,
                                              GtkFlowBoxChild     *child,
                                              GtkFlowBox          *flowbox)
{
  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GTK_IS_FLOW_BOX (flowbox));
  g_assert (GTK_IS_FLOW_BOX_CHILD (child));

  g_signal_emit (self, signals [ACTIVATED], 0, self->selected_palette, gtk_flow_box_child_get_index (child));
}

static void
gstyle_palette_widget_color_row_activated (GstylePaletteWidget *self,
                                           GtkListBoxRow       *row,
                                           GtkListBox          *listbox)
{
  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GTK_IS_LIST_BOX (listbox));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  g_signal_emit (self, signals [ACTIVATED], 0, self->selected_palette, gtk_list_box_row_get_index (row));
}

/**
 * gstyle_palette_widget_get_n_palettes:
 * @self: a #GstylePaletteWidget
 *
 * Get the number of #GstylePalette in the palettes list.
 *
 * Returns: number of palettes in the list.
 *
 */
gint
gstyle_palette_widget_get_n_palettes (GstylePaletteWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self->palettes));;
}

/**
 * gstyle_palette_widget_get_palette_at_index:
 * @self: a #GstylePaletteWidget
 * @index: A position in the palette list, from 0 to (n - 1) palettes
 *
 * Get the #GstylePalette ref at position @index in the palettes list.
 *
 * Returns: (transfer none): a #GstylePalette or %NULL if index is out of bounds.
 *
 */
GstylePalette *
gstyle_palette_widget_get_palette_at_index (GstylePaletteWidget *self,
                                            guint                index)
{
  g_autoptr (GstylePalette) palette = NULL;

  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), FALSE);

  palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), index);
  return palette;
}

/**
 * gstyle_palette_widget_get_store:
 * @self: a #GstylePaletteWidget
 *
 * Return a #GListStore containing the palettes.
 *
 * Returns: (transfer none): #GListStore.
 *
 */
GListStore *
gstyle_palette_widget_get_store (GstylePaletteWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), FALSE);

  return self->palettes;
}

/**
 * gstyle_palette_widget_get_list:
 * @self: a #GstylePaletteWidget
 *
 * Return a #GList of the palettes.
 *
 * Returns: (transfer container) (element-type GstylePalette): #GList of #GstylePalette.
 *
 */
GList *
gstyle_palette_widget_get_list (GstylePaletteWidget *self)
{
  GList *l = NULL;
  gint len;

  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), FALSE);

  len = g_list_model_get_n_items (G_LIST_MODEL (self->palettes));
  if (len > 0)
    for (gint n = (len - 1); n >= 0; --n)
    {
      g_autoptr (GstylePalette) palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), n);

      l = g_list_prepend (l, palette);
    }

  return l;
}

/**
 * gstyle_palette_widget_add:
 * @self: a #GstylePaletteWidget
 * @palette: a #GstylePalette
 *
 * Add @palette to the widget list.
 *
 * Returns: %TRUE on succes, %FALSE if @palette is already in the list.
 *
 */
gboolean
gstyle_palette_widget_add (GstylePaletteWidget *self,
                           GstylePalette       *palette)
{
  const gchar *id;
  const gchar *list_id;
  const gchar *palette_name;
  gchar *name;
  gint len;

  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), FALSE);
  g_return_val_if_fail (GSTYLE_IS_PALETTE (palette), FALSE);

  id = gstyle_palette_get_id (palette);
  len = g_list_model_get_n_items (G_LIST_MODEL (self->palettes));
  for (gint n = 0; n < len; ++n)
    {
      g_autoptr (GstylePalette) list_palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), n);

      list_id = gstyle_palette_get_id (list_palette);
      if (g_strcmp0 (id, list_id) == 0)
        return FALSE;
    }

  palette_name = gstyle_palette_get_name (palette);
  if (gstyle_str_empty0 (palette_name))
    {
      name = g_strdup_printf (_("Unsaved palette %u"), ++unsaved_palette_count);
      gstyle_palette_set_name (palette, name);
      g_free (name);
    }

  g_list_store_append (self->palettes, palette);
  g_signal_emit (self, signals [PALETTE_ADDED], 0, palette);

  return TRUE;
}

/**
 * gstyle_palette_widget_remove_all:
 * @self: a #GstylePaletteWidget
 *
 * Remove all palettes in the widget list.
 *
 */
void
gstyle_palette_widget_remove_all (GstylePaletteWidget *self)
{
  gint len;

  g_return_if_fail (GSTYLE_IS_PALETTE_WIDGET (self));

  len = g_list_model_get_n_items (G_LIST_MODEL (self->palettes));
  for (gint n = 0; n < len; ++n)
    {
      g_autoptr (GstylePalette) palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), n);

      g_signal_emit (self, signals [PALETTE_REMOVED], 0, palette);
    }

  bind_palette (self, NULL);
  g_list_store_remove_all (self->palettes);

  gtk_stack_set_visible_child_name (self->view_stack, "placeholder");
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTED_PALETTE_ID]);
}

/**
 * gstyle_palette_widget_remove:
 * @self: a #GstylePaletteWidget
 * @palette: a #GstylePalette
 *
 * Remove @palette in the widget list.
 *
 * Returns: %TRUE on succes, %FALSE if @palette is not in the list.
 *
 */
gboolean
gstyle_palette_widget_remove (GstylePaletteWidget *self,
                              GstylePalette       *palette)
{
  gint len;
  gint next;

  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), FALSE);
  g_return_val_if_fail (GSTYLE_IS_PALETTE (palette), FALSE);

  len = g_list_model_get_n_items (G_LIST_MODEL (self->palettes));
  for (gint n = 0; n < len; ++n)
    {
      g_autoptr (GstylePalette) next_palette = NULL;
      g_autoptr (GstylePalette) model_palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), n);

      if (palette == model_palette)
        {
          if (palette == self->selected_palette)
            bind_palette (self, NULL);

          g_list_store_remove (self->palettes, n);
          g_signal_emit (self, signals [PALETTE_REMOVED], 0, palette);

          len -= 1;
          if (len > 0)
            {
              next = (n == len) ? (n - 1) : n;
              next_palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), next);
              if (next_palette != NULL)
                gstyle_palette_widget_show_palette (self, next_palette);
            }
          else
            {
              gtk_stack_set_visible_child_name (self->view_stack, "placeholder");
              g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTED_PALETTE_ID]);
            }

          return TRUE;
        }
    }

  return FALSE;

}

/**
 * gstyle_palette_widget_get_palette_by_id:
 * @self: a #GstylePaletteWidget
 * @id: A palette id string
 *
 * Return the corresponding #GstylePalette if in the #GstylePaletteWidget list.
 *
 * Returns: (transfer none): a #GstylePalette if in the list, %NULL otherwise.
 *
 */
GstylePalette *
gstyle_palette_widget_get_palette_by_id (GstylePaletteWidget *self,
                                         const gchar         *id)
{
  const gchar *palette_id;
  gint len;

  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), NULL);
  g_return_val_if_fail (!gstyle_str_empty0 (id), NULL);

  len = g_list_model_get_n_items (G_LIST_MODEL (self->palettes));
  for (gint n = 0; n < len; ++n)
    {
      g_autoptr (GstylePalette) palette = g_list_model_get_item (G_LIST_MODEL (self->palettes), n);

      palette_id = gstyle_palette_get_id (palette);
      if (g_strcmp0 (palette_id, id) == 0)
        return palette;
    }

  return NULL;
}

/**
 * gstyle_palette_widget_remove_by_id:
 * @self: a #GstylePaletteWidget
 * @id: A palette id string
 *
 * Remove palette with @id  from the widget list.
 *
 * Returns: %TRUE on succes, %FALSE if not in the palettes list.
 *
 */
gboolean
gstyle_palette_widget_remove_by_id (GstylePaletteWidget *self,
                                    const gchar         *id)
{
  GstylePalette *palette;

  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), FALSE);
  g_return_val_if_fail (!gstyle_str_empty0 (id), FALSE);

  palette = gstyle_palette_widget_get_palette_by_id (self, id);
  if (palette != NULL)
    {
      gstyle_palette_widget_remove (self, palette);
      return TRUE;
    }

  return FALSE;
}

/**
 * gstyle_palette_widget_set_view_mode:
 * @self: a #GstylePaletteWidget
 * @mode: a #GstylePaletteWidgetViewMode
 *
 * Sets the view mode of the palette widget.
 *
 */
void
gstyle_palette_widget_set_view_mode (GstylePaletteWidget         *self,
                                     GstylePaletteWidgetViewMode  mode)
{
  g_return_if_fail (GSTYLE_IS_PALETTE_WIDGET (self));

  if (self->view_mode != mode)
    {
      self->view_mode = mode;
      self->dnd_child_index = -1;
      bind_palette (self, self->selected_palette);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VIEW_MODE]);
    }


  if (self->selected_palette != NULL || self->placeholder == NULL)
    {
      if (mode == GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST)
        gtk_stack_set_visible_child_name (self->view_stack, "list");
      else
        gtk_stack_set_visible_child_name (self->view_stack, "flow");
    }
}

/**
 * gstyle_palette_widget_get_view_mode:
 * @self: a #GstylePaletteWidget
 *
 * Get the view mode of the palette widget.
 *
 * Returns: The #GstylePaletteWidgetViewMode.
 *
 */
GstylePaletteWidgetViewMode
gstyle_palette_widget_get_view_mode (GstylePaletteWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST);

  return self->view_mode;
}

/**
 * gstyle_palette_widget_set_dnd_lock:
 * @self: a #GstylePaletteWidget
 * @flags: One or more #GstylePaletteWidgetDndLockFlags
 *
 * Sets the dnd lock flags of the palette widget.
 *
 */
void
gstyle_palette_widget_set_dnd_lock (GstylePaletteWidget             *self,
                                    GstylePaletteWidgetDndLockFlags  flags)
{
  g_return_if_fail (GSTYLE_IS_PALETTE_WIDGET (self));

  if (self->dnd_lock != flags)
  {
    self->dnd_lock = flags;
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DND_LOCK]);
  }
}

/**
 * gstyle_palette_widget_get_dnd_lock:
 * @self: a #GstylePaletteWidget
 *
 * Get the dnd lock flags of the palette widget.
 *
 * Returns: The #GstylePaletteWidgetDndLockFlags.
 *
 */
GstylePaletteWidgetDndLockFlags
gstyle_palette_widget_get_dnd_lock (GstylePaletteWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_NONE);

  return self->dnd_lock;
}

/**
 * gstyle_palette_widget_set_sort_mode:
 * @self: a #GstylePaletteWidget
 * @mode: a #GstylePaletteWidgetViewMode
 *
 * Sets the sort mode of the palette widget.
 *
 */
void
gstyle_palette_widget_set_sort_mode (GstylePaletteWidget         *self,
                                     GstylePaletteWidgetSortMode  mode)
{
  g_return_if_fail (GSTYLE_IS_PALETTE_WIDGET (self));

  if (self->sort_mode != mode)
  {
    self->sort_mode = mode;
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SORT_MODE]);
  }
}

/**
 * gstyle_palette_widget_get_sort_mode:
 * @self: a #GstylePaletteWidget
 *
 * Get the sort mode of the palette widget.
 *
 * Returns: The #GstylePaletteWidgetSortMode.
 *
 */
GstylePaletteWidgetSortMode
gstyle_palette_widget_get_sort_mode (GstylePaletteWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_PALETTE_WIDGET (self), GSTYLE_PALETTE_WIDGET_SORT_MODE_ORIGINAL);

  return self->sort_mode;
}

/**
 * gstyle_palette_widget_new:
 *
 * Create a new #GstylePaletteWidget.
 *
 * Returns: a #GstylePaletteWidget.
 *
 */
GstylePaletteWidget *
gstyle_palette_widget_new (void)
{
  return g_object_new (GSTYLE_TYPE_PALETTE_WIDGET, NULL);
}

static gboolean
listbox_draw_cb (GtkWidget           *listbox,
                 cairo_t             *cr,
                 GstylePaletteWidget *self)
{
  GtkStyleContext *style_context;
  GstylePalette *selected_palette;
  GtkListBoxRow *bin_child;
  GtkAllocation alloc;
  gint y;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GTK_IS_LIST_BOX (listbox));

  if (self->dnd_draw_highlight && self->dnd_child_index != -1)
    {
      style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_DND);

      selected_palette = gstyle_palette_widget_get_selected_palette (self);
      if (selected_palette == NULL || gstyle_palette_get_len (selected_palette) == 0)
        {
          gtk_widget_get_allocation (listbox, &alloc);
          y = 2;
        }
      else
        {
          if (self->is_dnd_at_end)
            {
              bin_child = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), self->dnd_child_index - 1);
              gtk_widget_get_allocation (GTK_WIDGET (bin_child), &alloc);
              y = alloc.y + alloc.height - 2;
            }
          else
            {
              bin_child = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), self->dnd_child_index);
              gtk_widget_get_allocation (GTK_WIDGET (bin_child), &alloc);
              y = alloc.y - 2;
              if (y < 0)
                y = 0;
            }
        }

      gtk_render_background (style_context, cr, alloc.x, y, alloc.width, 4);
      gtk_render_frame (style_context, cr, alloc.x, y, alloc.width, 4);
    }

  return FALSE;
}

static gboolean
flowbox_draw_cb (GtkWidget           *flowbox,
                 cairo_t             *cr,
                 GstylePaletteWidget *self)
{
  GtkStyleContext *style_context;
  GtkFlowBoxChild *bin_child;
  GtkAllocation alloc;
  gint len;
  gint x = 0;

  g_assert (GSTYLE_IS_PALETTE_WIDGET (self));
  g_assert (GTK_IS_FLOW_BOX (flowbox));

  if (self->dnd_draw_highlight)
    {
      style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_DND);

      if (self->dnd_child_index != -1)
        {
          len = gstyle_palette_get_len (self->selected_palette);
          if (len == 0)
            {
              gtk_widget_get_allocation (flowbox, &alloc);
              gtk_render_background (style_context, cr, 0, 0, alloc.width, 4);
              gtk_render_frame (style_context, cr, 0, 0, alloc.width, 4);

              return FALSE;
            }
          else if (self->dnd_child_index == len || self->is_dnd_at_end)
            {
              bin_child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (self->flowbox), self->dnd_child_index - 1);
              gtk_widget_get_allocation (GTK_WIDGET (bin_child), &alloc);
              x = alloc.x + alloc.width - 2;
            }
          else
            {
              bin_child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (self->flowbox), self->dnd_child_index);
              gtk_widget_get_allocation (GTK_WIDGET (bin_child), &alloc);
              x = alloc.x - 2;
              if (x < 0)
                x = 0;
            }
        }
      else
        {
          alloc.y = 0;
          alloc.height = GSTYLE_COLOR_WIDGET_SWATCH_HEIGHT;
          alloc.x = 2;
        }

      gtk_render_background (style_context, cr, x, alloc.y, 4, alloc.height);
      gtk_render_frame (style_context, cr, x, alloc.y, 4, alloc.height);
    }

  return FALSE;
}

static void
gstyle_palette_widget_finalize (GObject *object)
{
  GstylePaletteWidget *self = (GstylePaletteWidget *)object;

  g_clear_object (&self->dnd_color);
  g_clear_object (&self->placeholder);
  g_clear_object (&self->default_provider);

  bind_palette (self, NULL);
  g_clear_object (&self->palettes);

  G_OBJECT_CLASS (gstyle_palette_widget_parent_class)->finalize (object);
}

static void
gstyle_palette_widget_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GstylePaletteWidget *self = GSTYLE_PALETTE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_DND_LOCK:
      g_value_set_flags (value, gstyle_palette_widget_get_dnd_lock(self));
      break;

    case PROP_PLACEHOLDER:
      g_value_set_object (value, gstyle_palette_widget_get_placeholder (self));
      break;

    case PROP_SELECTED_PALETTE_ID:
      g_value_set_string (value, gstyle_palette_widget_get_selected_palette_id (self));
      break;

    case PROP_VIEW_MODE:
      g_value_set_enum (value, gstyle_palette_widget_get_view_mode (self));
      break;

    case PROP_SORT_MODE:
      g_value_set_enum (value, gstyle_palette_widget_get_sort_mode (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_palette_widget_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GstylePaletteWidget *self = GSTYLE_PALETTE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_DND_LOCK:
      gstyle_palette_widget_set_dnd_lock (self, g_value_get_flags (value));
      break;

    case PROP_PLACEHOLDER:
      gstyle_palette_widget_set_placeholder (self, g_value_get_object (value));
      break;

    case PROP_SELECTED_PALETTE_ID:
      gstyle_palette_widget_set_selected_palette_by_id (self, g_value_get_string (value));
      break;

    case PROP_VIEW_MODE:
      gstyle_palette_widget_set_view_mode (self, g_value_get_enum (value));
      break;

    case PROP_SORT_MODE:
      gstyle_palette_widget_set_sort_mode (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_palette_widget_class_init (GstylePaletteWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gstyle_palette_widget_finalize;
  object_class->get_property = gstyle_palette_widget_get_property;
  object_class->set_property = gstyle_palette_widget_set_property;

  widget_class->drag_motion = gstyle_palette_widget_on_drag_motion;
  widget_class->drag_leave = gstyle_palette_widget_on_drag_leave;
  widget_class->drag_drop = gstyle_palette_widget_on_drag_drop;
  widget_class->drag_data_received = gstyle_palette_widget_on_drag_data_received;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libgstyle/ui/gstyle-palette-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, GstylePaletteWidget, view_stack);
  gtk_widget_class_bind_template_child (widget_class, GstylePaletteWidget, placeholder_box);
  gtk_widget_class_bind_template_child (widget_class, GstylePaletteWidget, listbox);
  gtk_widget_class_bind_template_child (widget_class, GstylePaletteWidget, flowbox);

  properties [PROP_DND_LOCK] =
    g_param_spec_flags ("dnd-lock",
                        "dnd-lock",
                        "Dnd lockability",
                        GSTYLE_TYPE_PALETTE_WIDGET_DND_LOCK_FLAGS,
                        GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_NONE,
                        (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PLACEHOLDER] =
    g_param_spec_object ("placeholder",
                         "placeholder",
                         "placeholder GtkWidget",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SELECTED_PALETTE_ID] =
    g_param_spec_string ("selected-palette-id",
                         "selected-palette-id",
                         "The selected palette id",
                         "",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |G_PARAM_STATIC_STRINGS));

  properties [PROP_VIEW_MODE] =
    g_param_spec_enum ("view-mode",
                       "view-mode",
                       "The view mode of the palettes",
                       GSTYLE_TYPE_PALETTE_WIDGET_VIEW_MODE,
                       GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SORT_MODE] =
    g_param_spec_enum ("sort-mode",
                       "sort-mode",
                       "The sort mode of the palettes",
                       GSTYLE_TYPE_PALETTE_WIDGET_SORT_MODE,
                       GSTYLE_PALETTE_WIDGET_SORT_MODE_ORIGINAL,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * GstylePaletteWidget::activated:
   * @self: a #GstylePaletteWidget
   * @palette: a #GstylePalette
   * @position: a position in the #palette
   *
   * This signal is emitted when you select a color in the palette widget.
   */
  signals [ACTIVATED] = g_signal_new ("activated",
                                      G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL, NULL,
                                      G_TYPE_NONE,
                                      2,
                                      GSTYLE_TYPE_PALETTE,
                                      G_TYPE_INT);

  /**
   * GstylePaletteWidget::palette-added:
   * @self: a #GstylePaletteWidget
   * @palette: a #GstylePalette
   *
   * This signal is emitted when a palette is added to the palette widget.
   */
  signals [PALETTE_ADDED] = g_signal_new ("palette-added",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          1,
                                          GSTYLE_TYPE_PALETTE);

  /**
   * GstylePaletteWidget::palette-removed:
   * @self: a #GstylePaletteWidget
   * @palette: a #GstylePalette
   *
   * This signal is emitted when a palette is removed to the palette widget.
   */
  signals [PALETTE_REMOVED] = g_signal_new ("palette-removed",
                                            G_TYPE_FROM_CLASS (klass),
                                            G_SIGNAL_RUN_LAST,
                                            0,
                                            NULL, NULL, NULL,
                                            G_TYPE_NONE,
                                            1,
                                            GSTYLE_TYPE_PALETTE);

  gtk_widget_class_set_css_name (widget_class, "gstylepalettewidget");
}

static void
gstyle_palette_widget_init (GstylePaletteWidget *self)
{
  GtkStyleContext *context;

  static const GtkTargetEntry dnd_targets [] = {
    { (gchar *)"GSTYLE_COLOR_WIDGET", GTK_TARGET_SAME_APP, 0 },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  self->view_mode = GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST;
  gtk_stack_set_visible_child_name (self->view_stack, "list");

  self->palettes = g_list_store_new (GSTYLE_TYPE_PALETTE);

  gstyle_palette_widget_add_actions (self);

  g_signal_connect_swapped (self->listbox,
                            "row-activated",
                            G_CALLBACK (gstyle_palette_widget_color_row_activated),
                            self);

  g_signal_connect_after (self->listbox,
                          "draw",
                          G_CALLBACK (listbox_draw_cb),
                          self);

  g_signal_connect_swapped (self->flowbox,
                            "child-activated",
                            G_CALLBACK (gstyle_palette_widget_color_swatch_activated),
                            self);

  g_signal_connect_after (self->flowbox,
                          "draw",
                          G_CALLBACK (flowbox_draw_cb),
                          self);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  self->default_provider = gstyle_css_provider_init_default (gtk_style_context_get_screen (context));

  gtk_drag_dest_set (GTK_WIDGET (self), 0,
                     dnd_targets, G_N_ELEMENTS (dnd_targets),
                     GDK_ACTION_COPY);

  gtk_drag_dest_set_track_motion (GTK_WIDGET (self), TRUE);

  self->dnd_color = gstyle_color_new ("placeholder", GSTYLE_COLOR_KIND_RGBA, 210, 210, 210, 100);
  self->dnd_child_index = -1;
}

GType
gstyle_palette_widget_view_mode_get_type (void)
{
  static GType view_mode_type_id;
  static const GEnumValue values[] = {
    { GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST,    "GSTYLE_PALETTE_WIDGET__VIEW_MODE_LIST",    "list" },
    { GSTYLE_PALETTE_WIDGET_VIEW_MODE_SWATCHS, "GSTYLE_PALETTE_WIDGET__VIEW_MODE_SWATCHS", "swatchs" },
    { 0 }
  };

  if (g_once_init_enter (&view_mode_type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstylePaletteWidgetViewMode", values);
      g_once_init_leave (&view_mode_type_id, _type_id);
    }

  return view_mode_type_id;
}

GType
gstyle_palette_widget_sort_mode_get_type (void)
{
  static GType sort_mode_type_id;
  static const GEnumValue values[] = {
    { GSTYLE_PALETTE_WIDGET_SORT_MODE_ORIGINAL,   "GSTYLE_PALETTE_WIDGET_SORT_MODE_ORIGINAL",   "original" },
    { GSTYLE_PALETTE_WIDGET_SORT_MODE_LIGHT,      "GSTYLE_PALETTE_WIDGET_SORT_MODE_LIGHT",      "light" },
    { GSTYLE_PALETTE_WIDGET_SORT_MODE_APPROCHING, "GSTYLE_PALETTE_WIDGET_SORT_MODE_APPROCHING", "approching" },
    { 0 }
  };

  if (g_once_init_enter (&sort_mode_type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstylePaletteWidgetSortMode", values);
      g_once_init_leave (&sort_mode_type_id, _type_id);
    }

  return sort_mode_type_id;
}

GType
gstyle_palette_widget_dnd_lock_flags_get_type (void)
{
  static GType type_id;
  static const GFlagsValue values[] = {
    { GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_NONE,  "GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_NONE",  "none" },
    { GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DRAG,  "GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DRAG",  "drag" },
    { GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DROP,  "GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DROP",  "drop" },
    { GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_ALL,   "GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_ALL",   "all" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_flags_register_static ("GstylePaletteWidgetDndLockFlags", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
