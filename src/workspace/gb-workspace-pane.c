/* gb-workspace-pane.c
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

#include <glib/gi18n.h>

#include "gb-workspace-pane.h"

struct _GbWorkspacePane
{
  GtkBin            parent_instance;

  GtkBox           *box;
  GtkStackSwitcher *stack_switcher;
  GtkStack         *stack;

  GdkRectangle      handle_pos;

  GtkPositionType   position;
};

G_DEFINE_TYPE (GbWorkspacePane, gb_workspace_pane, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_POSITION,
  LAST_PROP
};

enum {
  STYLE_PROP_0,
  STYLE_PROP_HANDLE_SIZE,
  LAST_STYLE_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];
static GParamSpec *gStyleParamSpecs [LAST_STYLE_PROP];

static gboolean
gb_workspace_pane_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  GbWorkspacePane *self = (GbWorkspacePane *)widget;
  GtkStyleContext *style_context;
  gboolean ret;

  g_assert (GB_IS_WORKSPACE_PANE (self));
  g_assert (cr != NULL);

  ret = GTK_WIDGET_CLASS (gb_workspace_pane_parent_class)->draw (widget, cr);

  style_context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_PANE_SEPARATOR);
  gtk_render_handle (style_context, cr,
                     self->handle_pos.x,
                     self->handle_pos.y,
                     self->handle_pos.width,
                     self->handle_pos.height);
  gtk_style_context_restore (style_context);

  return ret;
}

static void
gb_workspace_pane_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *alloc)
{
  GbWorkspacePane *self = (GbWorkspacePane *)widget;
  GtkWidget *child;
  GtkAllocation child_alloc;
  gint handle_size;

  g_assert (GB_IS_WORKSPACE_PANE (self));

  gtk_widget_set_allocation (widget, alloc);

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child == NULL || !gtk_widget_get_visible (child))
    return;

  gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);

  child_alloc = *alloc;

  switch (self->position)
    {
    case GTK_POS_LEFT:
      child_alloc.width -= handle_size;
      self->handle_pos.x = child_alloc.x + child_alloc.width;
      self->handle_pos.width = handle_size;
      self->handle_pos.height = child_alloc.height;
      self->handle_pos.y = child_alloc.y;
      break;

    case GTK_POS_RIGHT:
      child_alloc.x += handle_size;
      child_alloc.width -= handle_size;
      self->handle_pos.x = alloc->x;
      self->handle_pos.width = handle_size;
      self->handle_pos.height = child_alloc.height;
      self->handle_pos.y = child_alloc.y;
      break;

    case GTK_POS_BOTTOM:
      child_alloc.y += handle_size;
      child_alloc.height -= handle_size;
      self->handle_pos.x = alloc->x;
      self->handle_pos.width = alloc->width;
      self->handle_pos.height = handle_size;
      self->handle_pos.y = alloc->y;
      break;

    case GTK_POS_TOP:
      self->handle_pos.x = 0;
      self->handle_pos.y = 0;
      self->handle_pos.width = 0;
      self->handle_pos.height = 0;
      break;

    default:
      break;
    }

  gtk_widget_size_allocate (child, &child_alloc);
}

static void
gb_workspace_pane_finalize (GObject *object)
{
  GbWorkspacePane *self = (GbWorkspacePane *)object;

  self->stack = NULL;
  self->stack_switcher = NULL;

  G_OBJECT_CLASS (gb_workspace_pane_parent_class)->finalize (object);
}

static void
gb_workspace_pane_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbWorkspacePane *self = GB_WORKSPACE_PANE (object);

  switch (prop_id)
    {
    case PROP_POSITION:
      g_value_set_enum (value, gb_workspace_pane_get_position (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workspace_pane_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbWorkspacePane *self = GB_WORKSPACE_PANE (object);

  switch (prop_id)
    {
    case PROP_POSITION:
      gb_workspace_pane_set_position (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workspace_pane_class_init (GbWorkspacePaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_workspace_pane_finalize;
  object_class->get_property = gb_workspace_pane_get_property;
  object_class->set_property = gb_workspace_pane_set_property;

  widget_class->draw = gb_workspace_pane_draw;
  widget_class->size_allocate = gb_workspace_pane_size_allocate;

  /**
   * GbWorkspacePane:position:
   *
   * The position at which to place the pane. This also dictates which
   * direction that animations will occur.
   *
   * For example, setting to %GTK_POS_LEFT will result in the resize grip
   * being placed on the right, and animations to and from the leftmost
   * of the allocation.
   */
  gParamSpecs [PROP_POSITION] =
    g_param_spec_enum ("position",
                       _("Position"),
                       _("The position of the pane."),
                       GTK_TYPE_POSITION_TYPE,
                       GTK_POS_LEFT,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gStyleParamSpecs [STYLE_PROP_HANDLE_SIZE] =
    g_param_spec_int ("handle-size",
                      "Handle Size",
                      "Width of handle.",
                      0, G_MAXINT,
                      1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gtk_widget_class_install_style_property (widget_class,
                                           gStyleParamSpecs [STYLE_PROP_HANDLE_SIZE]);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-workspace-pane.ui");
  gtk_widget_class_bind_template_child (widget_class, GbWorkspacePane, box);
  gtk_widget_class_bind_template_child_internal (widget_class, GbWorkspacePane, stack);
  gtk_widget_class_bind_template_child_internal (widget_class, GbWorkspacePane, stack_switcher);
}

static void
gb_workspace_pane_init (GbWorkspacePane *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gb_workspace_pane_new (void)
{
  return g_object_new (GB_TYPE_WORKSPACE_PANE, NULL);
}

GtkPositionType
gb_workspace_pane_get_position (GbWorkspacePane *self)
{
  g_return_val_if_fail (GB_IS_WORKSPACE_PANE (self), GTK_POS_LEFT);

  return self->position;
}

void
gb_workspace_pane_set_position (GbWorkspacePane *self,
                                GtkPositionType  position)
{
  g_return_if_fail (GB_IS_WORKSPACE_PANE (self));
  g_return_if_fail (position >= GTK_POS_LEFT);
  g_return_if_fail (position <= GTK_POS_BOTTOM);

  if (position != self->position)
    {
      self->position = position;
      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_POSITION]);
    }
}

void
gb_workspace_pane_add_page (GbWorkspacePane *self,
                            GtkWidget       *page,
                            const gchar     *title,
                            const gchar     *icon_name)
{
  gtk_container_add_with_properties (GTK_CONTAINER (self->stack), page,
                                     "icon-name", icon_name,
                                     "title", title,
                                     NULL);
}

void
gb_workspace_pane_remove_page (GbWorkspacePane *self,
                               GtkWidget       *page)
{
  g_return_if_fail (GB_IS_WORKSPACE_PANE (self));
  g_return_if_fail (GTK_IS_WIDGET (page));

  gtk_container_remove (GTK_CONTAINER (self->stack), page);
}
