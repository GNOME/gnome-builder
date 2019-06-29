/* ide-frame-header.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-frame-header"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-gui-private.h"
#include "ide-frame-header.h"

#define CSS_PROVIDER_PRIORITY (GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 100)

/**
 * SECTION:ide-frame-header
 * @title: IdeFrameHeader
 * @short_description: The header above document stacks
 *
 * The IdeFrameHeader is the titlebar widget above stacks of documents.
 * It is used to add state when a given document is in view.
 *
 * It can also track the primary color of the content and update it's
 * styling to match.
 *
 * Since: 3.32
 */

struct _IdeFrameHeader
{
  DzlPriorityBox  parent_instance;

  GtkCssProvider *css_provider;
  guint           update_css_handler;

  GdkRGBA         background_rgba;
  GdkRGBA         foreground_rgba;

  guint           background_rgba_set : 1;
  guint           foreground_rgba_set : 1;

  GtkButton      *close_button;
  DzlMenuButton  *document_button;
  GtkMenuButton  *title_button;
  GtkPopover     *title_popover;
  GtkListBox     *title_list_box;
  DzlPriorityBox *title_box;
  GtkLabel       *title_label;
  GtkLabel       *title_modified;
  GtkBox         *title_views_box;

  DzlJoinedMenu  *menu;
};

enum {
  PROP_0,
  PROP_BACKGROUND_RGBA,
  PROP_FOREGROUND_RGBA,
  PROP_MODIFIED,
  PROP_SHOW_CLOSE_BUTTON,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_TYPE (IdeFrameHeader, ide_frame_header, DZL_TYPE_PRIORITY_BOX)

static GParamSpec *properties [N_PROPS];

void
_ide_frame_header_focus_list (IdeFrameHeader *self)
{
  g_return_if_fail (IDE_IS_FRAME_HEADER (self));

  gtk_popover_popup (self->title_popover);
  gtk_widget_grab_focus (GTK_WIDGET (self->title_list_box));
}

void
_ide_frame_header_hide (IdeFrameHeader *self)
{
  GtkPopover *popover;

  g_return_if_fail (IDE_IS_FRAME_HEADER (self));

  /* This is like _ide_frame_header_popdown() but we hide the
   * popovers immediately without performing the popdown animation.
   */

  popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (self->document_button));
  if (popover != NULL)
    gtk_widget_hide (GTK_WIDGET (popover));

  gtk_widget_hide (GTK_WIDGET (self->title_popover));
}

void
_ide_frame_header_popdown (IdeFrameHeader *self)
{
  GtkPopover *popover;

  g_return_if_fail (IDE_IS_FRAME_HEADER (self));

  popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (self->document_button));
  if (popover != NULL)
    gtk_popover_popdown (popover);

  gtk_popover_popdown (self->title_popover);
}

void
_ide_frame_header_update (IdeFrameHeader *self,
                          IdePage        *view)
{
  const gchar *action = "frame.close-page";

  g_assert (IDE_IS_FRAME_HEADER (self));
  g_assert (!view || IDE_IS_PAGE (view));

  /*
   * Update our menus for the document to include the menu type needed for the
   * newly focused view. Make sure we keep the Frame section at the end which
   * is always the last section in the joined menus.
   */

  while (dzl_joined_menu_get_n_joined (self->menu) > 1)
    dzl_joined_menu_remove_index (self->menu, 0);

  if (view != NULL)
    {
      const gchar *menu_id = ide_page_get_menu_id (view);

      if (menu_id != NULL)
        {
          GMenu *menu = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT, menu_id);

          dzl_joined_menu_prepend_menu (self->menu, G_MENU_MODEL (menu));
        }
    }

  /*
   * Hide the document selectors if there are no views to select (which is
   * indicated by us having a NULL view here.
   */
  gtk_widget_set_visible (GTK_WIDGET (self->title_views_box), view != NULL);

  /*
   * The close button acts differently depending on the grid stage.
   *
   *  - Last column, single stack => do nothing (action will be disabled)
   *  - No more views and more than one stack in column (close just the stack)
   *  - No more views and single stack in column and more than one column (close the column)
   */

  if (view == NULL)
    {
      GtkWidget *stack;
      GtkWidget *column;

      action = "gridcolumn.close";
      stack = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_FRAME);
      column = gtk_widget_get_ancestor (GTK_WIDGET (stack), IDE_TYPE_GRID_COLUMN);

      if (stack != NULL && column != NULL)
        {
          if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (column)) > 1)
            action = "frame.close-stack";
        }
    }

  gtk_actionable_set_action_name (GTK_ACTIONABLE (self->close_button), action);

  /*
   * Hide any popovers that we know about. If we got here from closing
   * documents, we should hide the popover after the last document is closed
   * (inidicated by NULL view).
   */
  if (view == NULL)
    _ide_frame_header_popdown (self);
}

static void
close_view_cb (GtkButton      *button,
               IdeFrameHeader *self)
{
  GtkWidget *stack;
  GtkWidget *row;
  GtkWidget *view;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (IDE_IS_FRAME_HEADER (self));

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
  if (row == NULL)
    return;

  view = g_object_get_data (G_OBJECT (row), "IDE_PAGE");
  if (view == NULL)
    return;

  stack = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_FRAME);
  if (stack == NULL)
    return;

  _ide_frame_request_close (IDE_FRAME (stack), IDE_PAGE (view));
}

static gboolean
modified_to_attrs (GBinding     *binding,
                   const GValue *src_value,
                   GValue       *dst_value,
                   gpointer      user_data)
{
  PangoAttrList *attrs = NULL;

  if (g_value_get_boolean (src_value))
    {
      attrs = pango_attr_list_new ();
      pango_attr_list_insert (attrs, pango_attr_style_new (PANGO_STYLE_ITALIC));
    }

  g_value_take_boxed (dst_value, attrs);

  return TRUE;
}

static GtkWidget *
create_document_row (gpointer item,
                     gpointer user_data)
{
  IdeFrameHeader *self = user_data;
  GtkListBoxRow *row;
  GtkButton *close_button;
  GtkLabel *label;
  GtkImage *image;
  GtkBox *box;

  g_assert (IDE_IS_PAGE (item));
  g_assert (IDE_IS_FRAME_HEADER (self));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  box = g_object_new (GTK_TYPE_BOX,
                      "visible", TRUE,
                      NULL);
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-size", GTK_ICON_SIZE_MENU,
                        "visible", TRUE,
                        NULL);
  label = g_object_new (DZL_TYPE_BOLDING_LABEL,
                        "hexpand", TRUE,
                        "xalign", 0.0f,
                        "visible", TRUE,
                        NULL);
  close_button = g_object_new (GTK_TYPE_BUTTON,
                               "child", g_object_new (GTK_TYPE_IMAGE,
                                                      "icon-name", "window-close-symbolic",
                                                      "visible", TRUE,
                                                      NULL),
                               "visible", TRUE,
                               NULL);
  g_signal_connect_object (close_button,
                           "clicked",
                           G_CALLBACK (close_view_cb),
                           self, 0);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (close_button), "image-button");

  g_object_bind_property (item, "icon-name", image, "icon-name", G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (item, "modified", label, "attributes", G_BINDING_SYNC_CREATE,
                               modified_to_attrs, NULL, NULL, NULL);
  g_object_bind_property (item, "title", label, "label", G_BINDING_SYNC_CREATE);
  g_object_set_data (G_OBJECT (row), "IDE_PAGE", item);

  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (close_button));

  return GTK_WIDGET (row);
}

static void
ide_frame_header_model_changed (IdeFrameHeader *self,
                                guint           position,
                                guint           removed,
                                guint           added,
                                GListModel     *model)
{
  guint size;

  g_assert (IDE_IS_FRAME_HEADER (self));
  g_assert (G_IS_LIST_MODEL (model));

  size = g_list_model_get_n_items (model);

  gtk_widget_set_sensitive (GTK_WIDGET (self->title_button), size > 0);
}

void
_ide_frame_header_set_pages (IdeFrameHeader *self,
                             GListModel     *model)
{
  g_assert (IDE_IS_FRAME_HEADER (self));
  g_assert (!model || G_IS_LIST_MODEL (model));

  gtk_list_box_bind_model (self->title_list_box,
                           model,
                           create_document_row,
                           self, NULL);

  /* We need to watch our model for any new document added or removed */
  g_signal_connect_object (model,
                           "items-changed",
                           G_CALLBACK (ide_frame_header_model_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_frame_header_view_row_activated (GtkListBox     *list_box,
                                     GtkListBoxRow  *row,
                                     IdeFrameHeader *self)
{
  GtkWidget *stack;
  GtkWidget *page;

  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (IDE_IS_FRAME_HEADER (self));

  stack = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_FRAME);
  page = g_object_get_data (G_OBJECT (row), "IDE_PAGE");

  if (stack != NULL && page != NULL)
    {
      ide_frame_set_visible_child (IDE_FRAME (stack), IDE_PAGE (page));
      gtk_widget_grab_focus (page);
    }

  _ide_frame_header_popdown (self);
}

static gboolean
ide_frame_header_update_css (IdeFrameHeader *self)
{
  g_autoptr(GString) str = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_FRAME_HEADER (self));
  g_assert (self->css_provider != NULL);
  g_assert (GTK_IS_CSS_PROVIDER (self->css_provider));

  str = g_string_new (NULL);

  /*
   * We set various styles on this provider so that we can update multiple
   * widgets using the same CSS style. That includes ourself, various buttons,
   * labels, and some images.
   */

  if (self->background_rgba_set)
    {
      g_autofree gchar *bgstr = gdk_rgba_to_string (&self->background_rgba);

      g_string_append        (str, "ideframeheader {\n");
      g_string_append        (str, "  background: none;\n");
      g_string_append_printf (str, "  background-color: %s;\n", bgstr);
      g_string_append        (str, "  transition: background-color 400ms;\n");
      g_string_append        (str, "  transition-timing-function: ease;\n");
      g_string_append_printf (str, "  border-bottom: 1px solid shade(%s,0.9);\n", bgstr);
      g_string_append        (str, "  }\n");
      g_string_append        (str, "button { background: transparent; }\n");
      g_string_append        (str, "button:hover, button:checked {\n");
      g_string_append_printf (str, "  background: none; background-color: shade(%s,.85); }\n", bgstr);

      /* only use foreground when background is set */
      if (self->foreground_rgba_set)
        {
          static const gchar *names[] = { "image", "label" };
          g_autofree gchar *fgstr = gdk_rgba_to_string (&self->foreground_rgba);

          for (guint i = 0; i < G_N_ELEMENTS (names); i++)
            {
              g_string_append_printf (str, "%s { ", names[i]);
              g_string_append        (str, "  -gtk-icon-shadow: none;\n");
              g_string_append        (str, "  text-shadow: none;\n");
              g_string_append_printf (str, "  text-shadow: 0 -1px alpha(%s,0.05);\n", fgstr);
              g_string_append_printf (str, "  color: %s;\n", fgstr);
              g_string_append        (str, "}\n");
            }
        }
    }

  /* Use -1 for length so CSS provider knows the string is NULL terminated
   * and there-by avoid a string copy.
   */
  if (!gtk_css_provider_load_from_data (self->css_provider, str->str, -1, &error))
    g_warning ("Failed to load CSS: '%s': %s", str->str, error->message);

  self->update_css_handler = 0;

  return G_SOURCE_REMOVE;
}

static void
ide_frame_header_queue_update_css (IdeFrameHeader *self)
{
  g_assert (IDE_IS_FRAME_HEADER (self));

  if (self->update_css_handler == 0)
    self->update_css_handler =
      gdk_threads_add_idle_full (G_PRIORITY_HIGH,
                                 (GSourceFunc) ide_frame_header_update_css,
                                 g_object_ref (self),
                                 g_object_unref);
}

void
_ide_frame_header_set_background_rgba (IdeFrameHeader *self,
                                       const GdkRGBA  *background_rgba)
{
  GdkRGBA old;
  gboolean old_set;

  g_assert (IDE_IS_FRAME_HEADER (self));

  old_set = self->background_rgba_set;
  old = self->background_rgba;

  if (background_rgba != NULL)
    self->background_rgba = *background_rgba;

  self->background_rgba_set = !!background_rgba;

  if (self->background_rgba_set != old_set || !gdk_rgba_equal (&self->background_rgba, &old))
    ide_frame_header_queue_update_css (self);
}

void
_ide_frame_header_set_foreground_rgba (IdeFrameHeader *self,
                                       const GdkRGBA  *foreground_rgba)
{
  GdkRGBA old;
  gboolean old_set;

  g_assert (IDE_IS_FRAME_HEADER (self));

  old_set = self->foreground_rgba_set;
  old = self->foreground_rgba;

  if (foreground_rgba != NULL)
    self->foreground_rgba = *foreground_rgba;

  self->foreground_rgba_set = !!foreground_rgba;

  if (self->background_rgba_set != old_set || !gdk_rgba_equal (&self->foreground_rgba, &old))
    ide_frame_header_queue_update_css (self);
}

static void
update_widget_providers (GtkWidget      *widget,
                         IdeFrameHeader *self)
{
  g_assert (IDE_IS_FRAME_HEADER (self));
  g_assert (GTK_IS_WIDGET (widget));

  /*
   * The goal here is to explore the widget hierarchy a bit to find widget
   * types that we care about styling. This is the second half to our CSS
   * strategy to assign specific CSS providers to widgets instead of a global
   * CSS provider. The goal here is to avoid the giant CSS invalidation that
   * happens when invalidating the global CSS tree.
   */

  if (GTK_IS_BUTTON (widget) ||
      GTK_IS_LABEL (widget) ||
      GTK_IS_IMAGE (widget) ||
      DZL_IS_SIMPLE_LABEL (widget))
    {
      GtkStyleContext *style_context;

      style_context = gtk_widget_get_style_context (widget);
      gtk_style_context_add_provider (style_context,
                                      GTK_STYLE_PROVIDER (self->css_provider),
                                      CSS_PROVIDER_PRIORITY);
    }

  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
                           (GtkCallback) update_widget_providers,
                           self);
}

static void
ide_frame_header_add (GtkContainer *container,
                      GtkWidget    *widget)
{
  IdeFrameHeader *self = (IdeFrameHeader *)container;

  g_assert (IDE_IS_FRAME_HEADER (self));
  g_assert (GTK_IS_WIDGET (widget));

  GTK_CONTAINER_CLASS (ide_frame_header_parent_class)->add (container, widget);

  update_widget_providers (widget, self);
}

static void
ide_frame_header_get_preferred_width (GtkWidget *widget,
                                      gint      *min_width,
                                      gint      *nat_width)
{
  g_assert (IDE_IS_FRAME_HEADER (widget));
  g_assert (min_width != NULL);
  g_assert (nat_width != NULL);

  GTK_WIDGET_CLASS (ide_frame_header_parent_class)->get_preferred_width (widget, min_width, nat_width);

  /*
   * We don't want changes to the natural width to influence our positioning of
   * the grid separators (unless necessary). So instead, we always return our
   * minimum position as our natural size and let the grid expand as necessary.
   */
  *nat_width = *min_width;
}

static void
ide_frame_header_destroy (GtkWidget *widget)
{
  IdeFrameHeader *self = (IdeFrameHeader *)widget;

  g_assert (IDE_IS_FRAME_HEADER (self));

  dzl_clear_source (&self->update_css_handler);
  g_clear_object (&self->css_provider);

  if (self->title_list_box != NULL)
    gtk_list_box_bind_model (self->title_list_box, NULL, NULL, NULL, NULL);

  g_clear_object (&self->menu);

  GTK_WIDGET_CLASS (ide_frame_header_parent_class)->destroy (widget);
}

static void
ide_frame_header_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeFrameHeader *self = IDE_FRAME_HEADER (object);

  switch (prop_id)
    {
    case PROP_MODIFIED:
      g_value_set_boolean (value, gtk_widget_get_visible (GTK_WIDGET (self->title_modified)));
      break;

    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_widget_get_visible (GTK_WIDGET (self->close_button)));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (self->title_label)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_frame_header_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeFrameHeader *self = IDE_FRAME_HEADER (object);

  switch (prop_id)
    {
    case PROP_BACKGROUND_RGBA:
      _ide_frame_header_set_background_rgba (self, g_value_get_boxed (value));
      break;

    case PROP_FOREGROUND_RGBA:
      _ide_frame_header_set_foreground_rgba (self, g_value_get_boxed (value));
      break;

    case PROP_MODIFIED:
      _ide_frame_header_set_modified (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_CLOSE_BUTTON:
      gtk_widget_set_visible (GTK_WIDGET (self->close_button), g_value_get_boolean (value));
      break;

    case PROP_TITLE:
      _ide_frame_header_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_frame_header_class_init (IdeFrameHeaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = ide_frame_header_get_property;
  object_class->set_property = ide_frame_header_set_property;

  widget_class->destroy = ide_frame_header_destroy;
  widget_class->get_preferred_width = ide_frame_header_get_preferred_width;

  container_class->add = ide_frame_header_add;

  /**
   * IdeFrameHeader:background-rgba:
   *
   * The "background-rgba" property can be used to set the background
   * color of the header. This should be set to the
   * #IdePage:primary-color of the active view.
   *
   * Set to %NULL to unset the primary-color.
   *
   * Since: 3.32
   */
  properties [PROP_BACKGROUND_RGBA] =
    g_param_spec_boxed ("background-rgba",
                        "Background RGBA",
                        "The background color to use for the header",
                        GDK_TYPE_RGBA,
                        (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeFrameHeader:foreground-rgba:
   *
   * Sets the foreground color to use when
   * #IdeFrameHeader:background-rgba is used for the background.
   *
   * Since: 3.32
   */
  properties [PROP_FOREGROUND_RGBA] =
    g_param_spec_boxed ("foreground-rgba",
                        "Foreground RGBA",
                        "The foreground color to use with background-rgba",
                        GDK_TYPE_RGBA,
                        (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_CLOSE_BUTTON] =
    g_param_spec_boolean ("show-close-button",
                          "Show Close Button",
                          "If the close button should be displayed",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MODIFIED] =
    g_param_spec_boolean ("modified",
                          "Modified",
                          "If the current document is modified",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the current document or view",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "ideframeheader");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-frame-header.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, close_button);
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, document_button);
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, title_box);
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, title_button);
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, title_label);
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, title_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, title_modified);
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, title_popover);
  gtk_widget_class_bind_template_child (widget_class, IdeFrameHeader, title_views_box);
}

static void
ide_frame_header_init (IdeFrameHeader *self)
{
  GtkStyleContext *style_context;
  GMenu *frame_section;

  /*
   * To keep our foreground/background colors up to date, we use a CSS
   * provider. However, attaching the provider globally causes much CSS
   * style cascading exactly at the moment we want to animate. To avbid
   * that, and keep animations snappy, we add the provider directly to
   * our widget and to the children widgets we care about (buttons, their
   * labels, etc).
   */
  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  self->css_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider (style_context,
                                  GTK_STYLE_PROVIDER (self->css_provider),
                                  CSS_PROVIDER_PRIORITY);

  gtk_widget_init_template (GTK_WIDGET (self));

  /*
   * Create our menu for the document controls popover. It has two sections.
   * The top section is based on the document and is updated whenever the
   * visible child is changed. The bottom, are the frame controls are and
   * static, but setup by us here.
   */

  self->menu = dzl_joined_menu_new ();
  dzl_menu_button_set_model (self->document_button, G_MENU_MODEL (self->menu));
  frame_section = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT,
                                                  "ide-frame-menu");
  dzl_joined_menu_append_menu (self->menu, G_MENU_MODEL (frame_section));

  /*
   * When a row is selected, we want to change the current view and
   * hide the popover.
   */

  g_signal_connect_object (self->title_list_box,
                           "row-activated",
                           G_CALLBACK (ide_frame_header_view_row_activated),
                           self, 0);

  gtk_widget_set_sensitive (GTK_WIDGET (self->title_button), FALSE);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_container_set_reallocate_redraws (GTK_CONTAINER (self), TRUE);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

GtkWidget *
ide_frame_header_new (void)
{
  return g_object_new (IDE_TYPE_FRAME_HEADER, NULL);
}

/**
 * ide_frame_header_add_custom_title:
 * @self: a #IdeFrameHeader
 * @widget: a #GtkWidget
 * @priority: the sort priority
 *
 * This will add @widget to the title area with @priority determining the
 * sort order of the child.
 *
 * All "title" widgets in the #IdeFrameHeader are expanded to the
 * same size. So if you don't need that, you should just use the normal
 * gtk_container_add_with_properties() API to specify your widget with
 * a given priority.
 *
 * Since: 3.32
 */
void
ide_frame_header_add_custom_title (IdeFrameHeader *self,
                                   GtkWidget      *widget,
                                   gint            priority)
{
  g_return_if_fail (IDE_IS_FRAME_HEADER (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_container_add_with_properties (GTK_CONTAINER (self->title_box), widget,
                                     "priority", priority,
                                     NULL);

  update_widget_providers (widget, self);
}

void
_ide_frame_header_set_title (IdeFrameHeader *self,
                             const gchar    *title)
{
  g_return_if_fail (IDE_IS_FRAME_HEADER (self));

  gtk_label_set_label (GTK_LABEL (self->title_label), title);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

void
_ide_frame_header_set_modified (IdeFrameHeader *self,
                                gboolean        modified)
{
  PangoAttrList *attrs = NULL;

  g_return_if_fail (IDE_IS_FRAME_HEADER (self));

  if (modified)
    {
      attrs = pango_attr_list_new ();
      pango_attr_list_insert (attrs, pango_attr_style_new (PANGO_STYLE_ITALIC));
    }

  gtk_label_set_attributes (self->title_label, attrs);
  gtk_widget_set_visible (GTK_WIDGET (self->title_modified), modified);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODIFIED]);

  g_clear_pointer (&attrs, pango_attr_list_unref);
}
