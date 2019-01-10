/* ide-hover-popover.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-hover-popover"

#include "config.h"

#include <dazzle.h>
#include <libide-code.h>
#include <libide-gui.h>
#include <string.h>

#include "ide-hover-context-private.h"
#include "ide-hover-popover-private.h"

struct _IdeHoverPopover
{
  GtkPopover parent_instance;

  /*
   * A vertical box containing all of our marked content/widgets that
   * were provided by the context.
   */
  GtkBox *box;

  /*
   * Our context to be observed. As items are added to the context,
   * we add them to the popver (creating or re-using the widget) based
   * on the kind of content.
   */
  IdeHoverContext *context;

  /*
   * This is our cancellable to cancel any in-flight requests to the
   * hover providers when the popover is withdrawn. That could happen
   * before we've even really been displayed to the user.
   */
  GCancellable *cancellable;

  /*
   * The position where the hover operation began, in buffer coordinates.
   */
  GdkRectangle hovered_at;

  /*
   * If we've had any providers added, so that we can short-circuit
   * in that case without having to display the popover.
   */
  guint has_providers : 1;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_HOVERED_AT,
  N_PROPS
};

G_DEFINE_TYPE (IdeHoverPopover, ide_hover_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];

static void
ide_hover_popover_add_content (const gchar      *title,
                               IdeMarkedContent *content,
                               GtkWidget        *widget,
                               gpointer          user_data)
{
  IdeHoverPopover *self = user_data;
  GtkBox *box;

  g_assert (content != NULL || widget != NULL);
  g_assert (!widget || GTK_IS_WIDGET (widget));

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (box));

  if (!dzl_str_empty0 (title))
    {
      GtkWidget *label;

      label = g_object_new (GTK_TYPE_LABEL,
                            "xalign", 0.0f,
                            "label", title,
                            "use-markup", FALSE,
                            "visible", TRUE,
                            NULL);
      dzl_gtk_widget_add_style_class (label, "title");
      gtk_container_add (GTK_CONTAINER (box), label);
    }

  if (content != NULL)
    {
      GtkWidget *view = ide_marked_view_new (content);

      if (view != NULL)
        {
          gtk_container_add (GTK_CONTAINER (box), view);
          gtk_widget_show (view);
        }
    }

  if (widget != NULL)
    {
      gtk_container_add (GTK_CONTAINER (box), widget);
      gtk_widget_show (widget);
    }
}

static void
ide_hover_popover_query_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeHoverContext *context = (IdeHoverContext *)object;
  g_autoptr(IdeHoverPopover) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_HOVER_POPOVER (self));

  if (!_ide_hover_context_query_finish (context, result, &error) ||
      !ide_hover_context_has_content (context))
    {
      gtk_widget_destroy (GTK_WIDGET (self));
      return;
    }

  _ide_hover_context_foreach (context,
                              ide_hover_popover_add_content,
                              self);

  gtk_widget_show (GTK_WIDGET (self));
}

static void
ide_hover_popover_get_preferred_height (GtkWidget *widget,
                                        gint      *min_height,
                                        gint      *nat_height)
{
  g_assert (IDE_IS_HOVER_POPOVER (widget));
  g_assert (min_height != NULL);
  g_assert (nat_height != NULL);

  GTK_WIDGET_CLASS (ide_hover_popover_parent_class)->get_preferred_height (widget, min_height, nat_height);

  /*
   * If we have embedded webkit views, they can get some bogus size requests
   * sometimes. So try to detect that and prevent giant popovers.
   */

  if (*nat_height > 1024)
    *nat_height = *min_height;
}

void
_ide_hover_popover_set_hovered_at (IdeHoverPopover    *self,
                                   const GdkRectangle *hovered_at)
{
  g_assert (IDE_IS_HOVER_POPOVER (self));

  if (hovered_at != NULL)
    self->hovered_at = *hovered_at;
  else
    memset (&self->hovered_at, 0, sizeof self->hovered_at);
}

static void
ide_hover_popover_destroy (GtkWidget *widget)
{
  IdeHoverPopover *self = (IdeHoverPopover *)widget;

  g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->context);
  g_clear_object (&self->cancellable);

  GTK_WIDGET_CLASS (ide_hover_popover_parent_class)->destroy (widget);
}

static void
ide_hover_popover_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeHoverPopover *self = IDE_HOVER_POPOVER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, _ide_hover_popover_get_context (self));
      break;

    case PROP_HOVERED_AT:
      g_value_set_boxed (value, &self->hovered_at);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_hover_popover_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeHoverPopover *self = IDE_HOVER_POPOVER (object);

  switch (prop_id)
    {
    case PROP_HOVERED_AT:
      _ide_hover_popover_set_hovered_at (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_hover_popover_class_init (IdeHoverPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_hover_popover_get_property;
  object_class->set_property = ide_hover_popover_set_property;

  widget_class->destroy = ide_hover_popover_destroy;
  widget_class->get_preferred_height = ide_hover_popover_get_preferred_height;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The hover context to display to the user",
                         IDE_TYPE_HOVER_CONTEXT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HOVERED_AT] =
    g_param_spec_boxed ("hovered-at",
                         "Hovered At",
                         "The position that the hover originated in buffer coordinates",
                         GDK_TYPE_RECTANGLE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_hover_popover_init (IdeHoverPopover *self)
{
  GtkStyleContext *style_context;

  self->context = g_object_new (IDE_TYPE_HOVER_CONTEXT, NULL);
  self->cancellable = g_cancellable_new ();

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (style_context, "hoverer");

  self->box = g_object_new (GTK_TYPE_BOX,
                            "orientation", GTK_ORIENTATION_VERTICAL,
                            "visible", TRUE,
                            NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->box));
}

IdeHoverContext *
_ide_hover_popover_get_context (IdeHoverPopover *self)
{
  g_return_val_if_fail (IDE_IS_HOVER_POPOVER (self), NULL);

  return self->context;
}

void
_ide_hover_popover_add_provider (IdeHoverPopover  *self,
                                 IdeHoverProvider *provider)
{
  g_return_if_fail (IDE_IS_HOVER_POPOVER (self));
  g_return_if_fail (IDE_IS_HOVER_PROVIDER (provider));

  _ide_hover_context_add_provider (self->context, provider);

  self->has_providers = TRUE;
}

void
_ide_hover_popover_show (IdeHoverPopover *self)
{
  GtkWidget *view;

  g_return_if_fail (IDE_IS_HOVER_POPOVER (self));
  g_return_if_fail (self->context != NULL);

  if (self->has_providers &&
      !g_cancellable_is_cancelled (self->cancellable) &&
      (view = gtk_popover_get_relative_to (GTK_POPOVER (self))) &&
      GTK_IS_TEXT_VIEW (view))
    {
      GtkTextIter iter;

      /* hovered_at is in buffer coordinates */
      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view),
                                          &iter,
                                          self->hovered_at.x,
                                          self->hovered_at.y);

      _ide_hover_context_query_async (self->context,
                                      &iter,
                                      self->cancellable,
                                      ide_hover_popover_query_cb,
                                      g_object_ref (self));

      return;
    }

  /* Cancel this popover immediately, we have nothing to do */
  gtk_widget_destroy (GTK_WIDGET (self));
}

void
_ide_hover_popover_hide (IdeHoverPopover *self)
{
  g_return_if_fail (IDE_IS_HOVER_POPOVER (self));

  gtk_widget_destroy (GTK_WIDGET (self));
}
