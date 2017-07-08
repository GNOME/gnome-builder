/* ide-run-button.c
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

#define G_LOG_DOMAIN "ide-run-button"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "runner/ide-run-button.h"
#include "runner/ide-run-manager.h"
#include "runner/ide-run-manager-private.h"
#include "util/ide-gtk.h"

struct _IdeRunButton
{
  GtkBox                parent_instance;

  GtkSizeGroup         *accel_size_group;
  GtkButton            *button;
  GtkImage             *button_image;
  GtkListBox           *list_box;
  GtkMenuButton        *menu_button;
  GtkPopover           *popover;
  GtkButton            *stop_button;
  GtkShortcutsShortcut *run_shortcut;
};

G_DEFINE_TYPE (IdeRunButton, ide_run_button, GTK_TYPE_BOX)

static GtkWidget *
create_row (const IdeRunHandlerInfo *info,
            IdeRunButton            *self)
{
  GtkListBoxRow *row;
  GtkLabel *label;
  GtkImage *image;
  GtkBox *box;

  g_assert (info != NULL);
  g_assert (IDE_IS_RUN_BUTTON (self));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "selectable", FALSE,
                      "visible", TRUE,
                      NULL);

  g_object_set_data_full (G_OBJECT (row), "IDE_RUN_HANDLER_ID", g_strdup (info->id), g_free);

  box = g_object_new (GTK_TYPE_BOX,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));

  image = g_object_new (GTK_TYPE_IMAGE,
                        "hexpand", FALSE,
                        "icon-name", info->icon_name,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", info->title,
                        "hexpand", TRUE,
                        "xalign", 0.0f,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));

  if (info->accel != NULL)
    {
      g_autofree gchar *xaccel = NULL;
      guint accel_key = 0;
      GdkModifierType accel_mod = 0;

      gtk_accelerator_parse (info->accel, &accel_key, &accel_mod);
      xaccel = gtk_accelerator_get_label (accel_key, accel_mod);
      label = g_object_new (GTK_TYPE_LABEL,
                            "label", xaccel,
                            "visible", TRUE,
                            "xalign", 0.0f,
                            NULL);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (label), "dim-label");
      dzl_gtk_widget_add_style_class (GTK_WIDGET (label), "accel");
      gtk_container_add_with_properties (GTK_CONTAINER (box), GTK_WIDGET (label),
                                         "pack-type", GTK_PACK_END,
                                         NULL);
      gtk_size_group_add_widget (self->accel_size_group, GTK_WIDGET (label));
    }

  return GTK_WIDGET (row);
}

static void
ide_run_button_clear (IdeRunButton *self)
{
  g_assert (IDE_IS_RUN_BUTTON (self));

  gtk_container_foreach (GTK_CONTAINER (self->list_box), (GtkCallback)gtk_widget_destroy, NULL);
}

static void
ide_run_button_handler_set (IdeRunButton  *self,
                            GParamSpec    *pspec,
                            IdeRunManager *run_manager)
{
  const GList *list;
  const GList *iter;
  const gchar *handler;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  handler = ide_run_manager_get_handler (run_manager);
  list = _ide_run_manager_get_handlers (run_manager);

  for (iter = list; iter; iter = iter->next)
    {
      const IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, handler) == 0)
        {
          g_object_set (self->button_image,
                        "icon-name", info->icon_name,
                        NULL);
          break;
        }
    }
}

static void
ide_run_button_load (IdeRunButton *self,
                     IdeContext   *context)
{
  IdeRunManager *run_manager;
  const GList *list;
  const GList *iter;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (IDE_IS_CONTEXT (context));

  run_manager = ide_context_get_run_manager (context);
  list = _ide_run_manager_get_handlers (run_manager);

  for (iter = list; iter; iter = iter->next)
    {
      const IdeRunHandlerInfo *info = iter->data;
      GtkWidget *row;

      row = create_row (info, self);

      gtk_container_add (GTK_CONTAINER (self->list_box), row);
    }

  g_object_bind_property (run_manager, "busy", self->button, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property (run_manager, "busy", self->stop_button, "visible",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (run_manager,
                           "notify::handler",
                           G_CALLBACK (ide_run_button_handler_set),
                           self,
                           G_CONNECT_SWAPPED);

  ide_run_button_handler_set (self, NULL, run_manager);
}

static void
ide_run_button_row_activated (IdeRunButton  *self,
                              GtkListBoxRow *row,
                              GtkListBox    *list_box)
{
  IdeContext *context;
  const gchar *id;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  context = ide_widget_get_context (GTK_WIDGET (self));
  id = g_object_get_data (G_OBJECT (row), "IDE_RUN_HANDLER_ID");

  if (id != NULL && context != NULL)
    {
      IdeRunManager *run_manager;

      /* First change the run action to the selected handler. */
      run_manager = ide_context_get_run_manager (context);
      ide_run_manager_set_handler (run_manager, id);
      gtk_popover_popdown (self->popover);

      /* Now run the action */
      dzl_gtk_widget_action (GTK_WIDGET (self), "run-manager", "run-with-handler", g_variant_new_string (id));
    }
}

static void
ide_run_button_context_set (GtkWidget  *widget,
                            IdeContext *context)
{
  IdeRunButton *self = (IdeRunButton *)widget;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  ide_run_button_clear (self);

  if (context != NULL)
    ide_run_button_load (self, context);
}

static void
ide_run_button_query_tooltip (IdeRunButton *self,
                              gint          x,
                              gint          y,
                              gboolean      keyboard_tooltip,
                              GtkTooltip   *tooltip,
                              GtkButton    *button)
{
  IdeRunManager *run_manager;
  const GList *list;
  const GList *iter;
  const gchar *handler;
  IdeContext *context;

  g_assert (IDE_IS_RUN_BUTTON (self));
  g_assert (GTK_IS_TOOLTIP (tooltip));
  g_assert (GTK_IS_BUTTON (button));

  context = ide_widget_get_context (GTK_WIDGET (self));
  run_manager = ide_context_get_run_manager (context);
  handler = ide_run_manager_get_handler (run_manager);
  list = _ide_run_manager_get_handlers (run_manager);

  for (iter = list; iter; iter = iter->next)
    {
      const IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, handler) == 0)
        {
          g_object_set (self->run_shortcut,
                        "accelerator", info->accel,
                        "title", info->title,
                        "visible", TRUE,
                        NULL);
          gtk_tooltip_set_custom (tooltip, GTK_WIDGET (self->run_shortcut));
          break;
        }
    }
}

static void
ide_run_button_class_init (IdeRunButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-run-button.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, accel_size_group);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, button);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, button_image);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, menu_button);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, popover);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, run_shortcut);
  gtk_widget_class_bind_template_child (widget_class, IdeRunButton, stop_button);
}

static void
ide_run_button_init (IdeRunButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (ide_run_button_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->button,
                           "query-tooltip",
                           G_CALLBACK (ide_run_button_query_tooltip),
                           self,
                           G_CONNECT_SWAPPED);

  ide_widget_set_context_handler (self, ide_run_button_context_set);
}
