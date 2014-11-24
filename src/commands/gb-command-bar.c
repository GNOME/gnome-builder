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
  GtkScrolledWindow *completion_scroller;
  GtkFlowBox        *flow_box;

  gchar             *last_completion;

  GQueue            *history;
  GList             *history_current;
  gchar             *saved_text;
  int                saved_position;
  gboolean           saved_position_valid;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandBar, gb_command_bar, GTK_TYPE_REVEALER)

#define HISTORY_LENGTH 30

enum {
  COMPLETE,
  MOVE_HISTORY,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

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

  gtk_widget_hide (GTK_WIDGET (bar->priv->completion_scroller));

  bar->priv->history_current = NULL;
  g_clear_pointer (&bar->priv->saved_text, g_free);
  bar->priv->saved_position_valid = FALSE;

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

  gtk_widget_hide (GTK_WIDGET (bar->priv->completion_scroller));

  if (!gb_str_empty0 (text))
    {
      GbCommandManager *manager;
      GbCommandResult *result = NULL;
      GbCommand *command = NULL;

      g_queue_push_head (bar->priv->history, g_strdup (text));
      g_free (g_queue_pop_nth (bar->priv->history, HISTORY_LENGTH));

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

  bar->priv->history_current = NULL;
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

#define N_COMPLETION_COLUMS 3
#define N_UNSCROLLED_COMPLETION_ROWS 4

static void
gb_command_bar_complete (GbCommandBar *bar)
{
  GtkEditable *editable = GTK_EDITABLE (bar->priv->entry);
  GtkWidget *viewport = gtk_bin_get_child (GTK_BIN (bar->priv->completion_scroller));
  GbWorkbench *workbench;
  GbCommandManager *manager;
  gchar **completions;
  int pos, i;
  gchar *current_prefix, *expanded_prefix;

  workbench = GB_WORKBENCH (gtk_widget_get_toplevel (GTK_WIDGET (bar)));
  if (!workbench)
    return;

  pos = gtk_editable_get_position (editable);
  current_prefix = gtk_editable_get_chars (editable, 0, pos);

  /* If we complete again with the same data we scroll the completion instead */
  if (gtk_widget_is_visible (GTK_WIDGET (bar->priv->completion_scroller)) &&
      bar->priv->last_completion != NULL &&
      strcmp (bar->priv->last_completion, current_prefix) == 0)
    {
      GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment (bar->priv->completion_scroller);
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
      g_clear_pointer (&bar->priv->last_completion, g_free);

      manager = gb_workbench_get_command_manager (workbench);
      completions = gb_command_manager_complete (manager, current_prefix);

      expanded_prefix = find_longest_common_prefix (completions);

      if (strlen (expanded_prefix) > strlen (current_prefix))
        {
          gtk_widget_hide (GTK_WIDGET (bar->priv->completion_scroller));
          gtk_editable_insert_text (editable, expanded_prefix + strlen (current_prefix), -1, &pos);
          gtk_editable_set_position (editable, pos);
        }
      else if (g_strv_length (completions) > 1)
        {
          gint wrapped_height = 0;
          bar->priv->last_completion = g_strdup (current_prefix);

          gtk_widget_show (GTK_WIDGET (bar->priv->completion_scroller));
          gtk_container_foreach (GTK_CONTAINER (bar->priv->flow_box),
                                 (GtkCallback)gtk_widget_destroy, NULL);

          gtk_flow_box_set_min_children_per_line (bar->priv->flow_box, N_COMPLETION_COLUMS);
          gtk_flow_box_set_max_children_per_line (bar->priv->flow_box, N_COMPLETION_COLUMS);

          for (i = 0; completions[i] != NULL; i++)
            {
              GtkWidget *label;
              char *s;

              label = gtk_label_new ("");
              s = g_strdup_printf ("<b>%s</b>%s", current_prefix, completions[i] + strlen (current_prefix));
              gtk_label_set_markup (GTK_LABEL (label), s);
              g_free (s);

              gtk_container_add (GTK_CONTAINER (bar->priv->flow_box), label);
              gtk_widget_show (label);

              if (i == N_COMPLETION_COLUMS * N_UNSCROLLED_COMPLETION_ROWS - 1)
                gtk_widget_get_preferred_height (GTK_WIDGET (bar->priv->flow_box), &wrapped_height, NULL);
            }

          if (i < N_COMPLETION_COLUMS * N_UNSCROLLED_COMPLETION_ROWS)
            {
              gtk_widget_set_size_request (GTK_WIDGET (bar->priv->completion_scroller), -1, -1);
              gtk_scrolled_window_set_policy (bar->priv->completion_scroller,
                                              GTK_POLICY_NEVER, GTK_POLICY_NEVER);
            }
          else
            {
              gtk_widget_set_size_request (GTK_WIDGET (bar->priv->completion_scroller), -1, wrapped_height);
              gtk_scrolled_window_set_policy (bar->priv->completion_scroller,
                                              GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
            }
        }
      else
        gtk_widget_hide (GTK_WIDGET (bar->priv->completion_scroller));

      g_free (expanded_prefix);
      g_strfreev (completions);
    }

  g_free (current_prefix);
}

static void
gb_command_bar_move_history (GbCommandBar *bar,
                             GtkDirectionType dir)
{
  GList *l;

  switch (dir)
    {
    case GTK_DIR_UP:
      l = bar->priv->history_current;
      if (l == NULL)
        l = bar->priv->history->head;
      else
        l = l->next;

      if (l == NULL)
        {
          gtk_widget_error_bell (GTK_WIDGET (bar));
          return;
        }

      break;

    case GTK_DIR_DOWN:

      l = bar->priv->history_current;
      if (l == NULL)
        {
          gtk_widget_error_bell (GTK_WIDGET (bar));
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

  if (bar->priv->history_current == NULL)
    {
      g_clear_pointer (&bar->priv->saved_text, g_free);
      bar->priv->saved_text = g_strdup (gtk_entry_get_text (bar->priv->entry));
    }
  bar->priv->history_current = l;

  if (!bar->priv->saved_position_valid)
    {
      bar->priv->saved_position = gtk_editable_get_position (GTK_EDITABLE (bar->priv->entry));
      if (bar->priv->saved_position == gtk_entry_get_text_length (bar->priv->entry))
        bar->priv->saved_position = -1;
    }

  if (l == NULL)
    gtk_entry_set_text (bar->priv->entry, bar->priv->saved_text ? bar->priv->saved_text : "");
  else
    gtk_entry_set_text (bar->priv->entry, l->data);

  gtk_editable_set_position (GTK_EDITABLE (bar->priv->entry), bar->priv->saved_position);
  bar->priv->saved_position_valid = TRUE;
}

static void
gb_command_bar_on_entry_cursor_changed (GbCommandBar *bar)
{
  bar->priv->saved_position_valid = FALSE;
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

  g_signal_connect_object (bar->priv->entry,
                           "notify::cursor-position",
                           G_CALLBACK (gb_command_bar_on_entry_cursor_changed),
                           bar,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_header_func (bar->priv->list_box, update_header_func,
                                NULL, NULL);
}

static void
gb_command_bar_finalize (GObject *object)
{
  GbCommandBar *bar = (GbCommandBar *)object;

  g_clear_pointer (&bar->priv->last_completion, g_free);
  g_clear_pointer (&bar->priv->saved_text, g_free);
  g_queue_free_full (bar->priv->history, g_free);

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

  klass->complete = gb_command_bar_complete;
  klass->move_history = gb_command_bar_move_history;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-command-bar.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, entry);
  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, list_box);
  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, scroller);
  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, result_size_group);
  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, completion_scroller);
  gtk_widget_class_bind_template_child_private (widget_class, GbCommandBar, flow_box);


  /**
   * GbCommandBar::complete:
   * @bar: the object which received the signal.
   */
  signals[COMPLETE] =
    g_signal_new ("complete",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbCommandBarClass, complete),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /**
   * GbCommandBar::move-history:
   * @bar: the object which received the signal.
   * @direction: direction to move
   */
  signals[MOVE_HISTORY] =
    g_signal_new ("move-history",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbCommandBarClass, move_history),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
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
}

static void
gb_command_bar_init (GbCommandBar *self)
{
  self->priv = gb_command_bar_get_instance_private (self);
  gtk_widget_init_template (GTK_WIDGET (self));
  self->priv->history = g_queue_new ();
}
