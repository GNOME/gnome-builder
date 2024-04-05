/*
 * manuals-path-button.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libide-gui.h>

#include "gbp-manuals-pathbar.h"
#include "gbp-manuals-workspace-addin.h"

#include "manuals-path-button.h"
#include "manuals-sdk.h"
#include "manuals-search-result.h"

struct _ManualsPathButton
{
  GtkWidget           parent_instance;

  ManualsPathElement *element;

  GtkBox             *box;
  GtkImage           *image;
  GtkLabel           *label;
  GtkLabel           *separator;
  GtkPopover         *popover;
  GtkNoSelection     *selection;
};

enum {
  PROP_0,
  PROP_ELEMENT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (ManualsPathButton, manuals_path_button, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static void
manuals_path_button_list_item_activate_cb (ManualsPathButton *self,
                                           guint              position,
                                           GtkListView       *list_view)
{
  g_autoptr(ManualsNavigatable) navigatable = NULL;
  IdeWorkspaceAddin *addin;
  GtkSelectionModel *model;
  IdeWorkspace *workspace;

  g_assert (MANUALS_IS_PATH_BUTTON (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  model = gtk_list_view_get_model (list_view);
  navigatable = g_list_model_get_item (G_LIST_MODEL (model), position);
  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  addin = ide_workspace_addin_find_by_module_name (workspace, "manuals");

  gtk_popover_popdown (self->popover);

  gbp_manuals_workspace_addin_navigate_to (GBP_MANUALS_WORKSPACE_ADDIN (addin), navigatable);
}

static void
manuals_path_button_popover_closed_cb (ManualsPathButton *self,
                                       GtkPopover        *popover)
{
  GtkWidget *bar;

  g_assert (MANUALS_IS_PATH_BUTTON (self));
  g_assert (GTK_IS_POPOVER (popover));

  gtk_widget_unset_state_flags (GTK_WIDGET (self->box),
                                GTK_STATE_FLAG_ACTIVE);
  gtk_widget_queue_resize (GTK_WIDGET (self));

  bar = gtk_widget_get_ancestor (GTK_WIDGET (self), GBP_TYPE_MANUALS_PATHBAR);
  gbp_manuals_pathbar_uninhibit_scroll (GBP_MANUALS_PATHBAR (bar));
}

static DexFuture *
manuals_path_button_show_popover (DexFuture *completed,
                                  gpointer   user_data)
{
  ManualsPathButton *self = user_data;
  g_autoptr(GListModel) model = NULL;
  GtkScrollInfo *scroll_info;
  GtkWidget *viewport;
  GtkWidget *bar;

  g_assert (MANUALS_IS_PATH_BUTTON (self));
  g_assert (DEX_IS_FUTURE (completed));

  bar = gtk_widget_get_ancestor (GTK_WIDGET (self), GBP_TYPE_MANUALS_PATHBAR);
  viewport = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_VIEWPORT);

  gbp_manuals_pathbar_inhibit_scroll (GBP_MANUALS_PATHBAR (bar));

  scroll_info = gtk_scroll_info_new ();
  gtk_scroll_info_set_enable_horizontal (scroll_info, TRUE);
  gtk_viewport_scroll_to (GTK_VIEWPORT (viewport), GTK_WIDGET (self->box), scroll_info);

  gtk_widget_grab_focus (GTK_WIDGET (self->box));

  model = dex_await_object (dex_ref (completed), NULL);

  if (g_list_model_get_n_items (model) > 0)
    {
      gtk_widget_set_state_flags (GTK_WIDGET (self->box),
                                  GTK_STATE_FLAG_ACTIVE,
                                  FALSE);
      gtk_no_selection_set_model (self->selection, model);
      gtk_popover_popup (self->popover);
    }

  return dex_future_new_for_boolean (TRUE);
}

static void
manuals_path_button_context_pressed_cb (ManualsPathButton *self,
                                        int                n_press,
                                        double             x,
                                        double             y,
                                        GtkGestureClick   *click)
{
  DexFuture *future;
  GtkWidget *widget;
  GObject *object;
  int button;

  g_assert (MANUALS_IS_PATH_BUTTON (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));
  if (button != 3)
    return;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));
  if (gtk_widget_get_focus_on_click (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  object = manuals_path_element_get_item (self->element);

  g_assert (MANUALS_IS_NAVIGATABLE (object));

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);

  future = manuals_navigatable_find_peers (MANUALS_NAVIGATABLE (object));
  future = dex_future_then (future,
                            manuals_path_button_show_popover,
                            g_object_ref (self),
                            g_object_unref);
  dex_future_disown (future);
}

static void
manuals_path_button_pressed_cb (ManualsPathButton *self,
                                int                n_press,
                                double             x,
                                double             y,
                                GtkGestureClick   *click)
{
  IdeWorkspaceAddin *addin;
  IdeWorkspace *workspace;
  GtkWidget *widget;
  GObject *object;

  g_assert (MANUALS_IS_PATH_BUTTON (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));
  if (gtk_widget_get_focus_on_click (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);

  object = manuals_path_element_get_item (self->element);
  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  addin = ide_workspace_addin_find_by_module_name (workspace, "manuals");

  g_assert (GBP_IS_MANUALS_WORKSPACE_ADDIN (addin));
  g_assert (MANUALS_IS_NAVIGATABLE (object));

  gbp_manuals_workspace_addin_navigate_to (GBP_MANUALS_WORKSPACE_ADDIN (addin),
                                           MANUALS_NAVIGATABLE (object));

  gtk_widget_unset_state_flags (GTK_WIDGET (self->box),
                                GTK_STATE_FLAG_ACTIVE);
}

static void
update_css_class (ManualsPathButton *self,
                  const char        *name,
                  gboolean           on)
{
  if (on)
    gtk_widget_add_css_class (GTK_WIDGET (self), name);
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), name);
}

static void
manuals_path_button_notify_is_leaf (ManualsPathButton  *self,
                                    GParamSpec         *pspec,
                                    ManualsPathElement *element)
{
  const char *title;
  gsize title_len;
  gboolean is_leaf;

  g_object_get (element, "is-leaf", &is_leaf, NULL);

  update_css_class (self, "leaf", is_leaf);

  title = manuals_path_element_get_title (element);
  title_len = title ? strlen (title) : 0;

  if (is_leaf || title_len <= 7)
    {
      gtk_label_set_width_chars (self->label, -1);
      gtk_label_set_ellipsize (self->label, PANGO_ELLIPSIZE_NONE);
    }
  else
    {
      gtk_label_set_width_chars (self->label, 7);
      gtk_label_set_ellipsize (self->label, PANGO_ELLIPSIZE_MIDDLE);
    }
}

static void
manuals_path_button_notify_is_root (ManualsPathButton  *self,
                                    GParamSpec         *pspec,
                                    ManualsPathElement *element)
{
  gboolean is_root;
  g_object_get (element, "is-root", &is_root, NULL);
  update_css_class (self, "root", is_root);
}

static gboolean
invert_boolean (gpointer ignored,
                gboolean value)
{
  return !value;
}

static void
manuals_path_button_dispose (GObject *object)
{
  ManualsPathButton *self = (ManualsPathButton *)object;
  GtkWidget *child;

  manuals_path_button_set_element (self, NULL);

  gtk_widget_dispose_template (GTK_WIDGET (self), MANUALS_TYPE_PATH_BUTTON);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (manuals_path_button_parent_class)->dispose (object);
}

static void
manuals_path_button_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ManualsPathButton *self = MANUALS_PATH_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ELEMENT:
      g_value_set_object (value, manuals_path_button_get_element (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_path_button_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ManualsPathButton *self = MANUALS_PATH_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ELEMENT:
      manuals_path_button_set_element (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_path_button_class_init (ManualsPathButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = manuals_path_button_dispose;
  object_class->get_property = manuals_path_button_get_property;
  object_class->set_property = manuals_path_button_set_property;

  properties[PROP_ELEMENT] =
    g_param_spec_object ("element", NULL, NULL,
                         MANUALS_TYPE_PATH_ELEMENT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/manuals/manuals-path-button.ui");
  gtk_widget_class_set_css_name (widget_class, "pathbutton");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_bind_template_child (widget_class, ManualsPathButton, box);
  gtk_widget_class_bind_template_child (widget_class, ManualsPathButton, image);
  gtk_widget_class_bind_template_child (widget_class, ManualsPathButton, label);
  gtk_widget_class_bind_template_child (widget_class, ManualsPathButton, popover);
  gtk_widget_class_bind_template_child (widget_class, ManualsPathButton, selection);
  gtk_widget_class_bind_template_child (widget_class, ManualsPathButton, separator);

  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, manuals_path_button_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, manuals_path_button_context_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, manuals_path_button_popover_closed_cb);
  gtk_widget_class_bind_template_callback (widget_class, manuals_path_button_list_item_activate_cb);
}

static void
manuals_path_button_init (ManualsPathButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

ManualsPathButton *
manuals_path_button_new (void)
{
  return g_object_new (MANUALS_TYPE_PATH_BUTTON, NULL);
}

/**
 * manuals_path_button_get_element:
 * @self: a #ManualsPathButton
 *
 * Returns: (transfer none) (nullable): a #ManaulsPathElement or %NULL
 */
ManualsPathElement *
manuals_path_button_get_element (ManualsPathButton *self)
{
  g_return_val_if_fail (MANUALS_IS_PATH_BUTTON (self), NULL);

  return self->element;
}

void
manuals_path_button_set_element (ManualsPathButton  *self,
                                 ManualsPathElement *element)
{
  const char *title = NULL;

  g_return_if_fail (MANUALS_IS_PATH_BUTTON (self));
  g_return_if_fail (!element || G_IS_OBJECT (element));

  if (element == self->element)
    return;

  if (self->element != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->element,
                                            G_CALLBACK (manuals_path_button_notify_is_root),
                                            self);
      g_signal_handlers_disconnect_by_func (self->element,
                                            G_CALLBACK (manuals_path_button_notify_is_leaf),
                                            self);
    }

  g_set_object (&self->element, element);

  if (self->element != NULL)
    {
      g_signal_connect_object (self->element,
                               "notify::is-root",
                               G_CALLBACK (manuals_path_button_notify_is_root),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->element,
                               "notify::is-leaf",
                               G_CALLBACK (manuals_path_button_notify_is_leaf),
                               self,
                               G_CONNECT_SWAPPED);

      manuals_path_button_notify_is_root (self, NULL, self->element);
      manuals_path_button_notify_is_leaf (self, NULL, self->element);

      title = manuals_path_element_get_title (self->element);
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self), "leaf");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "root");
    }

  gtk_label_set_label (self->label, title);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ELEMENT]);
}
