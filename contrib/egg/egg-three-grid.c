/* egg-three-grid.c
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

#define G_LOG_DOMAIN "egg-three-grid"

#include <math.h>

#include "egg-three-grid.h"

typedef struct
{
  GtkWidget *widget;
  gint       column;
  gint       row;
  gint       min_height;
  gint       nat_height;
  gint       min_baseline;
  gint       nat_baseline;
} EggThreeGridChild;

typedef struct
{
  GPtrArray *children;
  GHashTable *row_infos;
  guint column_spacing;
  guint row_spacing;
} EggThreeGridPrivate;

typedef struct
{
  gint row;
  gint min_above_baseline;
  gint min_below_baseline;
  gint nat_above_baseline;
  gint nat_below_baseline;
} EggThreeGridRowInfo;

G_DEFINE_TYPE_WITH_PRIVATE (EggThreeGrid, egg_three_grid, GTK_TYPE_CONTAINER)

enum {
  PROP_0,
  PROP_COLUMN_SPACING,
  PROP_ROW_SPACING,
  N_PROPS
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_ROW,
  CHILD_PROP_COLUMN,
  N_CHILD_PROPS
};

static GParamSpec *properties [N_PROPS];
static GParamSpec *child_properties [N_CHILD_PROPS];
static EggThreeGridChild dummy;

static EggThreeGridChild *
egg_three_grid_child_new (void)
{
  return g_slice_new0 (EggThreeGridChild);
}

static void
egg_three_grid_child_free (gpointer data)
{
  EggThreeGridChild *child = data;

  g_clear_object (&child->widget);
  g_slice_free (EggThreeGridChild, child);
}

static EggThreeGridChild *
egg_three_grid_find_child (EggThreeGrid *self,
                           GtkWidget    *widget)
{
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);

  g_assert (EGG_IS_THREE_GRID (self));
  g_assert (GTK_IS_WIDGET (widget));

  for (guint i = 0; i < priv->children->len; i++)
    {
      EggThreeGridChild *child = g_ptr_array_index (priv->children, i);

      if (child->widget == widget)
        return child;
    }

  return &dummy;
}

static void
egg_three_grid_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  EggThreeGrid *self = (EggThreeGrid *)container;
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);
  EggThreeGridChild *child;

  g_assert (EGG_IS_THREE_GRID (self));
  g_assert (GTK_IS_WIDGET (widget));

  child = egg_three_grid_child_new ();
  child->widget = g_object_ref_sink (widget);
  g_ptr_array_add (priv->children, child);

  gtk_widget_set_parent (widget, GTK_WIDGET (self));
}

static void
egg_three_grid_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  EggThreeGrid *self = (EggThreeGrid *)container;
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);

  g_assert (EGG_IS_THREE_GRID (self));
  g_assert (GTK_IS_WIDGET (widget));

  for (guint i = 0; i < priv->children->len; i++)
    {
      EggThreeGridChild *child = g_ptr_array_index (priv->children, i);

      if (child->widget == widget)
        {
          gtk_widget_unparent (child->widget);
          g_ptr_array_remove_index (priv->children, i);
          gtk_widget_queue_resize (GTK_WIDGET (self));
          return;
        }
    }
}

static GtkSizeRequestMode
egg_three_grid_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
egg_three_grid_get_column_width (EggThreeGrid       *self,
                                 EggThreeGridColumn  column,
                                 gint               *min_width,
                                 gint               *nat_width)
{
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);
  gint real_min_width = 0;
  gint real_nat_width = 0;

  g_assert (EGG_IS_THREE_GRID (self));
  g_assert (column >= EGG_THREE_GRID_COLUMN_LEFT);
  g_assert (column <= EGG_THREE_GRID_COLUMN_RIGHT);
  g_assert (min_width != NULL);
  g_assert (nat_width != NULL);

  for (guint i = 0; i < priv->children->len; i++)
    {
      EggThreeGridChild *child = g_ptr_array_index (priv->children, i);

      if (child->column == column)
        {
          gint child_min_width;
          gint child_nat_width;

          gtk_widget_get_preferred_width (child->widget, &child_min_width, &child_nat_width);

          real_min_width = MAX (real_min_width, child_min_width);
          real_nat_width = MAX (real_nat_width, child_nat_width);
        }
    }

  *min_width = real_min_width;
  *nat_width = real_nat_width;
}

static void
egg_three_grid_get_preferred_width (GtkWidget *widget,
                                    gint      *min_width,
                                    gint      *nat_width)
{
  EggThreeGrid *self = (EggThreeGrid *)widget;
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);
  gint total_min_width = 0;
  gint total_nat_width = 0;
  gint min_widths[3];
  gint nat_widths[3];
  gint border_width;

  g_assert (EGG_IS_THREE_GRID (self));
  g_assert (min_width != NULL);
  g_assert (nat_width != NULL);

  for (guint i = 0; i < 3; i++)
    egg_three_grid_get_column_width (self, i, &min_widths[i], &nat_widths[i]);

  total_min_width = MAX (min_widths[0], min_widths[2]) * 2 + min_widths[1];
  total_nat_width = MAX (nat_widths[0], nat_widths[2]) * 2 + nat_widths[1];

  border_width = gtk_container_get_border_width (GTK_CONTAINER (self));

  *min_width = total_min_width + (border_width * 2) + (priv->column_spacing * 2);
  *nat_width = total_nat_width + (border_width * 2) + (priv->column_spacing * 2);
}

static void
row_info_merge (EggThreeGridRowInfo       *row_info,
                const EggThreeGridRowInfo *other)
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
                 EggThreeGridChild *child)
{
  GtkBaselinePosition baseline_position = GTK_BASELINE_POSITION_CENTER;
  EggThreeGridRowInfo *row_info;
  EggThreeGridRowInfo current = { 0 };

  g_assert (hashtable);
  g_assert (child);

  row_info = g_hash_table_lookup (hashtable, GINT_TO_POINTER (child->row));

  if (row_info == NULL)
    {
      row_info = g_new0 (EggThreeGridRowInfo, 1);
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
egg_three_grid_get_preferred_height_for_width (GtkWidget *widget,
                                               gint       width,
                                               gint      *min_height,
                                               gint      *nat_height)
{
  EggThreeGrid *self = (EggThreeGrid *)widget;
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);
  g_autoptr(GHashTable) row_infos = NULL;
  EggThreeGridRowInfo *row_info;
  GHashTableIter iter;
  gint real_min_height = 0;
  gint real_nat_height = 0;
  gint column_min_widths[3];
  gint column_nat_widths[3];
  gint widths[3];
  gint border_width;
  gint n_rows;

  g_assert (EGG_IS_THREE_GRID (self));
  g_assert (min_height != NULL);
  g_assert (nat_height != NULL);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (self));
  width -= border_width * 2;
  width -= priv->column_spacing * 2;

  egg_three_grid_get_column_width (self, EGG_THREE_GRID_COLUMN_LEFT, &column_min_widths[0], &column_nat_widths[0]);
  egg_three_grid_get_column_width (self, EGG_THREE_GRID_COLUMN_CENTER, &column_min_widths[1], &column_nat_widths[1]);
  egg_three_grid_get_column_width (self, EGG_THREE_GRID_COLUMN_RIGHT, &column_min_widths[2], &column_nat_widths[2]);

  if ((MAX (column_min_widths[0], column_min_widths[2]) * 2 + column_nat_widths[1]) >= width)
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

  for (guint i = 0; i < priv->children->len; i++)
    {
      EggThreeGridChild *child = g_ptr_array_index (priv->children, i);

      if (!gtk_widget_get_visible (child->widget) ||
          !gtk_widget_get_child_visible (child->widget))
        continue;

      gtk_widget_get_preferred_height_and_baseline_for_width (child->widget,
                                                              widths[child->column],
                                                              &child->min_height,
                                                              &child->nat_height,
                                                              &child->min_baseline,
                                                              &child->nat_baseline);
      update_row_info (row_infos, child);
    }

  g_hash_table_iter_init (&iter, row_infos);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&row_info))
    {
#if 0
      g_print ("Row %d: MIN  Above %d  Below %d\n",
               row_info->row,
               row_info->min_above_baseline, row_info->min_below_baseline);
      g_print ("Row %d: NAT  Above %d  Below %d\n",
               row_info->row,
               row_info->nat_above_baseline, row_info->nat_below_baseline);
#endif
      real_min_height += row_info->min_above_baseline + row_info->min_below_baseline;
      real_nat_height += row_info->nat_above_baseline + row_info->nat_below_baseline;
    }

  real_min_height += border_width * 2;
  real_nat_height += border_width * 2;

  n_rows = g_hash_table_size (row_infos);

  if (n_rows > 1)
    {
      real_min_height += (n_rows - 1) * priv->row_spacing;
      real_nat_height += (n_rows - 1) * priv->row_spacing;
    }

  *min_height = real_min_height;
  *nat_height = real_nat_height;

#if 0
  g_print ("%d children in %d rows: %dx%d\n",
           priv->children->len,
           g_hash_table_size (row_infos),
           real_min_height, real_nat_height);
#endif

  g_clear_pointer (&priv->row_infos, g_hash_table_unref);
  priv->row_infos = g_steal_pointer (&row_infos);
}

static gint
sort_by_row (gconstpointer a,
             gconstpointer b)
{
  const EggThreeGridRowInfo *info_a = a;
  const EggThreeGridRowInfo *info_b = b;

  return info_a->row - info_b->row;
}

static void
egg_three_grid_size_allocate_children (EggThreeGrid       *self,
                                       EggThreeGridColumn  column,
                                       gint                row,
                                       GtkAllocation      *allocation,
                                       gint                baseline)
{
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);

  g_assert (EGG_IS_THREE_GRID (self));
  g_assert (allocation != NULL);

  for (guint i = 0; i < priv->children->len; i++)
    {
      EggThreeGridChild *child = g_ptr_array_index (priv->children, i);

      if (child->row == row && child->column == column)
        {
          GtkAllocation copy = *allocation;
          gtk_widget_size_allocate_with_baseline (child->widget, &copy, baseline);
        }
    }
}

static void
egg_three_grid_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  EggThreeGrid *self = (EggThreeGrid *)widget;
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);
  g_autofree GtkRequestedSize *rows = NULL;
  const GList *iter;
  GtkAllocation area;
  GtkTextDirection dir;
  GList *values;
  guint i;
  guint n_rows;
  gint min_height;
  gint nat_height;
  gint border_width;
  gint left_min_width;
  gint left_nat_width;
  gint center_min_width;
  gint center_nat_width;
  gint right_min_width;
  gint right_nat_width;
  gint left;
  gint center;
  gint right;

  g_assert (EGG_IS_THREE_GRID (self));
  g_assert (allocation != NULL);

  dir = gtk_widget_get_direction (widget);

  gtk_widget_set_allocation (widget, allocation);

  egg_three_grid_get_preferred_height_for_width (widget, allocation->width, &min_height, &nat_height);

  if (min_height > allocation->height)
    g_warning ("%s requested a minimum height of %d and got %d",
               G_OBJECT_TYPE_NAME (widget), min_height, allocation->height);

  if (priv->row_infos == NULL)
    return;

  values = g_hash_table_get_values (priv->row_infos);
  values = g_list_sort (values, sort_by_row);

  area = *allocation;
  border_width = gtk_container_get_border_width (GTK_CONTAINER (self));

  area.x += border_width;
  area.y += border_width;
  area.width -= border_width * 2;
  area.height -= border_width * 2;

  egg_three_grid_get_column_width (self, EGG_THREE_GRID_COLUMN_LEFT, &left_min_width, &left_nat_width);
  egg_three_grid_get_column_width (self, EGG_THREE_GRID_COLUMN_CENTER, &center_min_width, &center_nat_width);
  egg_three_grid_get_column_width (self, EGG_THREE_GRID_COLUMN_RIGHT, &right_min_width, &right_nat_width);

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

  if ((MAX (left_min_width, right_min_width) * 2 + center_nat_width) >= (area.width - (priv->column_spacing * 2)))
    {
      /* Handle #3 */
      left = left_min_width;
      right = right_min_width;
      center = area.width - left - right;
    }
  else
    {
      /* Handle #1 and #2 */
      center = center_nat_width;
      right = left = (area.width - center) / 2 - priv->column_spacing;
    }

  n_rows = g_list_length (values);
  rows = g_new0 (GtkRequestedSize, n_rows);

  for (iter = values, i = 0; iter != NULL; iter = iter->next, i++)
    {
      EggThreeGridRowInfo *row_info = iter->data;

      rows[i].data = row_info;
      rows[i].minimum_size = row_info->min_above_baseline + row_info->min_below_baseline;
      rows[i].natural_size = row_info->nat_above_baseline + row_info->nat_below_baseline;
    }

  gtk_distribute_natural_allocation (area.height, n_rows, rows);

  for (i = 0; i < n_rows; i++)
    {
      GtkRequestedSize *size = &rows[i];
      EggThreeGridRowInfo *row_info = size->data;
      GtkAllocation child_alloc;
      gint baseline;

      if (row_info->nat_above_baseline + row_info->nat_below_baseline <= size->minimum_size)
        baseline = row_info->nat_above_baseline;
      else
        baseline = row_info->min_above_baseline;

      child_alloc.x = area.x;
      child_alloc.width = left;
      child_alloc.y = area.y;
      child_alloc.height = size->minimum_size;
      if (dir == GTK_TEXT_DIR_LTR)
        egg_three_grid_size_allocate_children (self, EGG_THREE_GRID_COLUMN_LEFT, row_info->row, &child_alloc, baseline);
      else
        egg_three_grid_size_allocate_children (self, EGG_THREE_GRID_COLUMN_RIGHT, row_info->row, &child_alloc, baseline);

      child_alloc.x = area.x + left + priv->column_spacing;
      child_alloc.width = center;
      child_alloc.y = area.y;
      child_alloc.height = size->minimum_size;
      egg_three_grid_size_allocate_children (self, EGG_THREE_GRID_COLUMN_CENTER, row_info->row, &child_alloc, baseline);

      child_alloc.x = area.x + area.width - right;
      child_alloc.width = right;
      child_alloc.y = area.y;
      child_alloc.height = size->minimum_size;
      if (dir == GTK_TEXT_DIR_LTR)
        egg_three_grid_size_allocate_children (self, EGG_THREE_GRID_COLUMN_RIGHT, row_info->row, &child_alloc, baseline);
      else
        egg_three_grid_size_allocate_children (self, EGG_THREE_GRID_COLUMN_LEFT, row_info->row, &child_alloc, baseline);

      area.y += child_alloc.height + priv->row_spacing;
      area.height -= child_alloc.height + priv->row_spacing;
    }
}

static void
egg_three_grid_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      user_data)
{
  EggThreeGrid *self = (EggThreeGrid *)container;
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);

  g_assert (GTK_IS_CONTAINER (self));
  g_assert (callback != NULL);

  for (guint i = priv->children->len; i > 0; i--)
    {
      EggThreeGridChild *child = g_ptr_array_index (priv->children, i - 1);

      callback (child->widget, user_data);
    }
}

static void
egg_three_grid_get_child_property (GtkContainer *container,
                                   GtkWidget    *widget,
                                   guint         prop_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  EggThreeGrid *self = (EggThreeGrid *)container;
  EggThreeGridChild *child = egg_three_grid_find_child (self, widget);

  switch (prop_id)
    {
    case CHILD_PROP_COLUMN:
      g_value_set_enum (value, child->column);
      break;

    case CHILD_PROP_ROW:
      g_value_set_uint (value, child->row);
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
egg_three_grid_set_child_property (GtkContainer *container,
                                   GtkWidget    *widget,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EggThreeGrid *self = (EggThreeGrid *)container;
  EggThreeGridChild *child = egg_three_grid_find_child (self, widget);

  switch (prop_id)
    {
    case CHILD_PROP_COLUMN:
      child->column = g_value_get_enum (value);
      break;

    case CHILD_PROP_ROW:
      child->row = g_value_get_uint (value);
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }

  gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
egg_three_grid_finalize (GObject *object)
{
  EggThreeGrid *self = (EggThreeGrid *)object;
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);

  g_clear_pointer (&priv->row_infos, g_hash_table_unref);
  g_clear_pointer (&priv->children, g_ptr_array_unref);

  G_OBJECT_CLASS (egg_three_grid_parent_class)->finalize (object);
}

static void
egg_three_grid_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EggThreeGrid *self = EGG_THREE_GRID (object);
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      g_value_set_uint (value, priv->column_spacing);
      break;

    case PROP_ROW_SPACING:
      g_value_set_uint (value, priv->row_spacing);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_three_grid_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EggThreeGrid *self = EGG_THREE_GRID (object);
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      priv->column_spacing = g_value_get_uint (value);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    case PROP_ROW_SPACING:
      priv->row_spacing = g_value_get_uint (value);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_three_grid_class_init (EggThreeGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = egg_three_grid_finalize;
  object_class->get_property = egg_three_grid_get_property;
  object_class->set_property = egg_three_grid_set_property;

  widget_class->get_request_mode = egg_three_grid_get_request_mode;
  widget_class->get_preferred_height_for_width = egg_three_grid_get_preferred_height_for_width;
  widget_class->get_preferred_width = egg_three_grid_get_preferred_width;
  widget_class->size_allocate = egg_three_grid_size_allocate;

  container_class->add = egg_three_grid_add;
  container_class->forall = egg_three_grid_forall;
  container_class->get_child_property = egg_three_grid_get_child_property;
  container_class->remove = egg_three_grid_remove;
  container_class->set_child_property = egg_three_grid_set_child_property;

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

  child_properties [CHILD_PROP_COLUMN] =
    g_param_spec_enum ("column",
                       "Column",
                       "Column",
                       EGG_TYPE_THREE_GRID_COLUMN,
                       EGG_THREE_GRID_COLUMN_LEFT,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  child_properties [CHILD_PROP_ROW] =
    g_param_spec_uint ("row",
                       "Row",
                       "Row",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gtk_container_class_install_child_properties (container_class, N_CHILD_PROPS, child_properties);

  gtk_widget_class_set_css_name (widget_class, "threegrid");
}

static void
egg_three_grid_init (EggThreeGrid *self)
{
  EggThreeGridPrivate *priv = egg_three_grid_get_instance_private (self);

  priv->children = g_ptr_array_new_with_free_func (egg_three_grid_child_free);

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

GtkWidget *
egg_three_grid_new (void)
{
  return g_object_new (EGG_TYPE_THREE_GRID, NULL);
}

GType
egg_three_grid_column_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;
      static const GEnumValue values[] = {
        { EGG_THREE_GRID_COLUMN_LEFT, "EGG_THREE_GRID_COLUMN_LEFT", "left" },
        { EGG_THREE_GRID_COLUMN_CENTER, "EGG_THREE_GRID_COLUMN_CENTER", "center" },
        { EGG_THREE_GRID_COLUMN_RIGHT, "EGG_THREE_GRID_COLUMN_RIGHT", "right" },
        { 0 }
      };
      _type_id = g_enum_register_static ("EggThreeGridColumn", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
