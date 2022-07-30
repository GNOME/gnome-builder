/* ide-run-button.c
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

#define G_LOG_DOMAIN "ide-run-button"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-gtk.h>

#include "ide-device-private.h"

#include "ide-application.h"
#include "ide-gui-global.h"
#include "ide-run-button.h"
#include "ide-run-manager-private.h"

struct _IdeRunButton
{
  GtkWidget       parent_instance;
  AdwSplitButton *split_button;
  IdeJoinedMenu  *joined_menu;
};

G_DEFINE_FINAL_TYPE (IdeRunButton, ide_run_button, GTK_TYPE_WIDGET)

static void
on_icon_state_changed_cb (IdeRunButton  *self,
                          GParamSpec    *pspec,
                          IdeRunManager *run_manager)
{
  const char *icon_name;
  const char *action_name;
  const char *tooltip_text;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  if (!ide_run_manager_get_busy (run_manager))
    {
      icon_name = ide_run_manager_get_icon_name (run_manager);
      action_name = "context.run-manager.run";
      tooltip_text = _("Run Project (Shift+Ctrl+Space)");
    }
  else
    {
      icon_name = "builder-run-stop-symbolic";
      action_name = "context.run-manager.stop";
      tooltip_text = _("Stop Running Project");
    }

  g_object_set (self->split_button,
                "action-name", action_name,
                "icon-name", icon_name,
                NULL);

  gtk_widget_set_tooltip_text (GTK_WIDGET (self), tooltip_text);
}

static void
ide_run_button_load (IdeRunButton *self,
                     IdeContext   *context)
{
  IdeDeviceManager *device_manager;
  IdeRunManager *run_manager;
  GMenu *menu;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (IDE_IS_CONTEXT (context));

  if (!ide_context_has_project (context))
    IDE_EXIT;

  /* Setup button action/icon */
  run_manager = ide_run_manager_from_context (context);
  g_signal_connect_object (run_manager,
                           "notify::busy",
                           G_CALLBACK (on_icon_state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (run_manager,
                           "notify::icon-name",
                           G_CALLBACK (on_icon_state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  on_icon_state_changed_cb (self, NULL, run_manager);

  /* Add devices section */
  device_manager = ide_device_manager_from_context (context);
  menu = _ide_device_manager_get_menu (device_manager);
  ide_joined_menu_prepend_menu (self->joined_menu, G_MENU_MODEL (menu));

  IDE_EXIT;
}

static void
ide_run_button_context_set (GtkWidget  *widget,
                            IdeContext *context)
{
  IdeRunButton *self = (IdeRunButton *)widget;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context != NULL)
    ide_run_button_load (self, context);
}

static gboolean
ide_run_button_query_tooltip (IdeRunButton *self,
                              gint          x,
                              gint          y,
                              gboolean      keyboard_tooltip,
                              GtkTooltip   *tooltip,
                              GtkButton    *button)
{
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (GTK_IS_TOOLTIP (tooltip));
  g_assert (GTK_IS_BUTTON (button));

  context = ide_widget_get_context (GTK_WIDGET (self));
  run_manager = ide_run_manager_from_context (context);

  if (ide_run_manager_get_busy (run_manager))
    gtk_tooltip_set_text (tooltip, _("Stop running"));
  else
    gtk_tooltip_set_text (tooltip, _("Run project"));

  return TRUE;
}

static void
ide_run_button_dispose (GObject *object)
{
  IdeRunButton *self = (IdeRunButton *)object;

  g_clear_pointer ((GtkWidget **)&self->split_button, gtk_widget_unparent);
  g_clear_object (&self->joined_menu);

  G_OBJECT_CLASS (ide_run_button_parent_class)->dispose (object);
}

static void
ide_run_button_class_init (IdeRunButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_run_button_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-run-button.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, split_button);
}

static void
ide_run_button_init (IdeRunButton *self)
{
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->joined_menu = ide_joined_menu_new ();

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "run-menu");
  ide_joined_menu_append_menu (self->joined_menu, G_MENU_MODEL (menu));

  adw_split_button_set_menu_model (self->split_button,
                                   G_MENU_MODEL (self->joined_menu));

  g_signal_connect_object (self->split_button,
                           "query-tooltip",
                           G_CALLBACK (ide_run_button_query_tooltip),
                           self,
                           G_CONNECT_SWAPPED);

  ide_widget_set_context_handler (self, ide_run_button_context_set);
}
