/* gb-command-bar.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-animation.h"
#include "gb-command.h"
#include "gb-command-bar.h"
#include "gb-command-bar-item.h"
#include "gb-command-manager.h"
#include "gb-string.h"

struct _GbCommandBarPrivate
{
  GtkSizeGroup      *result_size_group;
  GtkEntry          *entry;
  GtkListBox        *list_box;
  GtkScrolledWindow *scroller;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandBar, gb_command_bar, GTK_TYPE_REVEALER)

GtkWidget *
gb_command_bar_new (void)
{
  return g_object_new (GB_TYPE_COMMAND_BAR, NULL);
}

/**
 * gb_command_bar_hide:
 * @bar: A #GbCommandBar
 *
 * Hides the command bar in an animated fashion.
 */
void
gb_command_bar_hide (GbCommandBar *bar)
{
  g_return_if_fail (GB_IS_COMMAND_BAR (bar));

  gtk_revealer_set_reveal_child (GTK_REVEALER (bar), FALSE);
}

/**
 * gb_command_bar_show:
 * @bar: A #GbCommandBar
 *
 * Shows the command bar in an animated fashion.
 */
void
gb_command_bar_show (GbCommandBar *bar)
{
  g_return_if_fail (GB_IS_COMMAND_BAR (bar));

  gtk_revealer_set_reveal_child (GTK_REVEALER (bar), TRUE);
  gtk_entry_set_text (bar->priv->entry, "");
  gtk_widget_grab_focus (GTK_WIDGET (bar->priv->entry));
}

static void
gb_command_bar_push_result (GbCommandBar    *bar,
                            GbCommandResult *result)
{
  GtkAdjustment *vadj;
  GdkFrameClock *frame_clock;
  GtkWidget *item;
  GtkWidget *result_widget;
  gdouble upper;

  g_return_if_fail (GB_IS_COMMAND_BAR (bar));
  g_return_if_fail (GB_IS_COMMAND_RESULT (result));

  item = g_object_new (GB_TYPE_COMMAND_BAR_ITEM,
                       "result", result,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (bar->priv->list_box), item);

  result_widget = gb_command_bar_item_get_result (GB_COMMAND_BAR_ITEM (item));
  gtk_size_group_add_widget (bar->priv->result_size_group, result_widget);

  vadj = gtk_list_box_get_adjustment (bar->priv->list_box);
  upper = gtk_adjustment_get_upper (vadj);
  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (bar->priv->list_box));

  gb_object_animate (vadj,
                     GB_ANIMATION_EASE_IN_CUBIC,
                     250,
                     frame_clock,
                     "value", upper,
                     NULL);
}

static void
gb_command_bar_on_entry_activate (GbCommandBar *bar,
                                  GtkEntry     *entry)
{
  GbWorkbench *workbench = NULL;
  const gchar *text;

  g_return_if_fail (GB_IS_COMMAND_BAR (bar));
  g_return_if_fail (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);

  workbench = GB_WORKBENCH (gtk_widget_get_toplevel (GTK_WIDGET (bar)));
  if (!workbench)
    return;

  if (!gb_str_empty0 (text))
    {
      GbCommandManager *manager;
      GbCommandResult *result = NULL;
      GbCommand *command = NULL;

      manager = gb_workbench_get_command_manager (workbench);
      command = gb_command_manager_lookup (manager, text);

      if (command)
        {
          result = gb_command_execute (command);
          if (result)
            gb_command_bar_push_result (bar, result);
        }
      else
        {
          gchar *errmsg;

          errmsg = g_strdup_printf (_("Command not found: %s"), text);
          result = g_object_new (GB_TYPE_COMMAND_RESULT,
                                 "is-error", TRUE,
                                 "command-text", errmsg,
                                 NULL);
          gb_command_bar_push_result (bar, result);
          g_object_unref (result);
          g_free (errmsg);
        }

      g_clear_object (&result);
      g_clear_object (&command);
    }
  else
    gb_command_bar_hide (bar);

  gtk_entry_set_text (bar->priv->entry, "");
}

static gboolean
gb_command_bar_on_entry_focus_out_event (GbCommandBar *bar,
                                         GdkEventKey  *event,
                                         GtkEntry     *entry)
{
  g_return_val_if_fail (GB_IS_COMMAND_BAR (bar), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  gb_command_bar_hide (bar);

  return GDK_EVENT_PROPAGATE;
}

static void
gb_command_bar_grab_focus (GtkWidget *widget)
{
  GbCommandBar *bar = (GbCommandBar *)widget;

  g_return_if_fail (GB_IS_COMMAND_BAR (bar));

  gtk_widget_grab_focus (GTK_WIDGET (bar->priv->entry));
}

static gboolean
gb_command_bar_on_entry_key_press_event (GbCommandBar *bar,
                                         GdkEventKey  *event,
                                         GtkEntry     *entry)
{
  g_return_val_if_fail (GB_IS_COMMAND_BAR (bar), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  if (event->keyval == GDK_KEY_Escape)
    {
      gb_command_bar_hide (bar);
      return TRUE;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
update_header_func (GtkListBoxRow *row,
                    GtkListBoxRow *before,
                    gpointer       user_data)
{
  if (before)
    {
      GtkWidget *sep;

      sep = g_object_new (GTK_TYPE_SEPARATOR,
                          "orientation", GTK_ORIENTATION_HORIZONTAL,
                          "visible", TRUE,
                          NULL);
      gtk_list_box_row_set_header (row, sep);
    }
}

static void
gb_command_bar_constructed (GObject *object)
{
  GbCommandBar *bar = (GbCommandBar *)object;
  GtkWidget *placeholder;

  G_OBJECT_CLASS (gb_command_bar_parent_class)->constructed (object);

  placeholder = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "label", _("Use the entry below to execute a command"),
                              NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (placeholder),
                               "gb-command-bar-placeholder");
  gtk_list_box_set_placeholder (bar->priv->list_box, placeholder);

  g_signal_connect_object (bar->priv->entry,
                           "activate",
                           G_CALLBACK (gb_command_bar_on_entry_activate),
                           bar,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (bar->priv->entry,
                           "focus-out-event",
                           G_CALLBACK (gb_command_bar_on_entry_focus_out_event),
                           bar,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (bar->priv->entry,
                           "key-press-event",
                           G_CALLBACK (gb_command_bar_on_entry_key_press_event),
                           bar,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_header_func (bar->priv->list_box, update_header_func,
                                NULL, NULL);
}

static void
gb_command_bar_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_command_bar_parent_class)->finalize (object);
}

static void
gb_command_bar_class_init (GbCommandBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_command_bar_constructed;
  object_class->finalize = gb_command_bar_finalize;

  widget_class->grab_focus = gb_command_bar_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-command-bar.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, entry);
  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, list_box);
  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, scroller);
  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, result_size_group);
}

static void
gb_command_bar_init (GbCommandBar *self)
{
  self->priv = gb_command_bar_get_instance_private (self);
  gtk_widget_init_template (GTK_WIDGET (self));
}
