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

#include "gb-terminal.h"
#include "gb-workbench.h"

struct _GbTerminal
{
  GtkBin       parent_instance;

  VteTerminal *terminal;

  guint        has_spawned : 1;
};

G_DEFINE_TYPE (GbTerminal, gb_terminal, GTK_TYPE_BIN)

enum {
  PROP_0,
  LAST_PROP
};

#if 0
static GParamSpec *gParamSpecs [LAST_PROP];
#endif

static void
gb_terminal_respawn (GbTerminal *self)
{
  g_autoptr(GPtrArray) args = NULL;
  g_autofree gchar *workpath = NULL;
  GtkWidget *toplevel;
  GError *error = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  GPid child_pid;

  g_assert (GB_IS_TERMINAL (self));

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
child_exited_cb (VteTerminal *terminal,
                 gint         exit_status,
                 GbTerminal  *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL (self));

  if (!gtk_widget_in_destruction (GTK_WIDGET (terminal)))
    gb_terminal_respawn (self);
}

static void
gb_terminal_realize (GtkWidget *widget)
{
  GbTerminal *self = (GbTerminal *)widget;

  g_assert (GB_IS_TERMINAL (self));

  GTK_WIDGET_CLASS (gb_terminal_parent_class)->realize (widget);

  if (!self->has_spawned)
    {
      self->has_spawned = TRUE;
      gb_terminal_respawn (self);
    }
}

static void
size_allocate_cb (VteTerminal   *terminal,
                  GtkAllocation *alloc,
                  GbTerminal    *self)
{
  glong width;
  glong height;
  glong columns;
  glong rows;

  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (alloc != NULL);
  g_assert (GB_IS_TERMINAL (self));

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
  GTK_WIDGET_CLASS (gb_terminal_parent_class)->get_preferred_width (widget, min_width, nat_width);
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
  GTK_WIDGET_CLASS (gb_terminal_parent_class)->get_preferred_height (widget, min_height, nat_height);
  *nat_height = *min_height;
}

static void
gb_terminal_set_needs_attention (GbTerminal *self,
                                 gboolean    needs_attention)
{
  GtkWidget *parent;

  g_assert (GB_IS_TERMINAL (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (GTK_IS_STACK (parent))
    {
      gtk_container_child_set (GTK_CONTAINER (parent), GTK_WIDGET (self),
                               "needs-attention", !!needs_attention,
                               NULL);
    }
}

static void
notification_received_cb (VteTerminal *terminal,
                          const gchar *summary,
                          const gchar *body,
                          GbTerminal  *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL (self));

  g_print ("notification_received_cb: %s: %s\n", summary, body);

  if (!gtk_widget_has_focus (GTK_WIDGET (terminal)))
    gb_terminal_set_needs_attention (self, TRUE);
}

static gboolean
focus_in_event_cb (VteTerminal *terminal,
                   GdkEvent    *event,
                   GbTerminal  *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL (self));

  gb_terminal_set_needs_attention (self, FALSE);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
focus_out_event_cb (VteTerminal *terminal,
                    GdkEvent    *event,
                    GbTerminal  *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL (self));

  gb_terminal_set_needs_attention (self, FALSE);

  return GDK_EVENT_PROPAGATE;
}

static void
gb_terminal_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_terminal_parent_class)->finalize (object);
}

static void
gb_terminal_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_class_init (GbTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_terminal_finalize;
  object_class->get_property = gb_terminal_get_property;
  object_class->set_property = gb_terminal_set_property;

  widget_class->realize = gb_terminal_realize;
  widget_class->get_preferred_width = gb_terminal_get_preferred_width;
  widget_class->get_preferred_height = gb_terminal_get_preferred_height;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/terminal/gb-terminal.ui");
  gtk_widget_class_bind_template_child (widget_class, GbTerminal, terminal);

  g_type_ensure (VTE_TYPE_TERMINAL);
}

static void
gb_terminal_init (GbTerminal *self)
{
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
                           "focus-out-event",
                           G_CALLBACK (focus_out_event_cb),
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
}
