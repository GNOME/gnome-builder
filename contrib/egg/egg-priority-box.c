/* egg-priority-box.c
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

/**
 * SECTION:egg-priority-box:
 * @title: EggPriorityBox
 *
 * This is like a #GtkBox but uses stable priorities to sort.
 */

#define G_LOG_DOMAIN "egg-priority-box"

#include "egg-priority-box.h"

typedef struct
{
  GtkWidget *widget;
  gint       priority;
} EggPriorityBoxChild;

typedef struct
{
  GArray *children;
} EggPriorityBoxPrivate;

enum {
  CHILD_PROP_0,
  CHILD_PROP_PRIORITY,
  N_CHILD_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (EggPriorityBox, egg_priority_box, GTK_TYPE_BOX)

static GParamSpec *child_properties [N_CHILD_PROPS];

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b)
{
  const EggPriorityBoxChild *child_a = a;
  const EggPriorityBoxChild *child_b = b;

  return child_a->priority - child_b->priority;
}

static void
egg_priority_box_resort (EggPriorityBox *self)
{
  EggPriorityBoxPrivate *priv = egg_priority_box_get_instance_private (self);
  guint i;

  g_assert (EGG_IS_PRIORITY_BOX (self));

  g_array_sort (priv->children, sort_by_priority);

  for (i = 0; i < priv->children->len; i++)
    {
      EggPriorityBoxChild *child = &g_array_index (priv->children, EggPriorityBoxChild, i);

      gtk_container_child_set (GTK_CONTAINER (self), child->widget,
                               "position", i,
                               NULL);
    }
}

static gint
egg_priority_box_get_child_priority (EggPriorityBox *self,
                                     GtkWidget      *widget)
{
  EggPriorityBoxPrivate *priv = egg_priority_box_get_instance_private (self);
  guint i;

  g_assert (EGG_IS_PRIORITY_BOX (self));
  g_assert (GTK_IS_WIDGET (widget));

  for (i = 0; i < priv->children->len; i++)
    {
      EggPriorityBoxChild *child = &g_array_index (priv->children, EggPriorityBoxChild, i);

      if (child->widget == widget)
        return child->priority;
    }

  g_warning ("No such child \"%s\" of \"%s\"",
             G_OBJECT_TYPE_NAME (widget),
             G_OBJECT_TYPE_NAME (self));

  return 0;
}

static void
egg_priority_box_set_child_priority (EggPriorityBox *self,
                                     GtkWidget      *widget,
                                     gint            priority)
{
  EggPriorityBoxPrivate *priv = egg_priority_box_get_instance_private (self);
  guint i;

  g_assert (EGG_IS_PRIORITY_BOX (self));
  g_assert (GTK_IS_WIDGET (widget));

  for (i = 0; i < priv->children->len; i++)
    {
      EggPriorityBoxChild *child = &g_array_index (priv->children, EggPriorityBoxChild, i);

      if (child->widget == widget)
        {
          child->priority = priority;
          egg_priority_box_resort (self);
          return;
        }
    }

  g_warning ("No such child \"%s\" of \"%s\"",
             G_OBJECT_TYPE_NAME (widget),
             G_OBJECT_TYPE_NAME (self));
}

static void
egg_priority_box_add (GtkContainer *container,
                      GtkWidget    *widget)
{
  EggPriorityBox *self = (EggPriorityBox *)container;
  EggPriorityBoxPrivate *priv = egg_priority_box_get_instance_private (self);
  EggPriorityBoxChild child;

  g_assert (EGG_IS_PRIORITY_BOX (self));
  g_assert (GTK_IS_WIDGET (widget));

  child.widget = widget;
  child.priority = 0;

  g_array_append_val (priv->children, child);

  GTK_CONTAINER_CLASS (egg_priority_box_parent_class)->add (container, widget);

  egg_priority_box_resort (self);
}

static void
egg_priority_box_remove (GtkContainer *container,
                         GtkWidget    *widget)
{
  EggPriorityBox *self = (EggPriorityBox *)container;
  EggPriorityBoxPrivate *priv = egg_priority_box_get_instance_private (self);
  guint i;

  g_assert (EGG_IS_PRIORITY_BOX (self));
  g_assert (GTK_IS_WIDGET (widget));

  for (i = 0; i < priv->children->len; i++)
    {
      EggPriorityBoxChild *child;

      child = &g_array_index (priv->children, EggPriorityBoxChild, i);

      if (child->widget == widget)
        {
          g_array_remove_index_fast (priv->children, i);
          break;
        }
    }

  GTK_CONTAINER_CLASS (egg_priority_box_parent_class)->remove (container, widget);

  egg_priority_box_resort (self);
}

static void
egg_priority_box_finalize (GObject *object)
{
  EggPriorityBox *self = (EggPriorityBox *)object;
  EggPriorityBoxPrivate *priv = egg_priority_box_get_instance_private (self);

  g_clear_pointer (&priv->children, g_array_unref);

  G_OBJECT_CLASS (egg_priority_box_parent_class)->finalize (object);
}

static void
egg_priority_box_get_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  EggPriorityBox *self = EGG_PRIORITY_BOX (container);

  switch (prop_id)
    {
    case CHILD_PROP_PRIORITY:
      g_value_set_int (value, egg_priority_box_get_child_priority (self, child));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
egg_priority_box_set_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EggPriorityBox *self = EGG_PRIORITY_BOX (container);

  switch (prop_id)
    {
    case CHILD_PROP_PRIORITY:
      egg_priority_box_set_child_priority (self, child, g_value_get_int (value));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
egg_priority_box_class_init (EggPriorityBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = egg_priority_box_finalize;

  container_class->add = egg_priority_box_add;
  container_class->remove = egg_priority_box_remove;
  container_class->get_child_property = egg_priority_box_get_child_property;
  container_class->set_child_property = egg_priority_box_set_child_property;

  child_properties [CHILD_PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "Priority",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gtk_container_class_install_child_properties (container_class, N_CHILD_PROPS, child_properties);
}

static void
egg_priority_box_init (EggPriorityBox *self)
{
  EggPriorityBoxPrivate *priv = egg_priority_box_get_instance_private (self);

  priv->children = g_array_new (FALSE, FALSE, sizeof (EggPriorityBoxChild));
}
