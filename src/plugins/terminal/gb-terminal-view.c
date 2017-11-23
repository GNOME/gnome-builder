/* gb-terminal-view.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-terminal-view"
#define PCRE2_CODE_UNIT_WIDTH 0

#include "config.h"

#include <fcntl.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <pcre2.h>
#include <stdlib.h>
#include <vte/vte.h>
#include <unistd.h>

#include "gb-terminal-view.h"
#include "gb-terminal-view-private.h"
#include "gb-terminal-view-actions.h"

G_DEFINE_TYPE (GbTerminalView, gb_terminal_view, IDE_TYPE_LAYOUT_VIEW)

enum {
  PROP_0,
  PROP_MANAGE_SPAWN,
  PROP_PTY,
  PROP_RUNTIME,
  PROP_RUN_ON_HOST,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];
static gchar *cached_shell;

static void gb_terminal_view_connect_terminal (GbTerminalView *self,
                                               VteTerminal    *terminal);
static void gb_terminal_respawn               (GbTerminalView *self,
                                               VteTerminal    *terminal);

static gchar *
gb_terminal_view_discover_shell (GbTerminalView  *self,
                                 GCancellable    *cancellable,
                                 GError         **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autofree gchar *command = NULL;
  g_autofree gchar *stdout_buf = NULL;
  g_auto(GStrv) argv = NULL;

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (cached_shell != NULL)
    return g_strdup (cached_shell);

  command = g_strdup_printf ("sh -c 'getent passwd | grep ^%s: | head -n1 | cut -f 7 -d :'",
                             g_get_user_name ());

  if (!g_shell_parse_argv (command, NULL, &argv, error))
    return NULL;

  /*
   * We don't use the runtime shell here, because we want to know
   * what the host thinks the user shell should be.
   */
  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());
  ide_subprocess_launcher_push_args (launcher, (const gchar * const *)argv);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, error);

  if (subprocess == NULL)
    return NULL;

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, cancellable, &stdout_buf, NULL, error))
    return NULL;

  if (stdout_buf != NULL)
    {
      g_strstrip (stdout_buf);
      if (stdout_buf[0] == '/')
        cached_shell = g_steal_pointer (&stdout_buf);
    }

  if (cached_shell == NULL)
    g_set_error_literal (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "Unknown error when discovering user shell");

  return g_strdup (cached_shell);
}

static void
gb_terminal_view_wait_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  VteTerminal *terminal = user_data;
  GbTerminalView *self;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (VTE_IS_TERMINAL (terminal));

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    {
      g_warning ("%s", error->message);
      IDE_GOTO (failure);
    }

  self = (GbTerminalView *)gtk_widget_get_ancestor (GTK_WIDGET (terminal), GB_TYPE_TERMINAL_VIEW);
  if (self == NULL)
    IDE_GOTO (failure);

  if (!dzl_gtk_widget_action (GTK_WIDGET (self), "layoutstack", "close-view", NULL))
    {
      if (!gtk_widget_in_destruction (GTK_WIDGET (terminal)))
        gb_terminal_respawn (self, terminal);
    }

failure:
  g_clear_object (&terminal);

  IDE_EXIT;
}

static gboolean
terminal_has_notification_signal (void)
{
  GQuark quark;
  guint signal_id;

  return g_signal_parse_name ("notification-received",
                              VTE_TYPE_TERMINAL,
                              &signal_id,
                              &quark,
                              FALSE);
}

static void
gb_terminal_respawn (GbTerminalView *self,
                     VteTerminal    *terminal)
{
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *workpath = NULL;
  g_autofree gchar *shell = NULL;
  GtkWidget *toplevel;
  GError *error = NULL;
  IdeBuildManager *build_manager;
  IdeBuildPipeline *pipeline;
  IdeContext *context;
  IdeVcs *vcs;
  VtePty *pty = NULL;
  GFile *workdir;
  gint64 now;
  int tty_fd = -1;
  gint stdout_fd = -1;
  gint stderr_fd = -1;

  IDE_ENTRY;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  vte_terminal_reset (terminal, TRUE, TRUE);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!IDE_IS_WORKBENCH (toplevel))
    IDE_EXIT;

  /* Prevent flapping */
  now = g_get_monotonic_time ();
  if ((now - self->last_respawn) < (G_USEC_PER_SEC / 10))
    IDE_EXIT;
  self->last_respawn = now;

  context = ide_workbench_get_context (IDE_WORKBENCH (toplevel));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  workpath = g_file_get_path (workdir);

  build_manager = ide_context_get_build_manager (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  shell = gb_terminal_view_discover_shell (self, NULL, &error);

  if (shell == NULL)
    {
      g_warning ("Failed to discover user shell: %s", error->message);

      /* We prefer bash in flatpak over sh */
      if (ide_is_flatpak ())
        shell = g_strdup ("/bin/bash");
      else
        shell = vte_get_user_shell ();

      g_clear_error (&error);
    }

  pty = vte_terminal_pty_new_sync (terminal,
                                   VTE_PTY_DEFAULT | VTE_PTY_NO_LASTLOG | VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP,
                                   NULL,
                                   &error);
  if (pty == NULL)
    IDE_GOTO (failure);

  vte_terminal_set_pty (terminal, pty);

  if (-1 == (tty_fd = ide_vte_pty_create_slave (pty)))
    IDE_GOTO (failure);

  /* dup() is safe as it will inherit O_CLOEXEC */
  if (-1 == (stdout_fd = dup (tty_fd)) || -1 == (stderr_fd = dup (tty_fd)))
    IDE_GOTO (failure);

  if (self->runtime != NULL)
    {
      launcher = ide_runtime_create_launcher (self->runtime, NULL);

      if (!ide_runtime_contains_program_in_path (self->runtime, shell, NULL))
        {
          g_free (shell);
          shell = g_strdup ("/bin/bash");
        }
    }

  if (launcher == NULL)
    launcher = ide_subprocess_launcher_new (0);

  ide_subprocess_launcher_set_flags (launcher, 0);
  ide_subprocess_launcher_set_run_on_host (launcher, self->run_on_host);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_cwd (launcher, workpath);
  ide_subprocess_launcher_push_argv (launcher, shell);
  ide_subprocess_launcher_take_stdin_fd (launcher, tty_fd);
  ide_subprocess_launcher_take_stdout_fd (launcher, stdout_fd);
  ide_subprocess_launcher_take_stderr_fd (launcher, stderr_fd);
  ide_subprocess_launcher_setenv (launcher, "TERM", "xterm-256color", TRUE);
  ide_subprocess_launcher_setenv (launcher, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION, TRUE);
  ide_subprocess_launcher_setenv (launcher, "SHELL", shell, TRUE);

  if (pipeline != NULL)
    {
      ide_subprocess_launcher_setenv (launcher, "BUILDDIR", ide_build_pipeline_get_builddir (pipeline), TRUE);
      ide_subprocess_launcher_setenv (launcher, "SRCDIR", ide_build_pipeline_get_srcdir (pipeline), TRUE);
    }

  tty_fd = -1;
  stdout_fd = -1;
  stderr_fd = -1;

  if (NULL == (subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    IDE_GOTO (failure);

  ide_subprocess_wait_async (subprocess,
                             NULL,
                             gb_terminal_view_wait_cb,
                             g_object_ref (terminal));

failure:
  if (tty_fd != -1)
    close (tty_fd);

  if (stdout_fd != -1)
    close (stdout_fd);

  g_clear_object (&pty);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  IDE_EXIT;
}

static void
gb_terminal_realize (GtkWidget *widget)
{
  GbTerminalView *self = (GbTerminalView *)widget;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  GTK_WIDGET_CLASS (gb_terminal_view_parent_class)->realize (widget);

  if (self->manage_spawn && !self->top_has_spawned)
    {
      self->top_has_spawned = TRUE;
      gb_terminal_respawn (self, self->terminal_top);
    }

  if (!self->manage_spawn && self->pty != NULL)
    vte_terminal_set_pty (self->terminal_top, self->pty);
}

static void
gb_terminal_get_preferred_width (GtkWidget *widget,
                                 gint      *min_width,
                                 gint      *nat_width)
{
  /*
   * Since we are placing the terminal in a GtkStack, we need
   * to fake the size a bit. Otherwise, GtkStack tries to keep the
   * widget at its natural size (which prevents us from getting
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
   * widget at its natural size (which prevents us from getting
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
      !gtk_widget_in_destruction (parent))
    {
      if (!gtk_widget_in_destruction (GTK_WIDGET (self->terminal_top)))
        self->top_has_needs_attention = !!needs_attention;

      gtk_container_child_set (GTK_CONTAINER (parent), GTK_WIDGET (self),
                               "needs-attention", needs_attention,
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

  self->top_has_needs_attention = FALSE;
  gb_terminal_set_needs_attention (self, FALSE);
  gtk_revealer_set_reveal_child (self->search_revealer_top, FALSE);

  return GDK_EVENT_PROPAGATE;
}

static void
window_title_changed_cb (VteTerminal    *terminal,
                         GbTerminalView *self)
{
  const gchar *title;

  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  title = vte_terminal_get_window_title (self->terminal_top);

  if (title == NULL)
    title = _("Untitled terminal");

  ide_layout_view_set_title (IDE_LAYOUT_VIEW (self), title);
}

static void
style_context_changed (GtkStyleContext *style_context,
                       GbTerminalView  *self)
{
  GtkStateFlags state;
  GdkRGBA fg;
  GdkRGBA bg;

  g_assert (GTK_IS_STYLE_CONTEXT (style_context));
  g_assert (GB_IS_TERMINAL_VIEW (self));

  state = gtk_style_context_get_state (style_context);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_style_context_get_color (style_context, state, &fg);
  gtk_style_context_get_background_color (style_context, state, &bg);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (bg.alpha == 0.0)
    gdk_rgba_parse (&bg, "#f6f7f8");

  ide_layout_view_set_primary_color_fg (IDE_LAYOUT_VIEW (self), &fg);
  ide_layout_view_set_primary_color_bg (IDE_LAYOUT_VIEW (self), &bg);
}

static IdeLayoutView *
gb_terminal_create_split_view (IdeLayoutView *view)
{
  g_assert (GB_IS_TERMINAL_VIEW (view));

  return g_object_new (GB_TYPE_TERMINAL_VIEW,
                       "visible", TRUE,
                       NULL);
}

static void
gb_terminal_grab_focus (GtkWidget *widget)
{
  GbTerminalView *self = (GbTerminalView *)widget;

  g_assert (GB_IS_TERMINAL_VIEW (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->terminal_top));
}

static void
gb_terminal_view_connect_terminal (GbTerminalView *self,
                                   VteTerminal    *terminal)
{
  GtkAdjustment *vadj;

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (terminal));

  gtk_range_set_adjustment (GTK_RANGE (self->top_scrollbar), vadj);

  g_signal_connect_object (terminal,
                           "focus-in-event",
                           G_CALLBACK (focus_in_event_cb),
                           self,
                           0);

  g_signal_connect_object (terminal,
                           "window-title-changed",
                           G_CALLBACK (window_title_changed_cb),
                           self,
                           0);

  if (terminal_has_notification_signal ())
    {
      g_signal_connect_object (terminal,
                               "notification-received",
                               G_CALLBACK (notification_received_cb),
                               self,
                               0);
    }
}

static void
gb_terminal_view_finalize (GObject *object)
{
  GbTerminalView *self = GB_TERMINAL_VIEW (object);

  g_clear_object (&self->save_as_file_top);
  g_clear_pointer (&self->selection_buffer, g_free);
  g_clear_object (&self->pty);
  g_clear_object (&self->runtime);

  G_OBJECT_CLASS (gb_terminal_view_parent_class)->finalize (object);
}

static void
gb_terminal_view_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbTerminalView *self = GB_TERMINAL_VIEW (object);

  switch (prop_id)
    {
    case PROP_MANAGE_SPAWN:
      g_value_set_boolean (value, self->manage_spawn);
      break;

    case PROP_PTY:
      g_value_set_object (value, self->pty);
      break;

    case PROP_RUNTIME:
      g_value_set_object (value, self->runtime);
      break;

    case PROP_RUN_ON_HOST:
      g_value_set_boolean (value, self->run_on_host);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_view_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbTerminalView *self = GB_TERMINAL_VIEW (object);

  switch (prop_id)
    {
    case PROP_MANAGE_SPAWN:
      self->manage_spawn = g_value_get_boolean (value);
      break;

    case PROP_PTY:
      self->pty = g_value_dup_object (value);
      break;

    case PROP_RUNTIME:
      self->runtime = g_value_dup_object (value);
      break;

    case PROP_RUN_ON_HOST:
      self->run_on_host = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_view_class_init (GbTerminalViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeLayoutViewClass *view_class = IDE_LAYOUT_VIEW_CLASS (klass);

  object_class->finalize = gb_terminal_view_finalize;
  object_class->get_property = gb_terminal_view_get_property;
  object_class->set_property = gb_terminal_view_set_property;

  widget_class->realize = gb_terminal_realize;
  widget_class->get_preferred_width = gb_terminal_get_preferred_width;
  widget_class->get_preferred_height = gb_terminal_get_preferred_height;
  widget_class->grab_focus = gb_terminal_grab_focus;

  view_class->create_split_view = gb_terminal_create_split_view;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/terminal/gb-terminal-view.ui");
  gtk_widget_class_bind_template_child (widget_class, GbTerminalView, terminal_top);
  gtk_widget_class_bind_template_child (widget_class, GbTerminalView, top_scrollbar);
  gtk_widget_class_bind_template_child (widget_class, GbTerminalView, terminal_overlay_top);

  properties [PROP_MANAGE_SPAWN] =
    g_param_spec_boolean ("manage-spawn",
                          "Manage Spawn",
                          "Manage Spawn",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PTY] =
    g_param_spec_object ("pty",
                         "Pty",
                         "The psuedo terminal to use",
                         VTE_TYPE_PTY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNTIME] =
    g_param_spec_object ("runtime",
                         "Runtime",
                         "The runtime to use for spawning",
                         IDE_TYPE_RUNTIME,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUN_ON_HOST] =
    g_param_spec_boolean ("run-on-host",
                          "Run on Host",
                          "If the process should be spawned on the host",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_terminal_view_init (GbTerminalView *self)
{
  GtkStyleContext *style_context;

  self->run_on_host = TRUE;
  self->manage_spawn = TRUE;

  self->tsearch = g_object_new (IDE_TYPE_TERMINAL_SEARCH, NULL);
  self->search_revealer_top = ide_terminal_search_get_revealer (self->tsearch);

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_layout_view_set_icon_name (IDE_LAYOUT_VIEW (self), "utilities-terminal-symbolic");
  ide_layout_view_set_can_split (IDE_LAYOUT_VIEW (self), TRUE);
  ide_layout_view_set_menu_id (IDE_LAYOUT_VIEW (self), "terminal-view-document-menu");

  gtk_overlay_add_overlay (self->terminal_overlay_top,
                           GTK_WIDGET (self->search_revealer_top));

  gb_terminal_view_connect_terminal (self, self->terminal_top);

  ide_terminal_search_set_terminal (self->tsearch, self->terminal_top);

  gb_terminal_view_actions_init (self);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (style_context, "terminal");
  g_signal_connect_object (style_context,
                           "changed",
                           G_CALLBACK (style_context_changed),
                           self,
                           0);
  style_context_changed (style_context, self);

  gtk_widget_set_can_focus (GTK_WIDGET (self->terminal_top), TRUE);
}

void
gb_terminal_view_set_pty (GbTerminalView *self,
                          VtePty         *pty)
{
  g_return_if_fail (GB_IS_TERMINAL_VIEW (self));
  g_return_if_fail (VTE_IS_PTY (pty));

  if (self->manage_spawn)
    {
      g_warning ("Cannot set pty when GbTerminalView manages tty");
      return;
    }

  if (self->terminal_top)
    {
      vte_terminal_reset (self->terminal_top, TRUE, TRUE);
      vte_terminal_set_pty (self->terminal_top, pty);
    }
}
