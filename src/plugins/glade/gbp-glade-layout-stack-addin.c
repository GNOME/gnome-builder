/* gbp-glade-layout-stack-addin.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-glade-layout-stack-addin"

#include <glib/gi18n.h>
#include <gladeui/glade.h>

#include "gbp-glade-layout-stack-addin.h"
#include "gbp-glade-view.h"

struct _GbpGladeLayoutStackAddin
{
  GObject         parent_instance;
  GtkMenuButton  *button;
  GtkLabel       *label;
  GtkImage       *image;
  GladeInspector *inspector;
  DzlSignalGroup *project_signals;
};

static void layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpGladeLayoutStackAddin, gbp_glade_layout_stack_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_LAYOUT_STACK_ADDIN,
                                                layout_stack_addin_iface_init))



static void
gbp_glade_layout_stack_addin_selection_changed_cb (GbpGladeLayoutStackAddin *self,
                                                   GladeProject             *project)
{
  GList *selection = NULL;

  g_assert (GBP_IS_GLADE_LAYOUT_STACK_ADDIN (self));
  g_assert (!project || GLADE_IS_PROJECT (project));

  if (project != NULL)
    selection = glade_project_selection_get (project);

  if (selection != NULL && selection->next == NULL)
    {
      GtkWidget *widget = selection->data;
      GladeWidget *glade = glade_widget_get_from_gobject (widget);
      GladeWidgetAdaptor *adapter = glade_widget_get_adaptor (glade);
      g_autofree gchar *format = NULL;
      const gchar *display_name;
      const gchar *name;
      const gchar *icon_name;

      g_assert (GTK_IS_WIDGET (widget));

      name = glade_widget_get_name (glade);
      display_name = glade_widget_get_display_name (glade);
      icon_name = glade_widget_adaptor_get_icon_name (adapter);

      if (display_name != NULL &&
          display_name[0] != '(' &&
          name != NULL &&
          !g_str_equal (display_name, name))
        name = format = g_strdup_printf ("%s ­— %s", display_name, name);

      gtk_label_set_label (GTK_LABEL (self->label), name);
      g_object_set (self->image,
                    "icon-name", icon_name,
                    "visible", icon_name != NULL,
                    NULL);

      return;
    }

  gtk_label_set_label (GTK_LABEL (self->label), _("Select Widget…"));
  gtk_widget_hide (GTK_WIDGET (self->image));
}

static void
gbp_glade_layout_stack_addin_dispose (GObject *object)
{
  GbpGladeLayoutStackAddin *self = (GbpGladeLayoutStackAddin *)object;

  if (self->project_signals != NULL)
    {
      dzl_signal_group_set_target (self->project_signals, NULL);
      g_clear_object (&self->project_signals);
    }

  G_OBJECT_CLASS (gbp_glade_layout_stack_addin_parent_class)->dispose (object);
}

static void
gbp_glade_layout_stack_addin_class_init (GbpGladeLayoutStackAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_glade_layout_stack_addin_dispose;
}

static void
gbp_glade_layout_stack_addin_init (GbpGladeLayoutStackAddin *self)
{
  self->project_signals = dzl_signal_group_new (GLADE_TYPE_PROJECT);

  dzl_signal_group_connect_object (self->project_signals,
                                   "selection-changed",
                                   G_CALLBACK (gbp_glade_layout_stack_addin_selection_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
on_popover_show_cb (GtkPopover *popover,
                    gpointer    user_data)
{
  GtkTreeView *tree;

  g_assert (GTK_IS_POPOVER (popover));

  tree = dzl_gtk_widget_find_child_typed (GTK_WIDGET (popover), GTK_TYPE_TREE_VIEW);
  gtk_tree_view_expand_all (tree);
}

static void
gbp_glade_layout_stack_addin_load (IdeLayoutStackAddin *addin,
                                   IdeLayoutStack      *stack)
{
  GbpGladeLayoutStackAddin *self = (GbpGladeLayoutStackAddin *)addin;
  GtkPopover *popover;
  GtkWidget *header;
  GtkBox *box;

  g_assert (GBP_IS_GLADE_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  header = ide_layout_stack_get_titlebar (stack);

  popover = g_object_new (GTK_TYPE_POPOVER,
                          "width-request", 400,
                          "height-request", 400,
                          "position", GTK_POS_BOTTOM,
                          NULL);
  g_signal_connect (popover,
                    "show",
                    G_CALLBACK (on_popover_show_cb),
                    NULL);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (popover), "glade-stack-header");

  self->button = g_object_new (GTK_TYPE_MENU_BUTTON,
                               "popover", popover,
                               "visible", FALSE,
                               NULL);
  g_signal_connect (self->button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->button);
  ide_layout_stack_header_add_custom_title (IDE_LAYOUT_STACK_HEADER (header),
                                            GTK_WIDGET (self->button),
                                            200);

  box = g_object_new (GTK_TYPE_BOX,
                      "halign", GTK_ALIGN_CENTER,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 6,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self->button), GTK_WIDGET (box));

  self->image = g_object_new (GTK_TYPE_IMAGE,
                              "icon-size", GTK_ICON_SIZE_MENU,
                              NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (self->image));

  self->label = g_object_new (GTK_TYPE_LABEL,
                              "label", _("Select Widget…"),
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (self->label));

  self->inspector = g_object_new (GLADE_TYPE_INSPECTOR,
                                  "visible", TRUE,
                                  NULL);
  gtk_container_add (GTK_CONTAINER (popover), GTK_WIDGET (self->inspector));
}

static void
gbp_glade_layout_stack_addin_unload (IdeLayoutStackAddin *addin,
                                     IdeLayoutStack      *stack)
{
  GbpGladeLayoutStackAddin *self = (GbpGladeLayoutStackAddin *)addin;

  g_assert (GBP_IS_GLADE_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  if (self->button != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->button));
}

static void
gbp_glade_layout_stack_addin_set_view (IdeLayoutStackAddin *addin,
                                       IdeLayoutView       *view)
{
  GbpGladeLayoutStackAddin *self = (GbpGladeLayoutStackAddin *)addin;
  GladeProject *project = NULL;

  g_assert (GBP_IS_GLADE_LAYOUT_STACK_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  if (GBP_IS_GLADE_VIEW (view))
    project = gbp_glade_view_get_project (GBP_GLADE_VIEW (view));

  glade_inspector_set_project (self->inspector, project);
  gtk_widget_set_visible (GTK_WIDGET (self->button), project != NULL);

  dzl_signal_group_set_target (self->project_signals, project);
  gbp_glade_layout_stack_addin_selection_changed_cb (self, project);
}

static void
layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface)
{
  iface->load = gbp_glade_layout_stack_addin_load;
  iface->unload = gbp_glade_layout_stack_addin_unload;
  iface->set_view = gbp_glade_layout_stack_addin_set_view;
}
