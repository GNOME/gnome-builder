/* ide-preferences-flow-box.c
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

#include "ide-preferences-flow-box.h"
#include "ide-preferences-group.h"

typedef struct
{
  GtkWidget      *widget;
  GtkAllocation   alloc;
  GtkRequisition  req;
  gint            priority;
} IdePreferencesFlowBoxChild;

struct _IdePreferencesFlowBox
{
  GtkContainer  parent_instance;
  GArray       *children;
};

#define COLUMN_WIDTH   500
#define COLUMN_SPACING 24
#define ROW_SPACING    12

G_DEFINE_TYPE (IdePreferencesFlowBox, ide_preferences_flow_box, GTK_TYPE_CONTAINER)

static void
ide_preferences_flow_box_layout (IdePreferencesFlowBox *self,
                                 gint                   width,
                                 gint                   height,
                                 gint                  *tallest_column)
{
  gint real_tallest_column = 0;
  gint total_height = 0;
  gint n_columns = 0;
  gint border_width;
  gint column;
  guint i;

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (width > 0);
  g_assert (tallest_column != NULL);

  /*
   * We want to layout the children in a series of columns, but try to
   * fill up each column before spilling into the next column.
   *
   * We can determine the number of columns we can support by the width
   * of our allocation, and determine the max-height of each column
   * by dividing the total height of all children by the number of
   * columns. There is the chance that non-uniform sizing will mess up
   * the height a bit here, but in practice it's mostly okay.
   *
   * The order of children is sorted by the priority, so that we know
   * we can allocate them serially as we walk the array.
   *
   * We keep allocating children until we will go over the height of
   * the column.
   */

  border_width = gtk_container_get_border_width (GTK_CONTAINER (self));
  total_height = border_width * 2;

  for (i = 0; i < self->children->len; i++)
    {
      IdePreferencesFlowBoxChild *child;

      child = &g_array_index (self->children, IdePreferencesFlowBoxChild, i);

      gtk_widget_get_preferred_height_for_width (child->widget, COLUMN_WIDTH, NULL, &child->req.height);

      if (i != 0)
        total_height += ROW_SPACING;
      total_height += child->req.height;
    }

  if (total_height <= height)
    n_columns = 1;
  else
    n_columns = MAX (1, (width - (border_width * 2)) / (COLUMN_WIDTH + COLUMN_SPACING));

  for (column = 0, i = 0; column < n_columns; column++)
    {
      GtkAllocation alloc;
      gint j = 0;

      alloc.x = border_width + (COLUMN_WIDTH * column) + (column * COLUMN_SPACING);
      alloc.y = border_width;
      alloc.width = COLUMN_WIDTH;
      alloc.height = (height != 0) ? height : total_height / n_columns;

      for (; i < self->children->len; i++, j++)
        {
          IdePreferencesFlowBoxChild *child;

          child = &g_array_index (self->children, IdePreferencesFlowBoxChild, i);

          /*
           * Ignore this child if it is not visible.
           */
          if (!gtk_widget_get_visible (child->widget) ||
              !gtk_widget_get_child_visible (child->widget))
            continue;

          /*
           * If the child requisition is taller than the space we have left in
           * this column, we need to spill over to the next column.
           */
          if ((j != 0) && (child->req.height > alloc.height) && (column < (n_columns - 1)))
            break;

          child->alloc.x = alloc.x;
          child->alloc.y = alloc.y;
          child->alloc.width = COLUMN_WIDTH;
          child->alloc.height = child->req.height;

#if 0
          g_print ("Allocating child to: [%d] %d,%d %dx%d\n",
                   column,
                   child->alloc.x,
                   child->alloc.y,
                   child->alloc.width,
                   child->alloc.height);
#endif

          alloc.y += child->req.height + ROW_SPACING;
          alloc.height -= child->req.height + ROW_SPACING;

          if (alloc.y > real_tallest_column)
            real_tallest_column = alloc.y;
        }
    }

  real_tallest_column += border_width;

  *tallest_column = real_tallest_column;
}

static GtkSizeRequestMode
ide_preferences_flow_box_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
ide_preferences_flow_box_get_preferred_width (GtkWidget *widget,
                                              gint      *min_width,
                                              gint      *nat_width)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)widget;
  gint border_width;

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (min_width != NULL);
  g_assert (nat_width != NULL);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (self));

  *nat_width = (COLUMN_WIDTH * 3) + (COLUMN_SPACING * 2) + (border_width * 2);
  *min_width = COLUMN_WIDTH + (border_width * 2);
}

static void
ide_preferences_flow_box_get_preferred_height_for_width (GtkWidget *widget,
                                                         gint       width,
                                                         gint      *min_height,
                                                         gint      *nat_height)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)widget;
  gint tallest_column = 0;

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (min_height != NULL);
  g_assert (nat_height != NULL);

  ide_preferences_flow_box_layout (self, width, 0, &tallest_column);

  *min_height = *nat_height = tallest_column;
}

static void
ide_preferences_flow_box_size_allocate (GtkWidget     *widget,
                                        GtkAllocation *allocation)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)widget;
  gint tallest_column = 0;
  guint i;

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);

  ide_preferences_flow_box_layout (self, allocation->width, allocation->height, &tallest_column);

  for (i = 0; i < self->children->len; i++)
    {
      IdePreferencesFlowBoxChild *child;

      child = &g_array_index (self->children, IdePreferencesFlowBoxChild, i);
      gtk_widget_size_allocate (child->widget, &child->alloc);
    }
}

static gint
ide_preferences_flow_box_child_compare (gconstpointer a,
                                        gconstpointer b)
{
  const IdePreferencesFlowBoxChild *child_a = a;
  const IdePreferencesFlowBoxChild *child_b = b;

  return child_a->priority - child_b->priority;
}

static void
ide_preferences_flow_box_add (GtkContainer *container,
                              GtkWidget    *widget)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)container;
  IdePreferencesFlowBoxChild child = { 0 };

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (!IDE_IS_PREFERENCES_GROUP (widget))
    {
      g_warning ("Attempt to add a widget of type \"%s\" to a IdePreferencesFlowBox.",
                 G_OBJECT_TYPE_NAME (widget));
      return;
    }

  child.widget = g_object_ref_sink (widget);
  child.priority = ide_preferences_group_get_priority (IDE_PREFERENCES_GROUP (widget));

  g_array_append_val (self->children, child);
  g_array_sort (self->children, ide_preferences_flow_box_child_compare);

  gtk_widget_set_parent (widget, GTK_WIDGET (self));
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
ide_preferences_flow_box_remove (GtkContainer *container,
                                 GtkWidget    *widget)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)container;
  guint i;

  g_assert (GTK_IS_CONTAINER (container));
  g_assert (GTK_IS_WIDGET (widget));

  for (i = 0; i < self->children->len; i++)
    {
      IdePreferencesFlowBoxChild *child;

      child = &g_array_index (self->children, IdePreferencesFlowBoxChild, i);

      if (child->widget == widget)
        {
          gtk_widget_unparent (child->widget);
          g_array_remove_index (self->children, i);
          gtk_widget_queue_resize (GTK_WIDGET (self));
          return;
        }
    }
}

static void
ide_preferences_flow_box_forall (GtkContainer *container,
                                 gboolean      include_internals,
                                 GtkCallback   callback,
                                 gpointer      user_data)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)container;
  gint i;

  g_assert (GTK_IS_CONTAINER (container));
  g_assert (callback != NULL);

  /*
   * We walk backwards in the array to be safe against callback destorying
   * the widget (and causing it to be removed).
   */

  for (i = self->children->len; i > 0; i--)
    {
      IdePreferencesFlowBoxChild *child;

      child = &g_array_index (self->children, IdePreferencesFlowBoxChild, i - 1);
      callback (child->widget, user_data);
    }
}

static void
ide_preferences_flow_box_finalize (GObject *object)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)object;

  g_clear_pointer (&self->children, g_array_unref);

  G_OBJECT_CLASS (ide_preferences_flow_box_parent_class)->finalize (object);
}

static void
ide_preferences_flow_box_class_init (IdePreferencesFlowBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_preferences_flow_box_finalize;

  widget_class->get_preferred_height_for_width = ide_preferences_flow_box_get_preferred_height_for_width;
  widget_class->get_preferred_width = ide_preferences_flow_box_get_preferred_width;
  widget_class->get_request_mode = ide_preferences_flow_box_get_request_mode;
  widget_class->size_allocate = ide_preferences_flow_box_size_allocate;

  container_class->add = ide_preferences_flow_box_add;
  container_class->forall = ide_preferences_flow_box_forall;
  container_class->remove = ide_preferences_flow_box_remove;
}

static void
ide_preferences_flow_box_init (IdePreferencesFlowBox *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->children = g_array_new (FALSE, TRUE, sizeof (IdePreferencesFlowBoxChild));
}

GtkWidget *
ide_preferences_flow_box_new (void)
{
  return g_object_new (IDE_TYPE_PREFERENCES_FLOW_BOX, NULL);
}
