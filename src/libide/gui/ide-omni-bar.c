/* ide-omni-bar.c
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

#define G_LOG_DOMAIN "ide-omni-bar"

#include "config.h"

#include <libpeas.h>

#include "ide-application.h"
#include "ide-gui-global.h"
#include "ide-notification-list-box-row-private.h"
#include "ide-notification-stack-private.h"
#include "ide-omni-bar-addin.h"
#include "ide-omni-bar.h"

struct _IdeOmniBar
{
  PanelOmniBar          parent_instance;

  PeasExtensionSet     *addins;

  GtkStack             *stack;
  GtkPopover           *popover;
  IdeNotificationStack *notification_stack;
  GtkListBox           *notifications_list_box;
  GtkWidget            *placeholder;
  GtkBox               *sections_box;
};

static void ide_omni_bar_move_next     (IdeOmniBar        *self,
                                        GVariant          *param);
static void ide_omni_bar_move_previous (IdeOmniBar        *self,
                                        GVariant          *param);
static void buildable_iface_init       (GtkBuildableIface *iface);

IDE_DEFINE_ACTION_GROUP (IdeOmniBar, ide_omni_bar, {
  { "move-next", ide_omni_bar_move_next },
  { "move-previous", ide_omni_bar_move_previous },
})

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeOmniBar, ide_omni_bar, PANEL_TYPE_OMNI_BAR,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, ide_omni_bar_init_action_group)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

enum {
  PROP_0,
  PROP_MENU_ID,
  N_PROPS
};

static GtkBuildableIface *parent_buildable_iface;
static GParamSpec *properties [N_PROPS];

static void
ide_omni_bar_notification_stack_changed_cb (IdeOmniBar           *self,
                                            IdeNotificationStack *stack)
{
  IdeNotification *notif;
  gboolean enabled;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_NOTIFICATION_STACK (stack));

  enabled = ide_notification_stack_get_can_move (stack);

  ide_omni_bar_set_action_enabled (self, "move-previous", enabled);
  ide_omni_bar_set_action_enabled (self, "move-next", enabled);

  panel_omni_bar_stop_pulsing (PANEL_OMNI_BAR (self));

  if ((notif = ide_notification_stack_get_visible (stack)))
    {
      if (ide_notification_get_has_progress (notif))
        {
          if (ide_notification_get_progress_is_imprecise (notif))
            panel_omni_bar_start_pulsing (PANEL_OMNI_BAR (self));
        }
    }

  if (ide_notification_stack_is_empty (stack))
    gtk_stack_set_visible_child_name (self->stack, "placeholder");
  else
    gtk_stack_set_visible_child_name (self->stack, "notifications");
}

static void
ide_omni_bar_extension_added_cb (PeasExtensionSet *set,
                                 PeasPluginInfo   *plugin_info,
                                 GObject    *exten,
                                 gpointer          user_data)
{
  IdeOmniBarAddin *addin = (IdeOmniBarAddin *)exten;
  IdeOmniBar *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_OMNI_BAR_ADDIN (addin));
  g_assert (IDE_IS_OMNI_BAR (self));

  ide_omni_bar_addin_load (addin, self);
}

static void
ide_omni_bar_extension_removed_cb (PeasExtensionSet *set,
                                   PeasPluginInfo   *plugin_info,
                                   GObject    *exten,
                                   gpointer          user_data)
{
  IdeOmniBarAddin *addin = (IdeOmniBarAddin *)exten;
  IdeOmniBar *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_OMNI_BAR_ADDIN (addin));
  g_assert (IDE_IS_OMNI_BAR (self));

  ide_omni_bar_addin_unload (addin, self);
}

static GtkWidget *
create_notification_row (gpointer item,
                         gpointer user_data)
{
  IdeNotification *notif = item;
  gboolean has_default;

  g_assert (IDE_IS_NOTIFICATION (notif));

  has_default = ide_notification_get_default_action (notif, NULL, NULL);

  return g_object_new (IDE_TYPE_NOTIFICATION_LIST_BOX_ROW,
                       "activatable", has_default,
                       "notification", notif,
                       "visible", TRUE,
                       NULL);
}

static gboolean
filter_for_popover (gpointer item,
                    gpointer user_data)
{
  IdeNotification *notif = item;

  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (user_data == NULL);

  return !ide_notification_get_has_progress (notif) &&
         ide_notification_get_urgent (notif);
}

static void
ide_omni_bar_context_set_cb (GtkWidget  *widget,
                             IdeContext *context)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;
  g_autoptr(IdeObject) notifications = NULL;
  g_autoptr(GtkFilterListModel) model = NULL;
  g_autoptr(GtkCustomFilter) filter = NULL;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (self->addins == NULL);

  notifications = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_NOTIFICATIONS);
  ide_notification_stack_bind_model (self->notification_stack, G_LIST_MODEL (notifications));

  filter = gtk_custom_filter_new (filter_for_popover, NULL, NULL);
  model = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (notifications)),
                                     GTK_FILTER (g_steal_pointer (&filter)));
  gtk_list_box_bind_model (self->notifications_list_box,
                           G_LIST_MODEL (filter),
                           create_notification_row,
                           NULL, NULL);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_OMNI_BAR_ADDIN,
                                         NULL);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_omni_bar_extension_added_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_omni_bar_extension_removed_cb),
                    self);

  peas_extension_set_foreach (self->addins,
                              ide_omni_bar_extension_added_cb,
                              self);
}

static gboolean
ide_omni_bar_query_tooltip (GtkWidget  *widget,
                            gint        x,
                            gint        y,
                            gboolean    keyboard_mode,
                            GtkTooltip *tooltip)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;
  IdeNotification *notif;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_TOOLTIP (tooltip));

  if ((notif = ide_notification_stack_get_visible (self->notification_stack)))
    {
      g_autofree gchar *body = ide_notification_dup_body (notif);

      if (body != NULL)
        {
          gtk_tooltip_set_text (tooltip, body);
          return TRUE;
        }
    }

  return FALSE;
}

static void
ide_omni_bar_notification_row_activated (IdeOmniBar                *self,
                                         IdeNotificationListBoxRow *row,
                                         GtkListBox                *list_box)
{
  g_autofree gchar *default_action = NULL;
  g_autoptr(GVariant) default_target = NULL;
  IdeNotification *notif;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_NOTIFICATION_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  notif = ide_notification_list_box_row_get_notification (row);

  if (ide_notification_get_default_action (notif, &default_action, &default_target))
    gtk_widget_activate_action_variant (GTK_WIDGET (list_box), default_action, default_target);
}

static void
ide_omni_bar_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  g_assert (IDE_IS_OMNI_BAR (widget));

  GTK_WIDGET_CLASS (ide_omni_bar_parent_class)->measure (widget, orientation, for_size,
                                                         minimum, natural,
                                                         minimum_baseline, natural_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (*natural < 500)
        *natural = 500;
    }
}

static void
ide_omni_bar_dispose (GObject *object)
{
  IdeOmniBar *self = (IdeOmniBar *)object;

  g_assert (IDE_IS_OMNI_BAR (self));

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (ide_omni_bar_parent_class)->dispose (object);
}

static void
ide_omni_bar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeOmniBar *self = IDE_OMNI_BAR (object);

  switch (prop_id)
    {
    case PROP_MENU_ID:
      {
        const char *menu_id = g_value_get_string (value);
        GMenu *menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, menu_id);
        g_object_set (self, "menu-model", menu, NULL);
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_bar_class_init (IdeOmniBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_omni_bar_dispose;
  object_class->set_property = ide_omni_bar_set_property;

  widget_class->query_tooltip = ide_omni_bar_query_tooltip;
  widget_class->measure = ide_omni_bar_measure;

  properties [PROP_MENU_ID] =
    g_param_spec_string ("menu-id",
                         "Menu ID",
                         "The identifier for the merged menu",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-omni-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, notification_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, notifications_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, sections_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, stack);
  gtk_widget_class_bind_template_callback (widget_class, ide_omni_bar_notification_row_activated);

  g_type_ensure (IDE_TYPE_NOTIFICATION_STACK);
}

static void
ide_omni_bar_init (IdeOmniBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_has_tooltip (GTK_WIDGET (self), TRUE);

  g_signal_connect_object (self->notification_stack,
                           "changed",
                           G_CALLBACK (ide_omni_bar_notification_stack_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "omnibar", G_ACTION_GROUP (self));

  ide_omni_bar_set_action_enabled (self, "move-previous", FALSE);
  ide_omni_bar_set_action_enabled (self, "move-next", FALSE);

  ide_widget_set_context_handler (GTK_WIDGET (self), ide_omni_bar_context_set_cb);
}

GtkWidget *
ide_omni_bar_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_BAR, NULL);
}

static void
ide_omni_bar_move_next (IdeOmniBar *self,
                        GVariant   *param)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (param == NULL);

  ide_notification_stack_move_next (self->notification_stack);
}

static void
ide_omni_bar_move_previous (IdeOmniBar *self,
                            GVariant   *param)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (param == NULL);

  ide_notification_stack_move_previous (self->notification_stack);
}

/**
 * ide_omni_bar_add_status_icon:
 * @self: a #IdeOmniBar
 * @widget: the #GtkWidget to add
 * @priority: the sort priority for @widget
 *
 * Adds a status-icon style widget to the end of the omnibar. Generally,
 * you'll want this to be either a GtkButton, GtkLabel, or something simple.
 */
void
ide_omni_bar_add_status_icon (IdeOmniBar *self,
                              GtkWidget  *widget,
                              int         priority)
{
  g_return_if_fail (IDE_IS_OMNI_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  panel_omni_bar_add_suffix (PANEL_OMNI_BAR (self), priority, widget);
}

void
ide_omni_bar_set_placeholder (IdeOmniBar *self,
                              GtkWidget  *widget)
{
  g_return_if_fail (IDE_IS_OMNI_BAR (self));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  if (self->placeholder == widget)
    return;

  if (self->placeholder)
    gtk_stack_remove (self->stack, self->placeholder);

  self->placeholder = widget;

  if (self->placeholder)
    {
      gtk_stack_add_named (self->stack, self->placeholder, "placeholder");
      if (self->notification_stack == NULL ||
          ide_notification_stack_is_empty (self->notification_stack))
        gtk_stack_set_visible_child_name (self->stack, "placeholder");
    }
}

static void
ide_omni_bar_add_child (GtkBuildable *buildable,
                        GtkBuilder   *builder,
                        GObject      *child,
                        const gchar  *type)
{
  IdeOmniBar *self = (IdeOmniBar *)buildable;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (child));

  if (ide_str_equal0 (type, "placeholder") && GTK_IS_WIDGET (child))
    ide_omni_bar_set_placeholder (IDE_OMNI_BAR (self), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = ide_omni_bar_add_child;
}

#define GET_PRIORITY(w)   GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"PRIORITY"))
#define SET_PRIORITY(w,i) g_object_set_data(G_OBJECT(w),"PRIORITY",GINT_TO_POINTER(i))

/**
 * ide_omni_bar_add_popover_section:
 * @self: an #IdeOmniBar
 * @widget: a #GtkWidget
 * @priority: sort priority for the section
 *
 * Adds @widget to the omnibar popover, sorted by @priority
 */
void
ide_omni_bar_add_popover_section (IdeOmniBar *self,
                                  GtkWidget  *widget,
                                  int         priority)
{
  GtkWidget *sibling = NULL;

  g_return_if_fail (IDE_IS_OMNI_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  SET_PRIORITY (widget, priority);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->sections_box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (priority < GET_PRIORITY (child))
        break;
      sibling = child;
    }

  gtk_box_insert_child_after (self->sections_box, widget, sibling);
}
