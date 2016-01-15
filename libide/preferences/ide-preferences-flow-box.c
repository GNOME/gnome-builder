/* ide-preferences-flow-box.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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
#include "ide-preferences-group-private.h"

/**
 * SECTION:ide-preferences-flow-box
 *
 * This is a custom container similar to flow box, but not quiet. It is
 * meant to have multiple columns with preference items in it. We will
 * try to reflow the groups based on a couple hueristics to make things
 * more pleasant to look at.
 */

#define BORDER_WIDTH   32
#define COLUMN_WIDTH   500
#define COLUMN_SPACING 32
#define ROW_SPACING    24

struct _IdePreferencesFlowBox
{
  GtkBox     parent_instance;

  guint      needs_reflow : 1;
  guint      max_columns : 3;

  GPtrArray *columns;
  GList     *groups;
};

G_DEFINE_TYPE (IdePreferencesFlowBox, ide_preferences_flow_box, GTK_TYPE_BOX)

static gint
compare_group (gconstpointer a,
               gconstpointer b)
{
  const IdePreferencesGroup *group_a = a;
  const IdePreferencesGroup *group_b = b;

  return group_a->priority - group_b->priority;
}

static gint
find_next_column (IdePreferencesFlowBox *self,
                  IdePreferencesGroup   *group,
                  gint                   last_column)
{
  gint i;
  struct {
    gint column;
    gint height;
  } shortest = { -1, 0 };

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (IDE_IS_PREFERENCES_GROUP (group));
  g_assert (last_column >= -1);
  g_assert (self->columns->len > 0);

  if (ide_preferences_group_get_title (group) == NULL)
    return last_column == -1 ? 0 : last_column;

  for (i = 0; i < self->columns->len; i++)
    {
      GtkBox *column = g_ptr_array_index (self->columns, i);
      gint height;

      gtk_widget_get_preferred_height (GTK_WIDGET (column), NULL, &height);

      if (shortest.column == -1 || height < shortest.height)
        {
          shortest.height = height;
          shortest.column = i;
        }
    }

  return shortest.column;
}

static void
ide_preferences_flow_box_reflow (IdePreferencesFlowBox *self)
{
  GtkAllocation alloc;
  const GList *iter;
  gint n_columns;
  gint width;
  gint spacing;
  gint border_width;
  gint last_column = -1;

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));

  self->needs_reflow = FALSE;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  /*
   * Remove all groups from their containers, and add an extra reference
   * until we have added the items back to a new box.
   */
  for (iter = self->groups; iter; iter = iter->next)
    {
      GtkWidget *group = iter->data;
      GtkWidget *parent = gtk_widget_get_parent (group);

      g_object_ref (group);

      if (parent != NULL)
        gtk_container_remove (GTK_CONTAINER (parent), group);
    }

  /*
   * Now remove the containers.
   */
  while (self->columns->len > 0)
    {
      GtkWidget *box = g_ptr_array_index (self->columns, self->columns->len - 1);

      gtk_container_remove (GTK_CONTAINER (self), box);
      g_ptr_array_remove_index (self->columns, self->columns->len - 1);
    }

  g_assert (self->columns->len == 0);

  /*
   * Determine the number of containers we need based on column width and
   * allocation width, taking border_width and spacing into account.
   */
  n_columns = 1;
  spacing = gtk_box_get_spacing (GTK_BOX (self));
  border_width = gtk_container_get_border_width (GTK_CONTAINER (self));
  width = (border_width * 2) + COLUMN_WIDTH;

  while (TRUE)
    {
      width += spacing;
      width += COLUMN_WIDTH;

      if (width <= alloc.width)
        {
          n_columns++;
          continue;
        }

      break;
    }

  /*
   * Limit ourselves to our max columns.
   */
  n_columns = MIN (n_columns, self->max_columns);

  /*
   * Add those columns, we'll add items to them after they are configured.
   */
  while (self->columns->len < n_columns)
    {
      GtkWidget *column;

      column = g_object_new (GTK_TYPE_BOX,
                             "hexpand", FALSE,
                             "orientation", GTK_ORIENTATION_VERTICAL,
                             "spacing", ROW_SPACING,
                             "visible", TRUE,
                             "width-request", COLUMN_WIDTH,
                             NULL);
      GTK_CONTAINER_CLASS (ide_preferences_flow_box_parent_class)->add (GTK_CONTAINER (self), column);
      g_ptr_array_add (self->columns, column);
    }

  /*
   * Now go through adding groups to columns based on the column with the
   * shortest height. If the group does not have a title, it should be
   * placed in the same column as the previous group.
   */
  for (iter = self->groups; iter; iter = iter->next)
    {
      IdePreferencesGroup *group = iter->data;
      GtkBox *box;
      gint column;

      column = find_next_column (self, group, last_column);
      g_assert (column >= 0);
      g_assert (column < self->columns->len);

      box = g_ptr_array_index (self->columns, column);
      g_assert (GTK_IS_BOX (box));

      gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (group));

      last_column = column;
    }

  /*
   * Now we can drop our extra reference to the groups.
   */
  g_list_foreach (self->groups, (GFunc)g_object_unref, NULL);
}

static void
ide_preferences_flow_box_add_group (IdePreferencesFlowBox *self,
                                    IdePreferencesGroup   *group)
{
  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (IDE_IS_PREFERENCES_GROUP (group));

  self->groups = g_list_insert_sorted (self->groups, group, compare_group);
  self->needs_reflow = TRUE;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
ide_preferences_flow_box_add (GtkContainer *container,
                              GtkWidget    *child)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)container;

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (IDE_IS_PREFERENCES_GROUP (child));

  ide_preferences_flow_box_add_group (self, IDE_PREFERENCES_GROUP (child));
}

static void
ide_preferences_flow_box_size_allocate (GtkWidget     *widget,
                                        GtkAllocation *allocation)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)widget;
  gint border_width;
  gint min_width;
  gint spacing;

  g_assert (IDE_IS_PREFERENCES_FLOW_BOX (self));
  g_assert (allocation != NULL);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  spacing = gtk_box_get_spacing (GTK_BOX (self));

  min_width = (border_width * 2)
            + (spacing * (self->columns->len - 1))
            + (COLUMN_WIDTH * self->columns->len);

  /*
   * If we need to shrink our number of columns, or add another column,
   * lets go through the reflow state.
   */
  if ((allocation->width < min_width) ||
      ((allocation->width >= (min_width + spacing + COLUMN_WIDTH)) &&
       (self->columns->len < self->max_columns)))
    self->needs_reflow = TRUE;

  GTK_WIDGET_CLASS (ide_preferences_flow_box_parent_class)->size_allocate (widget, allocation);

  if (self->needs_reflow)
    ide_preferences_flow_box_reflow (self);
}

static void
ide_preferences_flow_box_get_preferred_width (GtkWidget *widget,
                                              gint      *min_width,
                                              gint      *nat_width)
{
  gint border_width;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (min_width != NULL);
  g_assert (nat_width != NULL);

  GTK_WIDGET_CLASS (ide_preferences_flow_box_parent_class)->get_preferred_width (widget, min_width, nat_width);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  *min_width = (border_width * 2) + COLUMN_WIDTH;

  if (*nat_width < *min_width)
    *nat_width = *min_width;
}

static void
ide_preferences_flow_box_finalize (GObject *object)
{
  IdePreferencesFlowBox *self = (IdePreferencesFlowBox *)object;

  g_clear_pointer (&self->columns, g_ptr_array_unref);
  g_clear_pointer (&self->groups, g_list_free);

  G_OBJECT_CLASS (ide_preferences_flow_box_parent_class)->finalize (object);
}

static void
ide_preferences_flow_box_class_init (IdePreferencesFlowBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_preferences_flow_box_finalize;

  widget_class->size_allocate = ide_preferences_flow_box_size_allocate;
  widget_class->get_preferred_width = ide_preferences_flow_box_get_preferred_width;

  container_class->add = ide_preferences_flow_box_add;

  gtk_widget_class_set_css_name (widget_class, "preferencesflowbox");
}

static void
ide_preferences_flow_box_init (IdePreferencesFlowBox *self)
{
  self->columns = g_ptr_array_new ();
  self->max_columns = 2;

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (self), BORDER_WIDTH);
  gtk_box_set_spacing (GTK_BOX (self), COLUMN_SPACING);
}
