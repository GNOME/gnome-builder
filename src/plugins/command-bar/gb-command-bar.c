/* gb-command-bar.c
 *
 * Copyright © 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-command-bar"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <libpeas/peas.h>

#include "gb-command.h"
#include "gb-command-bar.h"
#include "gb-command-gaction-provider.h"
#include "gb-command-manager.h"
#include "gb-command-vim-provider.h"

struct _GbCommandBar
{
  GtkRevealer        parent_instance;

  IdeWorkbench      *workbench;
  GbCommandManager  *command_manager;

  GSimpleAction     *show_action;

  GtkSizeGroup      *result_size_group;
  GtkEntry          *entry;
  GtkListBox        *list_box;
  GtkScrolledWindow *scroller;
  GtkScrolledWindow *completion_scroller;
  GtkFlowBox        *flow_box;
  GtkRevealer       *revealer;

  gchar             *last_completion;
  GtkWidget         *last_focus;

  GQueue            *history;
  GList             *history_current;
  gchar             *saved_text;
  int                saved_position;
  gboolean           saved_position_valid;
};

#define I_(s) g_intern_static_string(s)

static void workbench_addin_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbCommandBar, gb_command_bar, GTK_TYPE_REVEALER, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_init))

#define HISTORY_LENGTH 30

enum {
  COMPLETE,
  MOVE_HISTORY,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void
gb_command_bar_load (IdeWorkbenchAddin *addin,
                     IdeWorkbench      *workbench)
{
  GbCommandBar *self = (GbCommandBar *)addin;
  GbCommandProvider *provider;
  GtkWidget *box;

  g_assert (GB_IS_COMMAND_BAR (self));

  ide_set_weak_pointer (&self->workbench, workbench);

  provider = g_object_new (GB_TYPE_COMMAND_GACTION_PROVIDER,
                           "workbench", self->workbench,
                           NULL);
  gb_command_manager_add_provider (self->command_manager, provider);
  g_clear_object (&provider);

  provider = g_object_new (GB_TYPE_COMMAND_VIM_PROVIDER,
                           "workbench", self->workbench,
                           NULL);
  gb_command_manager_add_provider (self->command_manager, provider);
  g_clear_object (&provider);

  box = gtk_bin_get_child (GTK_BIN (self->workbench));
  gtk_overlay_add_overlay (GTK_OVERLAY (box), GTK_WIDGET (self));

  g_action_map_add_action (G_ACTION_MAP (self->workbench), G_ACTION (self->show_action));

  gtk_widget_show (GTK_WIDGET (self));
}

static void
gb_command_bar_unload (IdeWorkbenchAddin *addin,
                       IdeWorkbench      *workbench)
{
  GbCommandBar *self = (GbCommandBar *)addin;

  g_assert (GB_IS_COMMAND_BAR (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  /*
   * TODO: We can't rely on object lifecycle since we are a GtkWidget.
   *       This plugin should be changed to have a separate addin that
   *       then adds this widget to the workbench.
   */

  g_action_map_remove_action (G_ACTION_MAP (workbench), "show-command-bar");
  ide_clear_weak_pointer (&self->workbench);
}

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
 * @bar: a #GbCommandBar
 *
 * Hides the command bar in an animated fashion.
 */
void
gb_command_bar_hide (GbCommandBar *self)
{
  GtkWidget *focus;
  gboolean had_focus;

  g_return_if_fail (GB_IS_COMMAND_BAR (self));

  had_focus = gtk_widget_is_focus (GTK_WIDGET (self->entry));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);

  if (had_focus)
  {
    if (self->last_focus)
      focus = find_alternate_focus (self->last_focus);
    else
      focus = GTK_WIDGET (self->workbench);

    gtk_widget_grab_focus (focus);
  }
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
 * @bar: a #GbCommandBar
 *
 * Shows the command bar in an animated fashion.
 */
void
gb_command_bar_show (GbCommandBar *self)
{
  GtkWidget *focus;

  g_return_if_fail (GB_IS_COMMAND_BAR (self));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);

  focus = gtk_window_get_focus (GTK_WINDOW (self->workbench));
  gb_command_bar_set_last_focus (self, focus);

  gtk_widget_hide (GTK_WIDGET (self->completion_scroller));

  self->history_current = NULL;
  g_clear_pointer (&self->saved_text, g_free);
  self->saved_position_valid = FALSE;

  gtk_entry_set_text (self->entry, "");
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
gb_command_bar_push_result (GbCommandBar    *self,
                            GbCommandResult *result)
{
  /*
   * TODO: if we decide to keep results visible, add them to list here.
   */
}

static void
gb_command_bar_on_entry_activate (GbCommandBar *self,
                                  GtkEntry     *entry)
{
  const gchar *text;

  g_assert (GB_IS_COMMAND_BAR (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);

  gtk_widget_hide (GTK_WIDGET (self->completion_scroller));

  if (!ide_str_empty0 (text))
    {
      GbCommandResult *result = NULL;
      GbCommand *command = NULL;

      g_queue_push_head (self->history, g_strdup (text));
      g_free (g_queue_pop_nth (self->history, HISTORY_LENGTH));

      command = gb_command_manager_lookup (self->command_manager, text);

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
  gchar **completions;
  int pos, i;
  gchar *current_prefix, *expanded_prefix;

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

      completions = gb_command_manager_complete (self->command_manager, current_prefix);

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

              if (!g_str_has_prefix (completions[i], current_prefix))
                {
                  g_warning ("Provided completion does not contain '%s' as a prefix", current_prefix);
                  continue;
                }

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

static void
show_command_bar (GSimpleAction *action,
                  GVariant      *param,
                  GbCommandBar  *self)
{
  g_assert (GB_IS_COMMAND_BAR (self));

  gb_command_bar_show (self);
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

  ide_clear_weak_pointer (&self->workbench);

  g_clear_pointer (&self->last_completion, g_free);
  g_clear_pointer (&self->saved_text, g_free);
  g_queue_free_full (self->history, g_free);
  ide_clear_weak_pointer (&self->last_focus);

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

  /**
   * GbCommandBar::complete:
   * @bar: the object which received the signal.
   */
  signals [COMPLETE] =
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
  signals [MOVE_HISTORY] =
    g_signal_new_class_handler ("move-history",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gb_command_bar_move_history),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                1,
                                GTK_TYPE_DIRECTION_TYPE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/command-bar/gb-command-bar.ui");
  gtk_widget_class_set_css_name (widget_class, "commandbar");
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, entry);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, scroller);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, result_size_group);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, completion_scroller);
  gtk_widget_class_bind_template_child (widget_class, GbCommandBar, flow_box);
}

static const DzlShortcutEntry shortcuts[] = {
  { "org.gnome.builder.command-bar.show",
    0, NULL,
    NC_("shortcut window", "Workbench shortcuts"),
    NC_("shortcut window", "General"),
    NC_("shortcut window", "Command Bar") },
};

static void
gb_command_bar_init (GbCommandBar *self)
{
  DzlShortcutController *controller;

  self->history = g_queue_new ();
  self->command_manager = gb_command_manager_new ();

  self->show_action = g_simple_action_new ("show-command-bar", NULL);
  g_signal_connect_object (self->show_action,
                           "activate",
                           G_CALLBACK (show_command_bar),
                           self,
                           0);

  gtk_widget_init_template (GTK_WIDGET (self));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.command-bar.show"),
                                              I_("<Primary>Return"),
                                              DZL_SHORTCUT_PHASE_CAPTURE | DZL_SHORTCUT_PHASE_GLOBAL,
                                              "win.show-command-bar");

  dzl_shortcut_controller_add_command_signal (controller,
                                              I_("org.gnome.builder.command-bar.complete"),
                                              I_("Tab"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "complete", 0);

  dzl_shortcut_controller_add_command_signal (controller,
                                              I_("org.gnome.builder.command-bar.previous"),
                                              I_("Up"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "move-history", 1,
                                              GTK_TYPE_DIRECTION_TYPE, GTK_DIR_UP);

  dzl_shortcut_controller_add_command_signal (controller,
                                              I_("org.gnome.builder.command-bar.next"),
                                              I_("Down"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "move-history", 1,
                                              GTK_TYPE_DIRECTION_TYPE, GTK_DIR_DOWN);

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             shortcuts,
                                             G_N_ELEMENTS (shortcuts),
                                             GETTEXT_PACKAGE);

}

static void
workbench_addin_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gb_command_bar_load;
  iface->unload = gb_command_bar_unload;
}

void
gb_command_bar_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GB_TYPE_COMMAND_BAR);
}
