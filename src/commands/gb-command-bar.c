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
#include <ide.h>

#include "gb-command.h"
#include "gb-command-bar.h"
#include "gb-command-bar-item.h"
#include "gb-command-manager.h"
#include "gb-glib.h"
#include "gb-string.h"
#include "gb-view-stack.h"
#include "gb-widget.h"
#include "gb-workbench.h"

struct _GbCommandBar
{
  GtkRevealer        parent_instance;

  GtkSizeGroup      *result_size_group;
  GtkEntry          *entry;
  GtkListBox        *list_box;
  GtkScrolledWindow *scroller;
  GtkScrolledWindow *completion_scroller;
  GtkFlowBox        *flow_box;

  gchar             *last_completion;
  GtkWidget         *last_focus;

  GQueue            *history;
  GList             *history_current;
  gchar             *saved_text;
  int                saved_position;
  gboolean           saved_position_valid;
};

G_DEFINE_TYPE (GbCommandBar, gb_command_bar, GTK_TYPE_REVEALER)

#define HISTORY_LENGTH 30

enum {
  COMPLETE,
  MOVE_HISTORY,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

GtkWidget *
gb_command_bar_new (void)
{
  return g_object_new (GB_TYPE_COMMAND_BAR, NULL);
}

static GtkWidget *
find_alternate_focus (GtkWidget *focus)
{
  GtkWidget *parent;

  g_assert (GTK_IS_WIDGET (focus));

  /*
   * If this widget is in a stack, it may not be the GtkStack:visible-child anymore. If so,
   * we want to avoid refocusing this widget, but instead focus the new stack child.
   */

  for (parent = gtk_widget_get_parent (focus);
       parent && !GTK_IS_STACK (parent);
       parent = gtk_widget_get_parent (parent))
    { /* Do Nothing */ }

  if ((parent != NULL) && GTK_IS_STACK (parent))
    {
      GtkWidget *visible_child;

      visible_child = gtk_stack_get_visible_child (GTK_STACK (parent));

      if (!gtk_widget_is_ancestor (focus, visible_child))
        return visible_child;
    }

  return focus;
}

/**
 * gb_command_bar_hide:
 * @bar: A #GbCommandBar
 *
 * Hides the command bar in an animated fashion.
 */
void
gb_command_bar_hide (GbCommandBar *self)
{
  GbWorkbench *workbench;
  GtkWidget *focus;

  g_return_if_fail (GB_IS_COMMAND_BAR (self));

  if (!gtk_revealer_get_reveal_child (GTK_REVEALER (self)))
    return;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);

  workbench = gb_widget_get_workbench (GTK_WIDGET (self));
  if ((workbench == NULL) || gb_workbench_get_closing (workbench))
    return;

  if (self->last_focus)
    focus = find_alternate_focus (self->last_focus);
  else
    focus = GTK_WIDGET (workbench);

  gtk_widget_grab_focus (focus);
}

static void
gb_command_bar_set_last_focus (GbCommandBar *self,
                               GtkWidget    *widget)
{
  g_return_if_fail (GB_IS_COMMAND_BAR (self));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  ide_set_weak_pointer (&self->last_focus, widget);
}

/**
 * gb_command_bar_show:
 * @bar: A #GbCommandBar
 *
 * Shows the command bar in an animated fashion.
 */
void
gb_command_bar_show (GbCommandBar *self)
{
  GtkWidget *toplevel;
  GtkWidget *focus;

  g_return_if_fail (GB_IS_COMMAND_BAR (self));

  if (gtk_revealer_get_reveal_child (GTK_REVEALER (self)))
    return;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  focus = gtk_window_get_focus (GTK_WINDOW (toplevel));
  gb_command_bar_set_last_focus (self, focus);

  gtk_widget_hide (GTK_WIDGET (self->completion_scroller));

  self->history_current = NULL;
  g_clear_pointer (&self->saved_text, g_free);
  self->saved_position_valid = FALSE;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
  gtk_entry_set_text (self->entry, "");
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
gb_command_bar_push_result (GbCommandBar    *self,
                            GbCommandResult *result)
{
  GtkAdjustment *vadj;
  GdkFrameClock *frame_clock;
  GtkWidget *item;
  GtkWidget *result_widget;
  gdouble upper;

  g_return_if_fail (GB_IS_COMMAND_BAR (self));
  g_return_if_fail (GB_IS_COMMAND_RESULT (result));

  item = g_object_new (GB_TYPE_COMMAND_BAR_ITEM,
                       "result", result,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (self->list_box), item);

  result_widget = gb_command_bar_item_get_result (GB_COMMAND_BAR_ITEM (item));
  gtk_size_group_add_widget (self->result_size_group, result_widget);

  vadj = gtk_list_box_get_adjustment (self->list_box);
  upper = gtk_adjustment_get_upper (vadj);
  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (self->list_box));

  ide_object_animate (vadj,
                      IDE_ANIMATION_EASE_IN_CUBIC,
                      250,
                      frame_clock,
                      "value", upper,
                      NULL);
}

static void
gb_command_bar_on_entry_activate (GbCommandBar *self,
                                  GtkEntry     *entry)
{
  GbWorkbench *workbench = NULL;
  const gchar *text;

  g_assert (GB_IS_COMMAND_BAR (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);

  workbench = GB_WORKBENCH (gtk_widget_get_toplevel (GTK_WIDGET (self)));
  if (!workbench)
    return;

  gtk_widget_hide (GTK_WIDGET (self->completion_scroller));

  if (!gb_str_empty0 (text))
    {
      GbCommandManager *manager;
      GbCommandResult *result = NULL;
      GbCommand *command = NULL;

      g_queue_push_head (self->history, g_strdup (text));
      g_free (g_queue_pop_nth (self->history, HISTORY_LENGTH));

      manager = gb_workbench_get_command_manager (workbench);
      command = gb_command_manager_lookup (manager, text);

      if (command)
        {
          result = gb_command_execute (command);

          /* if we got a result item, keep the bar open for observing it.
           * (However, we currently have the result area hidden, until it is
           * ported to Popover.) Otherwise, just hide the command bar.
           */
          if (result)
            gb_command_bar_push_result (self, result);
          else
            gb_command_bar_hide (self);
        }
      else
        {
          gchar *errmsg;

          errmsg = g_strdup_printf (_("Command not found: %s"), text);
          result = g_object_new (GB_TYPE_COMMAND_RESULT,
                                 "is-error", TRUE,
                                 "command-text", errmsg,
                                 NULL);
          gb_command_bar_push_result (self, result);
          g_object_unref (result);
          g_free (errmsg);
        }

      g_clear_object (&result);
      g_clear_object (&command);
    }
  else
    gb_command_bar_hide (self);

  self->history_current = NULL;
  gtk_entry_set_text (self->entry, "");
}

static gboolean
gb_command_bar_on_entry_focus_out_event (GbCommandBar *self,
                                         GdkEventKey  *event,
                                         GtkEntry     *entry)
{
  g_assert (GB_IS_COMMAND_BAR (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_ENTRY (entry));

  gb_command_bar_hide (self);

  return GDK_EVENT_PROPAGATE;
}

static void
gb_command_bar_grab_focus (GtkWidget *widget)
{
  GbCommandBar *self = (GbCommandBar *)widget;

  g_assert (GB_IS_COMMAND_BAR (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static gchar *
find_longest_common_prefix (gchar **strv)
{
  gchar *lcp = NULL;
  gchar *lcp_end = NULL;
  int i;

  for (i = 0; strv[i] != NULL; i++)
    {
      gchar *str = strv[i];
      if (lcp == NULL)
        {
          lcp = str;
          lcp_end = str + strlen (str);
        }
      else
        {
          gchar *tmp = lcp;

          while (tmp < lcp_end  && *str != 0 && *tmp == *str)
            {
              str++;
              tmp++;
            }

          lcp_end = tmp;
        }
    }

  if (lcp == NULL)
    return g_strdup ("");

  return g_strndup (lcp, lcp_end - lcp);
}

#define MIN_COMPLETION_COLUMS 3
#define N_UNSCROLLED_COMPLETION_ROWS 4

static void
gb_command_bar_complete (GbCommandBar *self)
{
  GtkEditable *editable = GTK_EDITABLE (self->entry);
  GtkWidget *viewport = gtk_bin_get_child (GTK_BIN (self->completion_scroller));
  GbWorkbench *workbench;
  GbCommandManager *manager;
  gchar **completions;
  int pos, i;
  gchar *current_prefix, *expanded_prefix;

  workbench = GB_WORKBENCH (gtk_widget_get_toplevel (GTK_WIDGET (self)));
  if (!workbench)
    return;

  pos = gtk_editable_get_position (editable);
  current_prefix = gtk_editable_get_chars (editable, 0, pos);

  /* If we complete again with the same data we scroll the completion instead */
  if (gtk_widget_is_visible (GTK_WIDGET (self->completion_scroller)) &&
      self->last_completion != NULL &&
      strcmp (self->last_completion, current_prefix) == 0)
    {
      GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment (self->completion_scroller);
      int viewport_height = gtk_widget_get_allocated_height (viewport);
      int y = gtk_adjustment_get_value (vadj);
      int max = gtk_adjustment_get_upper (vadj);

      y += viewport_height;
      if (y >= max)
        y = 0;

      gtk_adjustment_set_value (vadj, y);
    }
  else
    {
      g_clear_pointer (&self->last_completion, g_free);

      manager = gb_workbench_get_command_manager (workbench);
      completions = gb_command_manager_complete (manager, current_prefix);

      expanded_prefix = find_longest_common_prefix (completions);

      if (strlen (expanded_prefix) > strlen (current_prefix))
        {
          gtk_widget_hide (GTK_WIDGET (self->completion_scroller));
          gtk_editable_insert_text (editable, expanded_prefix + strlen (current_prefix), -1, &pos);
          gtk_editable_set_position (editable, pos);
        }
      else if (g_strv_length (completions) > 1)
        {
          gint wrapped_height = 0;
          self->last_completion = g_strdup (current_prefix);

          gtk_widget_show (GTK_WIDGET (self->completion_scroller));
          gtk_container_foreach (GTK_CONTAINER (self->flow_box),
                                 (GtkCallback)gtk_widget_destroy, NULL);

          gtk_flow_box_set_min_children_per_line (self->flow_box, MIN_COMPLETION_COLUMS);

          for (i = 0; completions[i] != NULL; i++)
            {
              GtkWidget *label;
              char *s;

              label = gtk_label_new ("");
              s = g_strdup_printf ("<b>%s</b>%s", current_prefix, completions[i] + strlen (current_prefix));
              gtk_label_set_markup (GTK_LABEL (label), s);
              gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
              g_free (s);

              gtk_container_add (GTK_CONTAINER (self->flow_box), label);
              gtk_widget_show (label);

              if (i == MIN_COMPLETION_COLUMS * N_UNSCROLLED_COMPLETION_ROWS - 1)
                gtk_widget_get_preferred_height (GTK_WIDGET (self->flow_box), &wrapped_height, NULL);
            }

          if (i < MIN_COMPLETION_COLUMS * N_UNSCROLLED_COMPLETION_ROWS)
            {
              gtk_widget_set_size_request (GTK_WIDGET (self->completion_scroller), -1, -1);
              gtk_scrolled_window_set_policy (self->completion_scroller,
                                              GTK_POLICY_NEVER, GTK_POLICY_NEVER);
            }
          else
            {
              gtk_widget_set_size_request (GTK_WIDGET (self->completion_scroller), -1, wrapped_height);
              gtk_scrolled_window_set_policy (self->completion_scroller,
                                              GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
            }
        }
      else
        gtk_widget_hide (GTK_WIDGET (self->completion_scroller));

      g_free (expanded_prefix);
      g_strfreev (completions);
    }

  g_free (current_prefix);
}

static void
gb_command_bar_move_history (GbCommandBar     *self,
                             GtkDirectionType  dir)
{
  GList *l;

  switch (dir)
    {
    case GTK_DIR_UP:
      l = self->history_current;
      if (l == NULL)
        l = self->history->head;
      else
        l = l->next;

      if (l == NULL)
        {
          gtk_widget_error_bell (GTK_WIDGET (self));
          return;
        }

      break;

    case GTK_DIR_DOWN:

      l = self->history_current;
      if (l == NULL)
        {
          gtk_widget_error_bell (GTK_WIDGET (self));
          return;
        }

      l = l->prev;

      break;

    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
    default:
      return;
    }

  if (self->history_current == NULL)
    {
      g_clear_pointer (&self->saved_text, g_free);
      self->saved_text = g_strdup (gtk_entry_get_text (self->entry));
    }
  self->history_current = l;

  if (!self->saved_position_valid)
    {
      self->saved_position = gtk_editable_get_position (GTK_EDITABLE (self->entry));
      if (self->saved_position == gtk_entry_get_text_length (self->entry))
        self->saved_position = -1;
    }

  if (l == NULL)
    gtk_entry_set_text (self->entry, self->saved_text ? self->saved_text : "");
  else
    gtk_entry_set_text (self->entry, l->data);

  gtk_editable_set_position (GTK_EDITABLE (self->entry), self->saved_position);
  self->saved_position_valid = TRUE;
}

static void
gb_command_bar_on_entry_cursor_changed (GbCommandBar *self)
{
  g_assert (GB_IS_COMMAND_BAR (self));

  self->saved_position_valid = FALSE;
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
  GbCommandBar *self = (GbCommandBar *)object;
  GtkWidget *placeholder;

  G_OBJECT_CLASS (gb_command_bar_parent_class)->constructed (object);

  placeholder = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "label", _("Use the entry below to execute a command"),
                              NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (placeholder),
                               "gb-command-bar-placeholder");
  gtk_list_box_set_placeholder (self->list_box, placeholder);

  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (gb_command_bar_on_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "focus-out-event",
                           G_CALLBACK (gb_command_bar_on_entry_focus_out_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "key-press-event",
                           G_CALLBACK (gb_command_bar_on_entry_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "notify::cursor-position",
                           G_CALLBACK (gb_command_bar_on_entry_cursor_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_header_func (self->list_box, update_header_func,
                                NULL, NULL);
}

static void
gb_command_bar_finalize (GObject *object)
{
  GbCommandBar *self = (GbCommandBar *)object;

  g_clear_pointer (&self->last_completion, g_free);
  g_clear_pointer (&self->saved_text, g_free);
  g_queue_free_full (self->history, g_free);
  gb_clear_weak_pointer (&self->last_focus);

  G_OBJECT_CLASS (gb_command_bar_parent_class)->finalize (object);
}

static void
gb_command_bar_class_init (GbCommandBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->constructed = gb_command_bar_constructed;
  object_class->finalize = gb_command_bar_finalize;

  widget_class->grab_focus = gb_command_bar_grab_focus;

  /**
   * GbCommandBar::complete:
   * @bar: the object which received the signal.
   */
  gSignals [COMPLETE] =
    g_signal_new_class_handler ("complete",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gb_command_bar_complete),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  /**
   * GbCommandBar::move-history:
   * @bar: the object which received the signal.
   * @direction: direction to move
   */
  gSignals [MOVE_HISTORY] =
    g_signal_new_class_handler ("move-history",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gb_command_bar_move_history),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                1,
                                GTK_TYPE_DIRECTION_TYPE);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Tab, 0,
                                "complete", 0);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Up, 0,
                                "move-history", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_UP);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_Down, 0,
                                "move-history", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_DOWN);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-command-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, entry);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, scroller);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, result_size_group);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, completion_scroller);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, flow_box);
}

static void
gb_command_bar_init (GbCommandBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->history = g_queue_new ();
}
