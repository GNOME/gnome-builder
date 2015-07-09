/* gb-slider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <ide.h>

#include <glib/gi18n.h>

#include "gb-slider.h"

typedef struct
{
  GtkWidget     *widget;
  GdkWindow     *window;
  GtkAllocation  allocation;
  guint          position : 3;
} GbSliderChild;

typedef struct
{
  GtkAdjustment    *h_adj;
  GtkAdjustment    *v_adj;

  IdeAnimation     *h_anim;
  IdeAnimation     *v_anim;

  GPtrArray        *children;

  GbSliderPosition  position;
} GbSliderPrivate;

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GbSlider, gb_slider, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GbSlider)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

enum {
  PROP_0,
  PROP_POSITION,
  LAST_PROP
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_POSITION,
};

#define ANIMATION_MODE     IDE_ANIMATION_EASE_IN_QUAD
#define ANIMATION_DURATION 150

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_slider_child_free (GbSliderChild *child)
{
  g_slice_free (GbSliderChild, child);
}

static void
gb_slider_compute_margin (GbSlider *self,
                          gint     *x_margin,
                          gint     *y_margin)
{
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gdouble x_ratio;
  gdouble y_ratio;
  gsize i;
  gint real_top_margin = 0;
  gint real_bottom_margin = 0;
  gint real_left_margin = 0;
  gint real_right_margin = 0;

  g_assert (GB_IS_SLIDER (self));

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;
      gint margin;

      child = g_ptr_array_index (priv->children, i);

      switch (child->position)
        {
        case GB_SLIDER_NONE:
          break;

        case GB_SLIDER_BOTTOM:
          gtk_widget_get_preferred_height (child->widget, NULL, &margin);
          real_bottom_margin = MAX (real_bottom_margin, margin);
          break;

        case GB_SLIDER_TOP:
          gtk_widget_get_preferred_height (child->widget, NULL, &margin);
          real_top_margin = MAX (real_top_margin, margin);
          break;

        case GB_SLIDER_LEFT:
          gtk_widget_get_preferred_width (child->widget, NULL, &margin);
          real_left_margin = MAX (real_left_margin, margin);
          break;

        case GB_SLIDER_RIGHT:
          gtk_widget_get_preferred_width (child->widget, NULL, &margin);
          real_right_margin = MAX (real_right_margin, margin);
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }

  x_ratio = gtk_adjustment_get_value (priv->h_adj);
  y_ratio = gtk_adjustment_get_value (priv->v_adj);

  if (x_ratio < 0.0)
    *x_margin = x_ratio * real_left_margin;
  else if (x_ratio > 0.0)
    *x_margin = x_ratio * real_right_margin;
  else
    *x_margin = 0.0;

  if (y_ratio < 0.0)
    *y_margin = y_ratio * real_bottom_margin;
  else if (y_ratio > 0.0)
    *y_margin = y_ratio * real_top_margin;
  else
    *y_margin = 0.0;
}

static void
gb_slider_compute_child_allocation (GbSlider      *self,
                                    GbSliderChild *child,
                                    GtkAllocation *window_allocation,
                                    GtkAllocation *child_allocation)
{
  GtkAllocation real_window_allocation;
  GtkAllocation real_child_allocation;
  gint nat_height;
  gint nat_width;
  gint x_margin;
  gint y_margin;

  g_assert (GB_IS_SLIDER (self));
  g_assert (child != NULL);
  g_assert (GTK_IS_WIDGET (child->widget));

  gtk_widget_get_allocation (GTK_WIDGET (self), &real_window_allocation);

  gb_slider_compute_margin (self, &x_margin, &y_margin);

  if (child->position == GB_SLIDER_NONE)
    {
      real_child_allocation.y = y_margin;
      real_child_allocation.x = 0;
      real_child_allocation.width = real_window_allocation.width;
      real_child_allocation.height = real_window_allocation.height;
    }
  else if (child->position == GB_SLIDER_TOP)
    {
      gtk_widget_get_preferred_height (child->widget, NULL, &nat_height);

      real_child_allocation.y = -nat_height;
      real_child_allocation.x = 0;
      real_child_allocation.height = nat_height;
      real_child_allocation.width = real_window_allocation.width;
    }
  else if (child->position == GB_SLIDER_BOTTOM)
    {
      gtk_widget_get_preferred_height (child->widget, NULL, &nat_height);

      real_window_allocation.y += real_window_allocation.height + y_margin;
      real_window_allocation.height = nat_height;

      real_child_allocation.y = 0;
      real_child_allocation.x = 0;
      real_child_allocation.height = nat_height;
      real_child_allocation.width = real_window_allocation.width;
    }
  else if (child->position == GB_SLIDER_RIGHT)
    {
      gtk_widget_get_preferred_width (child->widget, NULL, &nat_width);

      real_child_allocation.y = 0;
      real_child_allocation.x = real_window_allocation.width;
      real_child_allocation.height = real_window_allocation.height;
      real_child_allocation.width = nat_width;
    }
  else if (child->position == GB_SLIDER_LEFT)
    {
      gtk_widget_get_preferred_width (child->widget, NULL, &nat_width);

      real_child_allocation.y = 0;
      real_child_allocation.x = -nat_width;
      real_child_allocation.height = real_window_allocation.height;
      real_child_allocation.width = nat_width;
    }

  if (window_allocation)
    *window_allocation = real_window_allocation;

  if (child_allocation)
    *child_allocation = real_child_allocation;
}

static GdkWindow *
gb_slider_create_child_window (GbSlider      *self,
                               GbSliderChild *child)
{
  GtkWidget *widget = (GtkWidget *)self;
  GdkWindow *window;
  GtkAllocation allocation;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_assert (GB_IS_SLIDER (self));
  g_assert (child != NULL);

  gb_slider_compute_child_allocation (self, child, &allocation, NULL);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

  window = gdk_window_new (gtk_widget_get_window (widget), &attributes, attributes_mask);
  gtk_widget_register_window (widget, window);

  gtk_widget_set_parent_window (child->widget, window);

  return window;
}

static void
gb_slider_add (GtkContainer *container,
               GtkWidget    *widget)
{
  GbSlider *self = (GbSlider *)container;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  GbSliderChild *child;

  g_assert (GB_IS_SLIDER (self));
  g_assert (GTK_IS_WIDGET (widget));

  child = g_slice_new0 (GbSliderChild);
  child->position = GB_SLIDER_NONE;
  child->widget = g_object_ref (widget);

  g_ptr_array_add (priv->children, child);

  gtk_widget_set_parent (widget, GTK_WIDGET (self));

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    child->window = gb_slider_create_child_window (self, child);
}

static void
gb_slider_remove (GtkContainer *container,
                  GtkWidget    *widget)
{
  GbSlider *self = (GbSlider *)container;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  GbSliderChild *child;
  gsize i;

  g_assert (GB_IS_SLIDER (self));
  g_assert (GTK_IS_WIDGET (widget));

  for (i = 0; i < priv->children->len; i++)
    {
      child = g_ptr_array_index (priv->children, i);

      if (child->widget == widget)
        {
          gtk_widget_unparent (widget);
          g_ptr_array_remove_index (priv->children, i);
          gtk_widget_queue_resize (GTK_WIDGET (self));
          break;
        }
    }
}

static void
gb_slider_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GbSlider *self = (GbSlider *)widget;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gsize i;

  g_assert (GB_IS_SLIDER (self));
  g_assert (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child = g_ptr_array_index (priv->children, i);

      if (gtk_widget_get_mapped (widget))
        {
          if (gtk_widget_get_visible (child->widget))
            gdk_window_show (child->window);
          else
            gdk_window_hide (child->window);
        }

      if (gtk_widget_get_realized (child->widget))
        {
          GtkAllocation window_allocation;
          GtkAllocation child_allocation;

          gb_slider_compute_child_allocation (self, child, &window_allocation, &child_allocation);

          gdk_window_move_resize (child->window,
                                  window_allocation.x,
                                  window_allocation.y,
                                  window_allocation.width,
                                  window_allocation.height);

          gtk_widget_size_allocate (child->widget, &child_allocation);
        }
    }
}

static void
gb_slider_forall (GtkContainer *container,
                  gboolean      include_internals,
                  GtkCallback   callback,
                  gpointer      callback_data)
{
  GbSlider *self = (GbSlider *)container;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gsize i;

  g_assert (GB_IS_SLIDER (self));

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;

      child = g_ptr_array_index (priv->children, i);
      callback (child->widget, callback_data);
    }
}

static GbSliderChild *
gb_slider_get_child (GbSlider  *self,
                     GtkWidget *widget)
{
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gsize i;

  g_assert (GB_IS_SLIDER (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (gtk_widget_get_parent (widget) == GTK_WIDGET (self));

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;

      child = g_ptr_array_index (priv->children, i);

      if (child->widget == widget)
        return child;
    }

  g_assert_not_reached ();

  return NULL;
}

static GbSliderPosition
gb_slider_child_get_position (GbSlider  *self,
                              GtkWidget *widget)
{
  GbSliderChild *child;

  g_assert (GB_IS_SLIDER (self));
  g_assert (GTK_IS_WIDGET (widget));

  child = gb_slider_get_child (self, widget);

  return child->position;
}

static void
gb_slider_child_set_position (GbSlider         *self,
                              GtkWidget        *widget,
                              GbSliderPosition  position)
{
  GbSliderChild *child;

  g_assert (GB_IS_SLIDER (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (position >= GB_SLIDER_NONE);
  g_assert (position <= GB_SLIDER_LEFT);

  child = gb_slider_get_child (self, widget);

  if (position != child->position)
    {
      child->position = position;
      gtk_container_child_notify (GTK_CONTAINER (self), widget, "position");
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

static void
gb_slider_get_child_property (GtkContainer *container,
                              GtkWidget    *child,
                              guint         prop_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  GbSlider *self = (GbSlider *)container;

  switch (prop_id)
    {
    case CHILD_PROP_POSITION:
      g_value_set_enum (value, gb_slider_child_get_position (self, child));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
gb_slider_set_child_property (GtkContainer *container,
                              GtkWidget    *child,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbSlider *self = (GbSlider *)container;

  switch (prop_id)
    {
    case CHILD_PROP_POSITION:
      gb_slider_child_set_position (self, child, g_value_get_enum (value));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
gb_slider_get_preferred_height (GtkWidget *widget,
                                gint      *min_height,
                                gint      *nat_height)
{
  GbSlider *self = (GbSlider *)widget;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gint real_min_height = 0;
  gint real_nat_height = 0;
  gsize i;

  g_assert (GB_IS_SLIDER (self));

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;
      gint child_min_height = 0;
      gint child_nat_height = 0;

      child = g_ptr_array_index (priv->children, i);

      if ((child->position == GB_SLIDER_NONE) && gtk_widget_get_visible (child->widget))
        {
          gtk_widget_get_preferred_height (child->widget, &child_min_height, &child_nat_height);
          real_min_height = MAX (real_min_height, child_min_height);
          real_nat_height = MAX (real_nat_height, child_nat_height);
        }
    }

  *min_height = real_min_height;
  *nat_height = real_nat_height;
}

static void
gb_slider_get_preferred_width (GtkWidget *widget,
                                gint      *min_width,
                                gint      *nat_width)
{
  GbSlider *self = (GbSlider *)widget;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gint real_min_width = 0;
  gint real_nat_width = 0;
  gsize i;

  g_assert (GB_IS_SLIDER (self));

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;
      gint child_min_width = 0;
      gint child_nat_width = 0;

      child = g_ptr_array_index (priv->children, i);

      if ((child->position == GB_SLIDER_NONE) && gtk_widget_get_visible (child->widget))
        {
          gtk_widget_get_preferred_width (child->widget, &child_min_width, &child_nat_width);
          real_min_width = MAX (real_min_width, child_min_width);
          real_nat_width = MAX (real_nat_width, child_nat_width);
        }
    }

  *min_width = real_min_width;
  *nat_width = real_nat_width;
}

static void
gb_slider_realize (GtkWidget *widget)
{
  GbSlider *self = (GbSlider *)widget;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  GdkWindow *window;
  gsize i;

  g_assert (GB_IS_SLIDER (self));

  gtk_widget_set_realized (widget, TRUE);

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;

      child = g_ptr_array_index (priv->children, i);

      if (child->window == NULL)
        child->window = gb_slider_create_child_window (self, child);
    }
}

static void
gb_slider_unrealize (GtkWidget *widget)
{
  GbSlider *self = (GbSlider *)widget;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gsize i;

  g_assert (GB_IS_SLIDER (self));

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;

      child = g_ptr_array_index (priv->children, i);

      if (child->window != NULL)
        {
          gtk_widget_set_parent_window (child->widget, NULL);
          gtk_widget_unregister_window (widget, child->window);
          gdk_window_destroy (child->window);
          child->window = NULL;
        }
    }

  GTK_WIDGET_CLASS (gb_slider_parent_class)->unrealize (widget);
}

static void
gb_slider_map (GtkWidget *widget)
{
  GbSlider *self = (GbSlider *)widget;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gsize i;

  g_assert (GB_IS_SLIDER (self));

  GTK_WIDGET_CLASS (gb_slider_parent_class)->map (widget);

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;

      child = g_ptr_array_index (priv->children, i);

      if ((child->window != NULL) &&
          gtk_widget_get_visible (child->widget) &&
          gtk_widget_get_child_visible (child->widget))
        gdk_window_show (child->window);
    }
}

static void
gb_slider_unmap (GtkWidget *widget)
{
  GbSlider *self = (GbSlider *)widget;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);
  gsize i;

  g_assert (GB_IS_SLIDER (self));

  for (i = 0; i < priv->children->len; i++)
    {
      GbSliderChild *child;

      child = g_ptr_array_index (priv->children, i);

      if ((child->window != NULL) && gdk_window_is_visible (child->window))
        gdk_window_hide (child->window);
    }

  GTK_WIDGET_CLASS (gb_slider_parent_class)->unmap (widget);
}

static void
gb_slider_add_child (GtkBuildable *buildable,
                     GtkBuilder   *builder,
                     GObject      *child,
                     const gchar  *type)
{
  GbSliderPosition position = GB_SLIDER_NONE;

  g_assert (GTK_IS_BUILDABLE (buildable));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (child));

  if (!GTK_IS_WIDGET (child))
    {
      g_warning ("Child \"%s\" must be of type GtkWidget.",
                 g_type_name (G_OBJECT_TYPE (child)));
      return;
    }

  if (ide_str_equal0 (type, "bottom"))
    position = GB_SLIDER_BOTTOM;
  else if (ide_str_equal0 (type, "top"))
    position = GB_SLIDER_TOP;
  else if (ide_str_equal0 (type, "left"))
    position = GB_SLIDER_LEFT;
  else if (ide_str_equal0 (type, "right"))
    position = GB_SLIDER_RIGHT;

  gtk_container_add_with_properties (GTK_CONTAINER (buildable), GTK_WIDGET (child),
                                     "position", position,
                                     NULL);
}

static void
gb_slider_finalize (GObject *object)
{
  GbSlider *self = (GbSlider *)object;
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);

  g_clear_object (&priv->h_adj);
  g_clear_object (&priv->v_adj);
  g_clear_pointer (&priv->children, g_ptr_array_unref);

  ide_clear_weak_pointer (&priv->h_anim);
  ide_clear_weak_pointer (&priv->v_anim);

  G_OBJECT_CLASS (gb_slider_parent_class)->finalize (object);
}

static void
gb_slider_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GbSlider *self = GB_SLIDER (object);

  switch (prop_id)
    {
    case PROP_POSITION:
      g_value_set_enum (value, gb_slider_get_position (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_slider_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GbSlider *self = GB_SLIDER (object);

  switch (prop_id)
    {
    case PROP_POSITION:
      gb_slider_set_position (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = gb_slider_add_child;
}

static void
gb_slider_class_init (GbSliderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = gb_slider_finalize;
  object_class->get_property = gb_slider_get_property;
  object_class->set_property = gb_slider_set_property;

  widget_class->get_preferred_height = gb_slider_get_preferred_height;
  widget_class->get_preferred_width = gb_slider_get_preferred_width;
  widget_class->map = gb_slider_map;
  widget_class->realize = gb_slider_realize;
  widget_class->size_allocate = gb_slider_size_allocate;
  widget_class->unmap = gb_slider_unmap;
  widget_class->unrealize = gb_slider_unrealize;

  container_class->add = gb_slider_add;
  container_class->forall = gb_slider_forall;
  container_class->get_child_property = gb_slider_get_child_property;
  container_class->remove = gb_slider_remove;
  container_class->set_child_property = gb_slider_set_child_property;

  gParamSpecs [PROP_POSITION] =
    g_param_spec_enum ("position",
                       _("Position"),
                       _("Which slider child is visible."),
                       GB_TYPE_SLIDER_POSITION,
                       GB_SLIDER_NONE,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_POSITION,
                                              g_param_spec_enum ("position",
                                                                 "Position",
                                                                 "Position",
                                                                 GB_TYPE_SLIDER_POSITION,
                                                                 GB_SLIDER_NONE,
                                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gb_slider_init (GbSlider *self)
{
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);

  priv->position = GB_SLIDER_NONE;
  priv->children = g_ptr_array_new_with_free_func ((GDestroyNotify)gb_slider_child_free);

  priv->v_adj = g_object_new (GTK_TYPE_ADJUSTMENT,
                              "lower", -1.0,
                              "upper", 1.0,
                              "value", 0.0,
                              NULL);
  g_signal_connect_object (priv->v_adj,
                           "value-changed",
                           G_CALLBACK (gtk_widget_queue_resize),
                           self,
                           G_CONNECT_SWAPPED);

  priv->h_adj = g_object_new (GTK_TYPE_ADJUSTMENT,
                              "lower", -1.0,
                              "upper", 1.0,
                              "value", 0.0,
                              NULL);
  g_signal_connect_object (priv->h_adj,
                           "value-changed",
                           G_CALLBACK (gtk_widget_queue_resize),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

GType
gb_slider_position_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GB_SLIDER_NONE, "GB_SLIDER_NONE", "none" },
    { GB_SLIDER_TOP, "GB_SLIDER_TOP", "top" },
    { GB_SLIDER_RIGHT, "GB_SLIDER_RIGHT", "right" },
    { GB_SLIDER_BOTTOM, "GB_SLIDER_BOTTOM", "bottom" },
    { GB_SLIDER_LEFT, "GB_SLIDER_LEFT", "left" },
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GbSliderPosition", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GtkWidget *
gb_slider_new (void)
{
  return g_object_new (GB_TYPE_SLIDER, NULL);
}

GbSliderPosition
gb_slider_get_position (GbSlider *self)
{
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);

  g_return_val_if_fail (GB_IS_SLIDER (self), GB_SLIDER_NONE);

  return priv->position;
}

void
gb_slider_set_position (GbSlider         *self,
                        GbSliderPosition  position)
{
  GbSliderPrivate *priv = gb_slider_get_instance_private (self);

  g_return_if_fail (GB_IS_SLIDER (self));
  g_return_if_fail (position >= GB_SLIDER_NONE);
  g_return_if_fail (position <= GB_SLIDER_LEFT);

  if (priv->position != position)
    {
      GdkFrameClock *frame_clock;
      IdeAnimation *anim;
      gdouble v_value;
      gdouble h_value;

      priv->position = position;

      if (priv->h_anim)
        ide_animation_stop (priv->h_anim);
      ide_clear_weak_pointer (&priv->h_anim);

      if (priv->v_anim)
        ide_animation_stop (priv->v_anim);
      ide_clear_weak_pointer (&priv->v_anim);

      switch (position)
        {
        case GB_SLIDER_NONE:
          h_value = 0.0;
          v_value = 0.0;
          break;

        case GB_SLIDER_TOP:
          h_value = 0.0;
          v_value = 1.0;
          break;

        case GB_SLIDER_RIGHT:
          h_value = -1.0;
          v_value = 0.0;
          break;

        case GB_SLIDER_BOTTOM:
          h_value = 0.0;
          v_value = -1.0;
          break;

        case GB_SLIDER_LEFT:
          h_value = 1.0;
          v_value = 0.0;
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (self));

      anim = ide_object_animate (priv->h_adj,
                                 ANIMATION_MODE,
                                 ANIMATION_DURATION,
                                 frame_clock,
                                 "value", h_value,
                                 NULL);
      ide_set_weak_pointer (&priv->h_anim, anim);

      anim = ide_object_animate (priv->v_adj,
                                 ANIMATION_MODE,
                                 ANIMATION_DURATION,
                                 frame_clock,
                                 "value", v_value,
                                 NULL);
      ide_set_weak_pointer (&priv->v_anim, anim);

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_POSITION]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}
