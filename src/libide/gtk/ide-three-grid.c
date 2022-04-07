/* ide-three-grid.c
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

#define G_LOG_DOMAIN "ide-three-grid"

#include "config.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include "ide-gtk-enums.h"
#include "ide-three-grid.h"

struct _IdeThreeGridChild
{
  GtkLayoutChild     parent_instance;
  IdeThreeGridColumn column;
  int                row;
  int                min_height;
  int                nat_height;
  int                min_baseline;
  int                nat_baseline;
};

#define IDE_TYPE_THREE_GRID_CHILD (ide_three_grid_child_get_type())
G_DECLARE_FINAL_TYPE (IdeThreeGridChild, ide_three_grid_child, IDE, THREE_GRID_CHILD, GtkLayoutChild)
G_DEFINE_FINAL_TYPE (IdeThreeGridChild, ide_three_grid_child, GTK_TYPE_LAYOUT_CHILD)

enum {
  CHILD_PROP_0,
  CHILD_PROP_COLUMN,
  CHILD_PROP_ROW,
  LAST_CHILD_PROP
};

static GParamSpec *child_properties [LAST_CHILD_PROP];

static void
ide_three_grid_child_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeThreeGridChild *self = IDE_THREE_GRID_CHILD (object);

  switch (prop_id)
    {
    case CHILD_PROP_ROW:
      g_value_set_uint (value, self->row);
      break;

    case CHILD_PROP_COLUMN:
      g_value_set_enum (value, self->column);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_three_grid_child_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeThreeGridChild *self = IDE_THREE_GRID_CHILD (object);

  switch (prop_id)
    {
    case CHILD_PROP_ROW:
      self->row = g_value_get_uint (value);
      break;

    case CHILD_PROP_COLUMN:
      self->column = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_three_grid_child_class_init (IdeThreeGridChildClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_three_grid_child_get_property;
  object_class->set_property = ide_three_grid_child_set_property;

  child_properties [CHILD_PROP_COLUMN] =
    g_param_spec_enum ("column",
                       "Column",
                       "The column for the child",
                       IDE_TYPE_THREE_GRID_COLUMN,
                       IDE_THREE_GRID_COLUMN_LEFT,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  child_properties [CHILD_PROP_ROW] =
    g_param_spec_uint ("row",
                       "Row",
                       "The row for the child",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_CHILD_PROP, child_properties);
}

static void
ide_three_grid_child_init (IdeThreeGridChild *self)
{
}

typedef struct
{
  int row;
  int min_above_baseline;
  int min_below_baseline;
  int nat_above_baseline;
  int nat_below_baseline;
} IdeThreeGridRowInfo;

struct _IdeThreeGridLayout
{
  GtkLayoutManager parent_instance;
  GHashTable *row_infos;
  int row_spacing;
  int column_spacing;
};

#define IDE_TYPE_THREE_GRID_LAYOUT (ide_three_grid_layout_get_type())
G_DECLARE_FINAL_TYPE (IdeThreeGridLayout, ide_three_grid_layout, IDE, THREE_GRID_LAYOUT, GtkLayoutManager)
G_DEFINE_FINAL_TYPE (IdeThreeGridLayout, ide_three_grid_layout, GTK_TYPE_LAYOUT_MANAGER)

static GtkSizeRequestMode
ide_three_grid_layout_get_request_mode (GtkLayoutManager *manager,
                                        GtkWidget        *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
get_column_width (IdeThreeGridLayout *self,
                  GtkWidget          *widget,
                  IdeThreeGridColumn  column,
                  int                *min_width,
                  int                *nat_width)
{
  int real_min_width = 0;
  int real_nat_width = 0;

  g_assert (IDE_IS_THREE_GRID_LAYOUT (self));
  g_assert (column >= IDE_THREE_GRID_COLUMN_LEFT);
  g_assert (column <= IDE_THREE_GRID_COLUMN_RIGHT);
  g_assert (min_width != NULL);
  g_assert (nat_width != NULL);

  for (GtkWidget *iter = gtk_widget_get_first_child (widget);
       iter;
       iter = gtk_widget_get_next_sibling (iter))
    {
      IdeThreeGridChild *child = IDE_THREE_GRID_CHILD (gtk_layout_manager_get_layout_child (GTK_LAYOUT_MANAGER (self), iter));

      if (child->column == column)
        {
          int child_min_width;
          int child_nat_width;

          gtk_widget_measure (iter, GTK_ORIENTATION_HORIZONTAL, 0, &child_min_width, &child_nat_width, NULL, NULL);

          real_min_width = MAX (real_min_width, child_min_width);
          real_nat_width = MAX (real_nat_width, child_nat_width);
        }
    }

  *min_width = real_min_width;
  *nat_width = real_nat_width;
}

static void
get_preferred_width (IdeThreeGridLayout *self,
                     GtkWidget          *widget,
                     int                *min_width,
                     int                *nat_width)
{
  int min_widths[3];
  int nat_widths[3];

  g_assert (IDE_IS_THREE_GRID_LAYOUT (self));
  g_assert (min_width != NULL);
  g_assert (nat_width != NULL);

  for (guint i = 0; i < 3; i++)
    get_column_width (self, widget, i, &min_widths[i], &nat_widths[i]);

  *min_width = MAX (min_widths[0], min_widths[2]) * 2 + min_widths[1] + (self->column_spacing * 2);
  *nat_width = MAX (nat_widths[0], nat_widths[2]) * 2 + nat_widths[1] + (self->column_spacing * 2);
}

static void
row_info_merge (IdeThreeGridRowInfo       *row_info,
                const IdeThreeGridRowInfo *other)
{
  g_assert (row_info);
  g_assert (other);

  row_info->min_above_baseline = MAX (row_info->min_above_baseline, other->min_above_baseline);
  row_info->min_below_baseline = MAX (row_info->min_below_baseline, other->min_below_baseline);
  row_info->nat_above_baseline = MAX (row_info->nat_above_baseline, other->nat_above_baseline);
  row_info->nat_below_baseline = MAX (row_info->nat_below_baseline, other->nat_below_baseline);
}

static void
update_row_info (GHashTable        *hashtable,
                 IdeThreeGridChild *child)
{
  GtkBaselinePosition baseline_position = GTK_BASELINE_POSITION_CENTER;
  IdeThreeGridRowInfo *row_info;
  IdeThreeGridRowInfo current = { 0 };

  g_assert (hashtable);
  g_assert (child);

  row_info = g_hash_table_lookup (hashtable, GINT_TO_POINTER (child->row));

  if (row_info == NULL)
    {
      row_info = g_new0 (IdeThreeGridRowInfo, 1);
      row_info->row = child->row;
      g_hash_table_insert (hashtable, GINT_TO_POINTER (child->row), row_info);
    }

  /*
   * TODO:
   *
   * Allow setting baseline position per row. Right now we only support center
   * because that is the easiest thing to start with.
   */

  if (child->min_baseline == -1)
    {
      if (baseline_position == GTK_BASELINE_POSITION_CENTER)
        {
          current.min_above_baseline = current.min_below_baseline = ceil (child->min_height / 2.0);
          current.nat_above_baseline = current.nat_below_baseline = ceil (child->min_height / 2.0);
        }
      else if (baseline_position == GTK_BASELINE_POSITION_TOP)
        {
          g_assert_not_reached ();
        }
      else if (baseline_position == GTK_BASELINE_POSITION_BOTTOM)
        {
          g_assert_not_reached ();
        }
    }
  else
    {
      current.min_above_baseline = child->min_baseline;
      current.min_below_baseline = child->min_height - child->min_baseline;
      current.nat_above_baseline = child->nat_baseline;
      current.nat_below_baseline = child->nat_height - child->nat_baseline;
    }

  row_info_merge (row_info, &current);
}

static void
get_preferred_height_for_width (IdeThreeGridLayout *self,
                                GtkWidget          *widget,
                                int                 width,
                                int                *min_height,
                                int                *nat_height)
{
  g_autoptr(GHashTable) row_infos = NULL;
  IdeThreeGridRowInfo *row_info;
  GHashTableIter hiter;
  int real_min_height = 0;
  int real_nat_height = 0;
  int column_min_widths[3];
  int column_nat_widths[3];
  int widths[3];
  int n_rows;

  g_assert (IDE_IS_THREE_GRID_LAYOUT (self));
  g_assert (min_height != NULL);
  g_assert (nat_height != NULL);

  width -= self->column_spacing * 2;

  get_column_width (self, widget, IDE_THREE_GRID_COLUMN_LEFT, &column_min_widths[0], &column_nat_widths[0]);
  get_column_width (self, widget, IDE_THREE_GRID_COLUMN_CENTER, &column_min_widths[1], &column_nat_widths[1]);
  get_column_width (self, widget, IDE_THREE_GRID_COLUMN_RIGHT, &column_min_widths[2], &column_nat_widths[2]);

  if ((MAX (column_min_widths[0], column_min_widths[2]) * 2 + column_nat_widths[1]) > width)
    {
      widths[0] = column_min_widths[0];
      widths[2] = column_min_widths[2];
      widths[1] = width - widths[0] - widths[2];
    }
  else
    {
      /* Handle #1 and #2 */
      widths[1] = column_nat_widths[1];
      widths[0] = (width - widths[1]) / 2;
      widths[2] = width - widths[1] - widths[0];
    }

  row_infos = g_hash_table_new_full (NULL, NULL, NULL, g_free);

  for (GtkWidget *iter = gtk_widget_get_first_child (widget);
       iter;
       iter = gtk_widget_get_next_sibling (iter))
    {
      IdeThreeGridChild *child = IDE_THREE_GRID_CHILD (gtk_layout_manager_get_layout_child (GTK_LAYOUT_MANAGER (self), iter));

      if (!gtk_widget_get_visible (iter) || !gtk_widget_get_child_visible (iter))
        continue;

      gtk_widget_measure (iter, GTK_ORIENTATION_VERTICAL, MAX (0, widths[child->column]),
                          &child->min_height, &child->nat_height,
                          &child->min_baseline, &child->nat_baseline);

      update_row_info (row_infos, child);
    }

  g_hash_table_iter_init (&hiter, row_infos);
  while (g_hash_table_iter_next (&hiter, NULL, (gpointer *)&row_info))
    {
      real_min_height += row_info->min_above_baseline + row_info->min_below_baseline;
      real_nat_height += row_info->nat_above_baseline + row_info->nat_below_baseline;
    }

  n_rows = g_hash_table_size (row_infos);

  if (n_rows > 1)
    {
      real_min_height += (n_rows - 1) * self->row_spacing;
      real_nat_height += (n_rows - 1) * self->row_spacing;
    }

  *min_height = real_min_height;
  *nat_height = real_nat_height;

  g_clear_pointer (&self->row_infos, g_hash_table_unref);
  self->row_infos = g_steal_pointer (&row_infos);
}

static int
sort_by_row (gconstpointer a,
             gconstpointer b)
{
  const IdeThreeGridRowInfo *info_a = a;
  const IdeThreeGridRowInfo *info_b = b;

  return info_a->row - info_b->row;
}

static void
size_allocate_children (IdeThreeGridLayout *self,
                        GtkWidget          *widget,
                        IdeThreeGridColumn  column,
                        int                 row,
                        GtkAllocation      *allocation,
                        int                 baseline)
{
  g_assert (IDE_IS_THREE_GRID_LAYOUT (self));
  g_assert (allocation != NULL);

  for (GtkWidget *iter = gtk_widget_get_first_child (widget);
       iter;
       iter = gtk_widget_get_next_sibling (iter))
    {
      IdeThreeGridChild *child = IDE_THREE_GRID_CHILD (gtk_layout_manager_get_layout_child (GTK_LAYOUT_MANAGER (self), iter));

      if (child->row == row && child->column == column)
        {
          GtkAllocation copy = *allocation;
          gtk_widget_size_allocate (iter, &copy, baseline);
        }
    }
}

static void
ide_three_grid_layout_allocate (GtkLayoutManager *manager,
                                GtkWidget        *widget,
                                int               width,
                                int               height,
                                int               baseline)
{
  IdeThreeGridLayout *self = (IdeThreeGridLayout *)manager;
  g_autofree GtkRequestedSize *rows = NULL;
  const GList *iter;
  GtkAllocation area;
  GtkTextDirection dir;
  GList *values;
  guint i;
  guint n_rows;
  int min_height;
  int nat_height;
  int left_min_width;
  int left_nat_width;
  int center_min_width;
  int center_nat_width;
  int right_min_width;
  int right_nat_width;
  int left;
  int center;
  int right;

  g_assert (IDE_IS_THREE_GRID_LAYOUT (self));

  area.x = 0;
  area.y = 0;
  area.width = width;
  area.height = height;

  dir = gtk_widget_get_direction (widget);

  get_preferred_height_for_width (self, widget, width, &min_height, &nat_height);

  if (min_height > height)
    g_warning ("%s requested a minimum height of %d and got %d",
               G_OBJECT_TYPE_NAME (widget), min_height, height);

  if (self->row_infos == NULL)
    return;

  values = g_hash_table_get_values (self->row_infos);
  values = g_list_sort (values, sort_by_row);

  get_column_width (self, widget, IDE_THREE_GRID_COLUMN_LEFT, &left_min_width, &left_nat_width);
  get_column_width (self, widget, IDE_THREE_GRID_COLUMN_CENTER, &center_min_width, &center_nat_width);
  get_column_width (self, widget, IDE_THREE_GRID_COLUMN_RIGHT, &right_min_width, &right_nat_width);

  /*
   * Determine how much to give to the center widget first. This is because we will
   * just give the rest of the space on the sides to left/right columns and they
   * can deal with alignment by using halign.
   *
   * We can be in one of a couple states:
   *
   * 1) There is enough room for all columns natural size.
   *    (We allocate the same to the left and the right).
   * 2) There is enough for the natural size of the center
   *    but for some amount between natural and min sizing
   *    of the left/right columns.
   * 3) There is only minimum size for columns and some
   *    amount between natural/minimum of the center.
   *
   * We can handle #1 and #2 with the same logic though.
   */

  if ((MAX (left_min_width, right_min_width) * 2 + center_nat_width + 2 * self->column_spacing) > area.width)
    {
      /* Handle #3 */
      left = right = MAX (left_min_width, right_min_width);
      center = area.width - left - right - 2 * self->column_spacing;
    }
  else
    {
      /* Handle #1 and #2 */
      center = center_nat_width;
      right = left = (area.width - center) / 2 - self->column_spacing;
    }

  n_rows = g_list_length (values);
  rows = g_new0 (GtkRequestedSize, n_rows);

  for (iter = values, i = 0; iter != NULL; iter = iter->next, i++)
    {
      IdeThreeGridRowInfo *row_info = iter->data;

      rows[i].data = row_info;
      rows[i].minimum_size = row_info->min_above_baseline + row_info->min_below_baseline;
      rows[i].natural_size = row_info->nat_above_baseline + row_info->nat_below_baseline;
    }

  gtk_distribute_natural_allocation (area.height, n_rows, rows);

  for (i = 0; i < n_rows; i++)
    {
      GtkRequestedSize *size = &rows[i];
      IdeThreeGridRowInfo *row_info = size->data;
      GtkAllocation child_alloc;
      int child_baseline;

      if (row_info->nat_above_baseline + row_info->nat_below_baseline < size->minimum_size)
        child_baseline = row_info->nat_above_baseline;
      else
        child_baseline = row_info->min_above_baseline;

      child_alloc.x = area.x;
      child_alloc.width = left;
      child_alloc.y = area.y;
      child_alloc.height = size->minimum_size;

      if (dir == GTK_TEXT_DIR_LTR)
        size_allocate_children (self, widget, IDE_THREE_GRID_COLUMN_LEFT, row_info->row, &child_alloc, child_baseline);
      else
        size_allocate_children (self, widget, IDE_THREE_GRID_COLUMN_RIGHT, row_info->row, &child_alloc, child_baseline);

      child_alloc.x = area.x + left + self->column_spacing;
      child_alloc.width = center;
      child_alloc.y = area.y;
      child_alloc.height = size->minimum_size;

      size_allocate_children (self, widget, IDE_THREE_GRID_COLUMN_CENTER, row_info->row, &child_alloc, child_baseline);

      child_alloc.x = area.x + area.width - right;
      child_alloc.width = right;
      child_alloc.y = area.y;
      child_alloc.height = size->minimum_size;

      if (dir == GTK_TEXT_DIR_LTR)
        size_allocate_children (self, widget, IDE_THREE_GRID_COLUMN_RIGHT, row_info->row, &child_alloc, child_baseline);
      else
        size_allocate_children (self, widget, IDE_THREE_GRID_COLUMN_LEFT, row_info->row, &child_alloc, child_baseline);

      area.y += child_alloc.height + self->row_spacing;
      area.height -= child_alloc.height + self->row_spacing;
    }

  g_list_free (values);
}

static void
ide_three_grid_layout_measure (GtkLayoutManager *manager,
                               GtkWidget        *widget,
                               GtkOrientation    orientation,
                               int               for_size,
                               int              *minimum,
                               int              *natural,
                               int              *minimum_baseline,
                               int              *natural_baseline)
{
  IdeThreeGridLayout *self = (IdeThreeGridLayout *)manager;

  g_assert (IDE_IS_THREE_GRID_LAYOUT (self));

  *minimum_baseline = -1;
  *natural_baseline = -1;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    get_preferred_width (self, widget, minimum, natural);
  else
    get_preferred_height_for_width (self, widget, for_size, minimum, natural);
}

static void
ide_three_grid_layout_dispose (GObject *object)
{
  IdeThreeGridLayout *self = (IdeThreeGridLayout *)object;

  g_clear_pointer (&self->row_infos, g_hash_table_unref);

  G_OBJECT_CLASS (ide_three_grid_layout_parent_class)->dispose (object);
}

static void
ide_three_grid_layout_class_init (IdeThreeGridLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  object_class->dispose = ide_three_grid_layout_dispose;

  layout_class->get_request_mode = ide_three_grid_layout_get_request_mode;
  layout_class->measure = ide_three_grid_layout_measure;
  layout_class->allocate = ide_three_grid_layout_allocate;
  layout_class->layout_child_type = IDE_TYPE_THREE_GRID_CHILD;
}

static void
ide_three_grid_layout_init (IdeThreeGridLayout *self)
{
}

struct _IdeThreeGrid
{
  GtkWidget parent_instance;
};

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeThreeGrid, ide_three_grid, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

enum {
  PROP_0,
  PROP_COLUMN_SPACING,
  PROP_ROW_SPACING,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

void
ide_three_grid_add (IdeThreeGrid       *self,
                    GtkWidget          *widget,
                    guint               row,
                    IdeThreeGridColumn  column)
{
  g_assert (IDE_IS_THREE_GRID (self));
  g_assert (GTK_IS_WIDGET (widget));

  gtk_widget_set_parent (widget, GTK_WIDGET (self));
}

void
ide_three_grid_remove (IdeThreeGrid *self,
                       GtkWidget    *widget)
{
  g_assert (IDE_IS_THREE_GRID (self));
  g_assert (GTK_IS_WIDGET (widget));

  gtk_widget_unparent (widget);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
ide_three_grid_dispose (GObject *object)
{
  IdeThreeGrid *self = (IdeThreeGrid *)object;
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    ide_three_grid_remove (self, child);

  G_OBJECT_CLASS (ide_three_grid_parent_class)->dispose (object);
}

static void
ide_three_grid_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeThreeGrid *self = IDE_THREE_GRID (object);
  GtkLayoutManager *manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      g_value_set_uint (value, IDE_THREE_GRID_LAYOUT (manager)->column_spacing);
      break;

    case PROP_ROW_SPACING:
      g_value_set_uint (value, IDE_THREE_GRID_LAYOUT (manager)->row_spacing);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_three_grid_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeThreeGrid *self = IDE_THREE_GRID (object);
  GtkLayoutManager *manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      IDE_THREE_GRID_LAYOUT (manager)->column_spacing = g_value_get_uint (value);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    case PROP_ROW_SPACING:
      IDE_THREE_GRID_LAYOUT (manager)->row_spacing = g_value_get_uint (value);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_three_grid_class_init (IdeThreeGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_three_grid_dispose;
  object_class->get_property = ide_three_grid_get_property;
  object_class->set_property = ide_three_grid_set_property;

  properties [PROP_COLUMN_SPACING] =
    g_param_spec_uint ("column-spacing",
                       "Column Spacing",
                       "The amount of spacing to add between columns",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ROW_SPACING] =
    g_param_spec_uint ("row-spacing",
                       "Row Spacing",
                       "The amount of spacing to add between rows",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "threegrid");
  gtk_widget_class_set_layout_manager_type (widget_class, IDE_TYPE_THREE_GRID_LAYOUT);
}

static void
ide_three_grid_init (IdeThreeGrid *self)
{
}

GtkWidget *
ide_three_grid_new (void)
{
  return g_object_new (IDE_TYPE_THREE_GRID, NULL);
}

static void
ide_three_grid_add_child (GtkBuildable *buildable,
                          GtkBuilder   *builder,
                          GObject      *child,
                          const char   *type)
{
  if (GTK_IS_WIDGET (child))
    ide_three_grid_add (IDE_THREE_GRID (buildable), GTK_WIDGET (child), 0, IDE_THREE_GRID_COLUMN_LEFT);
  else
    g_warning ("%s cannot be added to %s", G_OBJECT_TYPE_NAME (child), G_OBJECT_TYPE_NAME (buildable));
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = ide_three_grid_add_child;
}
