/*
 * gbp-manuals-pathbar.c
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

#include "gbp-manuals-pathbar.h"

#include "manuals-path-button.h"
#include "manuals-path-element.h"
#include "manuals-path-model.h"

struct _GbpManualsPathbar
{
  GtkWidget           parent_instance;

  ManualsNavigatable *navigatable;
  ManualsPathModel   *model;

  GtkBox             *elements;
  GtkScrolledWindow  *scroller;

  int                 inhibit_scroll;
  guint               scroll_source;
};

enum {
  PROP_0,
  PROP_NAVIGATABLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpManualsPathbar, gbp_manuals_pathbar, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static void
gbp_manuals_pathbar_scroll_to_end (GbpManualsPathbar *self)
{
  GtkAdjustment *hadj;
  double page_size;
  double upper;

  g_assert (GBP_IS_MANUALS_PATHBAR (self));

  if (self->inhibit_scroll)
    return;

  hadj = gtk_scrolled_window_get_hadjustment (self->scroller);
  upper = gtk_adjustment_get_upper (hadj);
  page_size = gtk_adjustment_get_page_size (hadj);

  gtk_adjustment_set_value (hadj, upper - page_size);
}

static gboolean
gbp_manuals_pathbar_scroll_to_end_idle (gpointer data)
{
  GbpManualsPathbar *self = data;

  g_assert (GBP_IS_MANUALS_PATHBAR (self));

  self->scroll_source = 0;
  gbp_manuals_pathbar_scroll_to_end (self);
  return G_SOURCE_REMOVE;
}

static void
gbp_manuals_pathbar_queue_scroll (GbpManualsPathbar *self)
{
  g_assert (GBP_IS_MANUALS_PATHBAR (self));

  g_clear_handle_id (&self->scroll_source, g_source_remove);
  self->scroll_source = g_idle_add_full (G_PRIORITY_LOW,
                                         gbp_manuals_pathbar_scroll_to_end_idle,
                                         g_object_ref (self),
                                         g_object_unref);
}

static void
gbp_manuals_pathbar_notify_upper_cb (GbpManualsPathbar *self,
                                     GParamSpec        *pspec,
                                     GtkAdjustment     *hadj)
{
  GtkWidget *focus;
  GtkRoot *root;

  g_assert (GBP_IS_MANUALS_PATHBAR (self));
  g_assert (GTK_IS_ADJUSTMENT (hadj));

  root = gtk_widget_get_root (GTK_WIDGET (self));
  focus = gtk_root_get_focus (root);

  if (focus && gtk_widget_is_ancestor (focus, GTK_WIDGET (self)))
    return;

  gbp_manuals_pathbar_queue_scroll (self);
}

static GtkWidget *
create_button (ManualsPathElement *element)
{
  g_autoptr(ManualsPathElement) to_free = element;

  g_assert (MANUALS_IS_PATH_ELEMENT (element));

  return g_object_new (MANUALS_TYPE_PATH_BUTTON,
                       "element", element,
                       "valign", GTK_ALIGN_CENTER,
                       NULL);
}

static void
gbp_manuals_pathbar_path_items_changed_cb (GbpManualsPathbar *self,
                                           guint              position,
                                           guint              removed,
                                           guint              added,
                                           ManualsPathModel  *model)
{
  g_assert (GBP_IS_MANUALS_PATHBAR (self));
  g_assert (MANUALS_IS_PATH_MODEL (model));

  if (removed > 0)
    {
      GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->elements));

      for (guint j = position; j > 0; j--)
        child = gtk_widget_get_next_sibling (child);

      while (removed > 0)
        {
          GtkWidget *to_remove = child;
          child = gtk_widget_get_next_sibling (child);
          gtk_widget_unparent (to_remove);
          removed--;
        }
    }

  if (added > 0)
    {
      GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->elements));

      for (guint j = position; j > 0; j--)
        child = gtk_widget_get_next_sibling (child);

      for (guint i = 0; i < added; i++)
        {
          GtkWidget *to_add = create_button (g_list_model_get_item (G_LIST_MODEL (model), position + i));
          gtk_box_insert_child_after (self->elements, to_add, child);
          child = to_add;
        }
    }
}

static void
gbp_manuals_pathbar_dispose (GObject *object)
{
  GbpManualsPathbar *self = (GbpManualsPathbar *)object;
  GtkWidget *child;

  gtk_widget_dispose_template (GTK_WIDGET (self), GBP_TYPE_MANUALS_PATHBAR);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->navigatable);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (gbp_manuals_pathbar_parent_class)->dispose (object);
}

static void
gbp_manuals_pathbar_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpManualsPathbar *self = GBP_MANUALS_PATHBAR (object);

  switch (prop_id)
    {
    case PROP_NAVIGATABLE:
      g_value_set_object (value, gbp_manuals_pathbar_get_navigatable (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_pathbar_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpManualsPathbar *self = GBP_MANUALS_PATHBAR (object);

  switch (prop_id)
    {
    case PROP_NAVIGATABLE:
      gbp_manuals_pathbar_set_navigatable (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_pathbar_class_init (GbpManualsPathbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_manuals_pathbar_dispose;
  object_class->get_property = gbp_manuals_pathbar_get_property;
  object_class->set_property = gbp_manuals_pathbar_set_property;

  properties[PROP_NAVIGATABLE] =
    g_param_spec_object ("navigatable", NULL, NULL,
                         MANUALS_TYPE_NAVIGATABLE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "GbpManualsPathbar");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/manuals/gbp-manuals-pathbar.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpManualsPathbar, elements);
  gtk_widget_class_bind_template_child (widget_class, GbpManualsPathbar, scroller);
}

static void
gbp_manuals_pathbar_init (GbpManualsPathbar *self)
{
  guint n_items;

  self->model = manuals_path_model_new ();

  g_signal_connect_object (self->model,
                           "items-changed",
                           G_CALLBACK (gbp_manuals_pathbar_path_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_init_template (GTK_WIDGET (self));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));

  if (n_items > 0)
    gbp_manuals_pathbar_path_items_changed_cb (self, 0, n_items, 0, self->model);

  g_signal_connect_object (gtk_scrolled_window_get_hadjustment (self->scroller),
                           "notify::upper",
                           G_CALLBACK (gbp_manuals_pathbar_notify_upper_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GbpManualsPathbar *
gbp_manuals_pathbar_new (void)
{
  return g_object_new (GBP_TYPE_MANUALS_PATHBAR, NULL);
}

ManualsNavigatable *
gbp_manuals_pathbar_get_navigatable (GbpManualsPathbar *self)
{
  g_return_val_if_fail (GBP_IS_MANUALS_PATHBAR (self), NULL);

  return self->navigatable;
}

void
gbp_manuals_pathbar_set_navigatable (GbpManualsPathbar  *self,
                                     ManualsNavigatable *navigatable)
{
  g_return_if_fail (GBP_IS_MANUALS_PATHBAR (self));

  if (g_set_object (&self->navigatable, navigatable))
    {
      manuals_path_model_set_navigatable (self->model, navigatable);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAVIGATABLE]);
    }
}

void
gbp_manuals_pathbar_inhibit_scroll (GbpManualsPathbar *self)
{
  g_return_if_fail (GBP_IS_MANUALS_PATHBAR (self));

  self->inhibit_scroll++;
}

void
gbp_manuals_pathbar_uninhibit_scroll (GbpManualsPathbar *self)
{
  g_return_if_fail (GBP_IS_MANUALS_PATHBAR (self));

  self->inhibit_scroll--;
}
