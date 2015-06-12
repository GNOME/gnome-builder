/* gb-terminal.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
#include <vte/vte.h>

#include "gb-terminal-view.h"
#include "gb-view.h"
#include "gb-widget.h"
#include "gb-workbench.h"

struct _GbTerminalView
{
  GbView       parent_instance;

  VteTerminal *terminal;

  guint        has_spawned : 1;
};

G_DEFINE_TYPE (GbTerminalView, gb_terminal_view, GB_TYPE_VIEW)

static const GdkRGBA solarized_palette[] =
{
  /*
   * Solarized palette (1.0.0beta2):
   * http://ethanschoonover.com/solarized
   */
  { 0.02745,  0.211764, 0.258823, 1 },
  { 0.862745, 0.196078, 0.184313, 1 },
  { 0.521568, 0.6,      0,        1 },
  { 0.709803, 0.537254, 0,        1 },
  { 0.149019, 0.545098, 0.823529, 1 },
  { 0.82745,  0.211764, 0.509803, 1 },
  { 0.164705, 0.631372, 0.596078, 1 },
  { 0.933333, 0.909803, 0.835294, 1 },
  { 0,        0.168627, 0.211764, 1 },
  { 0.796078, 0.294117, 0.086274, 1 },
  { 0.345098, 0.431372, 0.458823, 1 },
  { 0.396078, 0.482352, 0.513725, 1 },
  { 0.513725, 0.580392, 0.588235, 1 },
  { 0.423529, 0.443137, 0.768627, 1 },
  { 0.57647,  0.631372, 0.631372, 1 },
  { 0.992156, 0.964705, 0.890196, 1 },
};

static void
gb_terminal_respawn (GbTerminalView *self)
{
  g_autoptr(GPtrArray) args = NULL;
  g_autofree gchar *workpath = NULL;
  GtkWidget *toplevel;
  GError *error = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  GPid child_pid;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  vte_terminal_reset (self->terminal, TRUE, TRUE);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!GB_IS_WORKBENCH (toplevel))
    return;

  context = gb_workbench_get_context (GB_WORKBENCH (toplevel));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  workpath = g_file_get_path (workdir);

  args = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (args, vte_get_user_shell ());
  g_ptr_array_add (args, NULL);

  vte_terminal_spawn_sync (self->terminal,
                           VTE_PTY_DEFAULT | VTE_PTY_NO_LASTLOG | VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP,
                           workpath,
                           (gchar **)args->pdata,
                           NULL,
                           G_SPAWN_DEFAULT,
                           NULL,
                           NULL,
                           &child_pid,
                           NULL,
                           &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  vte_terminal_watch_child (self->terminal, child_pid);
}

static void
child_exited_cb (VteTerminal    *terminal,
                 gint            exit_status,
                 GbTerminalView *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  if (!gb_widget_activate_action (GTK_WIDGET (self), "view-stack", "close", NULL))
    {
      if (!gtk_widget_in_destruction (GTK_WIDGET (terminal)))
        gb_terminal_respawn (self);
    }
}

static void
gb_terminal_realize (GtkWidget *widget)
{
  GbTerminalView *self = (GbTerminalView *)widget;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  GTK_WIDGET_CLASS (gb_terminal_view_parent_class)->realize (widget);

  if (!self->has_spawned)
    {
      self->has_spawned = TRUE;
      gb_terminal_respawn (self);
    }
}

static void
size_allocate_cb (VteTerminal    *terminal,
                  GtkAllocation  *alloc,
                  GbTerminalView *self)
{
  glong width;
  glong height;
  glong columns;
  glong rows;

  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (alloc != NULL);
  g_assert (GB_IS_TERMINAL_VIEW (self));

  if ((alloc->width == 0) || (alloc->height == 0))
    return;

  width = vte_terminal_get_char_width (terminal);
  height = vte_terminal_get_char_height (terminal);

  if ((width == 0) || (height == 0))
    return;

  columns = alloc->width / width;
  rows = alloc->height / height;

  if ((columns < 2) || (rows < 2))
    return;

  vte_terminal_set_size (self->terminal, columns, rows);
}

static void
gb_terminal_get_preferred_width (GtkWidget *widget,
                                 gint      *min_width,
                                 gint      *nat_width)
{
  /*
   * Since we are placing the terminal in a GtkStack, we need
   * to fake the size a bit. Otherwise, GtkStack tries to keep the
   * widget at it's natural size (which prevents us from getting
   * appropriate size requests.
   */
  GTK_WIDGET_CLASS (gb_terminal_view_parent_class)->get_preferred_width (widget, min_width, nat_width);
  *nat_width = *min_width;
}

static void
gb_terminal_get_preferred_height (GtkWidget *widget,
                                  gint      *min_height,
                                  gint      *nat_height)
{
  /*
   * Since we are placing the terminal in a GtkStack, we need
   * to fake the size a bit. Otherwise, GtkStack tries to keep the
   * widget at it's natural size (which prevents us from getting
   * appropriate size requests.
   */
  GTK_WIDGET_CLASS (gb_terminal_view_parent_class)->get_preferred_height (widget, min_height, nat_height);
  *nat_height = *min_height;
}

static void
gb_terminal_set_needs_attention (GbTerminalView *self,
                                 gboolean        needs_attention)
{
  GtkWidget *parent;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (GTK_IS_STACK (parent) &&
      !gtk_widget_in_destruction (GTK_WIDGET (self)) &&
      !gtk_widget_in_destruction (GTK_WIDGET (self->terminal)) &&
      !gtk_widget_in_destruction (parent))
    {

      gtk_container_child_set (GTK_CONTAINER (parent), GTK_WIDGET (self),
                               "needs-attention", !!needs_attention,
                               NULL);
    }
}

static void
notification_received_cb (VteTerminal    *terminal,
                          const gchar    *summary,
                          const gchar    *body,
                          GbTerminalView *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  if (!gtk_widget_has_focus (GTK_WIDGET (terminal)))
    gb_terminal_set_needs_attention (self, TRUE);
}

static gboolean
focus_in_event_cb (VteTerminal    *terminal,
                   GdkEvent       *event,
                   GbTerminalView *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  gb_terminal_set_needs_attention (self, FALSE);

  return GDK_EVENT_PROPAGATE;
}

static const gchar *
gb_terminal_get_title (GbView *view)
{
  GbTerminalView *self = (GbTerminalView *)view;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  return vte_terminal_get_window_title (self->terminal);
}

static void
window_title_changed_cb (VteTerminal    *terminal,
                         GbTerminalView *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  g_object_notify (G_OBJECT (self), "title");
}

static void
style_context_changed (GtkStyleContext *style_context,
                       GbTerminalView  *self)
{
  GdkRGBA fg;
  GdkRGBA bg;

  g_assert (GTK_IS_STYLE_CONTEXT (style_context));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  gtk_style_context_get_color (style_context, GTK_STATE_FLAG_NORMAL, &fg);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_NORMAL, &bg);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (bg.alpha == 0.0)
    {
      gdk_rgba_parse (&bg, "#f6f7f8");
    }

  vte_terminal_set_colors (self->terminal, &fg, &bg,
                           solarized_palette,
                           G_N_ELEMENTS (solarized_palette));
}

static void
gb_terminal_grab_focus (GtkWidget *widget)
{
  GbTerminalView *self = (GbTerminalView *)widget;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->terminal));
}

static void
gb_terminal_view_class_init (GbTerminalViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbViewClass *view_class = GB_VIEW_CLASS (klass);

  widget_class->realize = gb_terminal_realize;
  widget_class->get_preferred_width = gb_terminal_get_preferred_width;
  widget_class->get_preferred_height = gb_terminal_get_preferred_height;
  widget_class->grab_focus = gb_terminal_grab_focus;

  view_class->get_title = gb_terminal_get_title;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/terminal/gb-terminal-view.ui");
  gtk_widget_class_bind_template_child (widget_class, GbTerminalView, terminal);

  g_type_ensure (VTE_TYPE_TERMINAL);
}

static void
gb_terminal_view_init (GbTerminalView *self)
{
  GtkStyleContext *style_context;
  GQuark quark;
  guint signal_id;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->terminal,
                           "size-allocate",
                           G_CALLBACK (size_allocate_cb),
                           self,
                           0);

  g_signal_connect_object (self->terminal,
                           "child-exited",
                           G_CALLBACK (child_exited_cb),
                           self,
                           0);

  g_signal_connect_object (self->terminal,
                           "focus-in-event",
                           G_CALLBACK (focus_in_event_cb),
                           self,
                           0);

  g_signal_connect_object (self->terminal,
                           "window-title-changed",
                           G_CALLBACK (window_title_changed_cb),
                           self,
                           0);

  if (g_signal_parse_name ("notification-received",
                           VTE_TYPE_TERMINAL,
                           &signal_id,
                           &quark,
                           FALSE))
    {
      g_signal_connect_object (self->terminal,
                               "notification-received",
                               G_CALLBACK (notification_received_cb),
                               self,
                               0);
    }

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  g_signal_connect_object (style_context,
                           "changed",
                           G_CALLBACK (style_context_changed),
                           self,
                           0);
  style_context_changed (style_context, self);
}
