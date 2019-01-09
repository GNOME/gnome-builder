/* ide-terminal-page.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-terminal-page"

#include "config.h"

#include <fcntl.h>
#include <glib/gi18n.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-terminal.h>
#include <stdlib.h>
#include <vte/vte.h>
#include <unistd.h>

#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>

#include "ide-terminal-page.h"
#include "ide-terminal-page-private.h"
#include "ide-terminal-page-actions.h"

G_DEFINE_TYPE (IdeTerminalPage, ide_terminal_page, IDE_TYPE_PAGE)

enum {
  PROP_0,
  PROP_CWD,
  PROP_MANAGE_SPAWN,
  PROP_PTY,
  PROP_RUNTIME,
  PROP_RUN_ON_HOST,
  PROP_USE_RUNNER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void ide_terminal_page_connect_terminal (IdeTerminalPage *self,
                                                VteTerminal     *terminal);
static void gbp_terminal_respawn               (IdeTerminalPage *self,
                                                VteTerminal     *terminal);

static gboolean
shell_supports_login (const gchar *shell)
{
  g_autofree gchar *name = NULL;

  /* Shells that support --login */
  static const gchar *supported[] = {
    "bash",
  };

  if (shell == NULL)
    return FALSE;

  if (!(name = g_path_get_basename (shell)))
    return FALSE;

  for (guint i = 0; i < G_N_ELEMENTS (supported); i++)
    {
      if (g_str_equal (name, supported[i]))
        return TRUE;
    }

  return FALSE;
}

static void
ide_terminal_page_wait_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  VteTerminal *terminal = user_data;
  IdeTerminalPage *self;
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

  self = (IdeTerminalPage *)gtk_widget_get_ancestor (GTK_WIDGET (terminal), IDE_TYPE_TERMINAL_PAGE);
  if (self == NULL)
    IDE_GOTO (failure);

  if (!dzl_gtk_widget_action (GTK_WIDGET (self), "frame", "close-page", NULL))
    {
      if (!gtk_widget_in_destruction (GTK_WIDGET (terminal)))
        gbp_terminal_respawn (self, terminal);
    }

failure:
  g_clear_object (&terminal);

  IDE_EXIT;
}

static void
ide_terminal_page_run_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  IdeRunner *runner = (IdeRunner *)object;
  VteTerminal *terminal = user_data;
  IdeTerminalPage *self;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (VTE_IS_TERMINAL (terminal));

  if (!ide_runner_run_finish (runner, result, &error))
    {
      g_warning ("%s", error->message);
      IDE_GOTO (failure);
    }

  self = (IdeTerminalPage *)gtk_widget_get_ancestor (GTK_WIDGET (terminal), IDE_TYPE_TERMINAL_PAGE);
  if (self == NULL)
    IDE_GOTO (failure);

  if (!dzl_gtk_widget_action (GTK_WIDGET (self), "frame", "close-page", NULL))
    {
      if (!gtk_widget_in_destruction (GTK_WIDGET (terminal)))
        gbp_terminal_respawn (self, terminal);
    }

failure:
  ide_object_destroy (IDE_OBJECT (runner));
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
gbp_terminal_respawn (IdeTerminalPage *self,
                      VteTerminal     *terminal)
{
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *workpath = NULL;
  g_autofree gchar *shell = NULL;
  IdeBuildPipeline *pipeline = NULL;
  IdeWorkbench *workbench;
  IdeContext *context;
  VtePty *pty = NULL;
  gint64 now;
  int tty_fd = -1;
  gint stdout_fd = -1;
  gint stderr_fd = -1;

  IDE_ENTRY;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  vte_terminal_reset (terminal, TRUE, TRUE);

  if (!(workbench = ide_widget_get_workbench (GTK_WIDGET (self))))
    IDE_EXIT;

  /* Prevent flapping */
  now = g_get_monotonic_time ();
  if ((now - self->last_respawn) < (G_USEC_PER_SEC / 10))
    IDE_EXIT;
  self->last_respawn = now;

  context = ide_widget_get_context (GTK_WIDGET (self));
  workdir = ide_context_ref_workdir (context);
  workpath = g_file_get_path (workdir);

  if (ide_workbench_has_project (workbench))
    {
      IdeBuildManager *build_manager;

      build_manager = ide_build_manager_from_context (context);
      pipeline = ide_build_manager_get_pipeline (build_manager);
    }

  shell = g_strdup (ide_get_user_shell ());

  pty = vte_terminal_pty_new_sync (terminal,
                                   VTE_PTY_DEFAULT | VTE_PTY_NO_LASTLOG | VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP,
                                   NULL,
                                   &error);
  if (pty == NULL)
    IDE_GOTO (cleanup);

  vte_terminal_set_pty (terminal, pty);

  if (-1 == (tty_fd = ide_vte_pty_create_slave (pty)))
    IDE_GOTO (cleanup);

  if (self->runtime != NULL &&
      !ide_runtime_contains_program_in_path (self->runtime, shell, NULL))
    {
      g_free (shell);
      shell = g_strdup ("/bin/bash");
    }

  /* they want to use the runner API, which means we spawn in the
   * program mount namespace, etc.
   */
  if (self->runtime != NULL && self->use_runner)
    {
      g_autoptr(IdeSimpleBuildTarget) target = NULL;
      g_autoptr(IdeRunner) runner = NULL;
      const gchar *argv[] = { shell, NULL };


      target = ide_simple_build_target_new (context);
      ide_simple_build_target_set_argv (target, argv);
      ide_simple_build_target_set_cwd (target, self->cwd ?: workpath);

      runner = ide_runtime_create_runner (self->runtime, IDE_BUILD_TARGET (target));

      if (runner != NULL)
        {
          IdeEnvironment *env = ide_runner_get_environment (runner);

          /* set_tty() will dup() the fd */
          ide_runner_set_tty (runner, tty_fd);

          ide_environment_setenv (env, "TERM", "xterm-256color");
          ide_environment_setenv (env, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION);
          ide_environment_setenv (env, "SHELL", shell);

          if (pipeline != NULL)
            {
              ide_environment_setenv (env, "BUILDDIR", ide_build_pipeline_get_builddir (pipeline));
              ide_environment_setenv (env, "SRCDIR", ide_build_pipeline_get_srcdir (pipeline));
            }

          ide_runner_run_async (runner,
                                NULL,
                                ide_terminal_page_run_cb,
                                g_object_ref (terminal));
          IDE_GOTO (cleanup);
        }
    }

  /* dup() is safe as it will inherit O_CLOEXEC */
  if (-1 == (stdout_fd = dup (tty_fd)) || -1 == (stderr_fd = dup (tty_fd)))
    IDE_GOTO (cleanup);

  if (self->runtime != NULL)
    launcher = ide_runtime_create_launcher (self->runtime, NULL);

  if (launcher == NULL)
    launcher = ide_subprocess_launcher_new (0);

  ide_subprocess_launcher_set_flags (launcher, 0);
  ide_subprocess_launcher_set_run_on_host (launcher, self->run_on_host);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_push_argv (launcher, shell);
  if (shell_supports_login (shell))
    ide_subprocess_launcher_push_argv (launcher, "--login");
  ide_subprocess_launcher_take_stdin_fd (launcher, tty_fd);
  ide_subprocess_launcher_take_stdout_fd (launcher, stdout_fd);
  ide_subprocess_launcher_take_stderr_fd (launcher, stderr_fd);
  ide_subprocess_launcher_setenv (launcher, "TERM", "xterm-256color", TRUE);
  ide_subprocess_launcher_setenv (launcher, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION, TRUE);
  ide_subprocess_launcher_setenv (launcher, "SHELL", shell, TRUE);

  if (self->cwd != NULL)
    ide_subprocess_launcher_set_cwd (launcher, self->cwd);
  else
    ide_subprocess_launcher_set_cwd (launcher, workpath);

  if (pipeline != NULL)
    {
      ide_subprocess_launcher_setenv (launcher, "BUILDDIR", ide_build_pipeline_get_builddir (pipeline), TRUE);
      ide_subprocess_launcher_setenv (launcher, "SRCDIR", ide_build_pipeline_get_srcdir (pipeline), TRUE);
    }

  tty_fd = -1;
  stdout_fd = -1;
  stderr_fd = -1;

  if (NULL == (subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    IDE_GOTO (cleanup);

  ide_subprocess_wait_async (subprocess,
                             NULL,
                             ide_terminal_page_wait_cb,
                             g_object_ref (terminal));

cleanup:
  if (tty_fd != -1)
    close (tty_fd);

  if (stdout_fd != -1)
    close (stdout_fd);

  if (stderr_fd != -1)
    close (stderr_fd);

  g_clear_object (&pty);

  if (error != NULL)
    g_warning ("%s", error->message);

  IDE_EXIT;
}

static void
gbp_terminal_realize (GtkWidget *widget)
{
  IdeTerminalPage *self = (IdeTerminalPage *)widget;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  GTK_WIDGET_CLASS (ide_terminal_page_parent_class)->realize (widget);

  if (self->manage_spawn && !self->top_has_spawned)
    {
      self->top_has_spawned = TRUE;
      gbp_terminal_respawn (self, VTE_TERMINAL (self->terminal_top));
    }

  if (!self->manage_spawn && self->pty != NULL)
    vte_terminal_set_pty (VTE_TERMINAL (self->terminal_top), self->pty);
}

static void
gbp_terminal_get_preferred_width (GtkWidget *widget,
                                  gint      *min_width,
                                  gint      *nat_width)
{
  /*
   * Since we are placing the terminal in a GtkStack, we need
   * to fake the size a bit. Otherwise, GtkStack tries to keep the
   * widget at its natural size (which prevents us from getting
   * appropriate size requests.
   */
  GTK_WIDGET_CLASS (ide_terminal_page_parent_class)->get_preferred_width (widget, min_width, nat_width);
  *nat_width = *min_width;
}

static void
gbp_terminal_get_preferred_height (GtkWidget *widget,
                                   gint      *min_height,
                                   gint      *nat_height)
{
  /*
   * Since we are placing the terminal in a GtkStack, we need
   * to fake the size a bit. Otherwise, GtkStack tries to keep the
   * widget at its natural size (which prevents us from getting
   * appropriate size requests.
   */
  GTK_WIDGET_CLASS (ide_terminal_page_parent_class)->get_preferred_height (widget, min_height, nat_height);
  *nat_height = *min_height;
}

static void
gbp_terminal_set_needs_attention (IdeTerminalPage *self,
                                  gboolean         needs_attention)
{
  GtkWidget *parent;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

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
notification_received_cb (VteTerminal     *terminal,
                          const gchar     *summary,
                          const gchar     *body,
                          IdeTerminalPage *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  if (!gtk_widget_has_focus (GTK_WIDGET (terminal)))
    gbp_terminal_set_needs_attention (self, TRUE);
}

static gboolean
focus_in_event_cb (VteTerminal     *terminal,
                   GdkEvent        *event,
                   IdeTerminalPage *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  self->top_has_needs_attention = FALSE;
  gbp_terminal_set_needs_attention (self, FALSE);
  gtk_revealer_set_reveal_child (self->search_revealer_top, FALSE);

  return GDK_EVENT_PROPAGATE;
}

static void
window_title_changed_cb (VteTerminal     *terminal,
                         IdeTerminalPage *self)
{
  const gchar *title;

  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  title = vte_terminal_get_window_title (VTE_TERMINAL (self->terminal_top));

  if (title == NULL)
    title = _("Untitled terminal");

  ide_page_set_title (IDE_PAGE (self), title);
}

static void
style_context_changed (GtkStyleContext *style_context,
                       IdeTerminalPage *self)
{
  GtkStateFlags state;
  GdkRGBA fg;
  GdkRGBA bg;

  g_assert (GTK_IS_STYLE_CONTEXT (style_context));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  state = gtk_style_context_get_state (style_context);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_style_context_get_color (style_context, state, &fg);
  gtk_style_context_get_background_color (style_context, state, &bg);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (bg.alpha == 0.0)
    gdk_rgba_parse (&bg, "#f6f7f8");

  ide_page_set_primary_color_fg (IDE_PAGE (self), &fg);
  ide_page_set_primary_color_bg (IDE_PAGE (self), &bg);
}

static IdePage *
gbp_terminal_create_split (IdePage *page)
{
  g_assert (IDE_IS_TERMINAL_PAGE (page));

  return g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "visible", TRUE,
                       NULL);
}

static void
gbp_terminal_grab_focus (GtkWidget *widget)
{
  IdeTerminalPage *self = (IdeTerminalPage *)widget;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->terminal_top));
}

static void
ide_terminal_page_connect_terminal (IdeTerminalPage *self,
                                    VteTerminal     *terminal)
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
ide_terminal_page_finalize (GObject *object)
{
  IdeTerminalPage *self = IDE_TERMINAL_PAGE (object);

  g_clear_object (&self->save_as_file_top);
  g_clear_pointer (&self->cwd, g_free);
  g_clear_pointer (&self->selection_buffer, g_free);
  g_clear_object (&self->pty);
  g_clear_object (&self->runtime);

  G_OBJECT_CLASS (ide_terminal_page_parent_class)->finalize (object);
}

static void
ide_terminal_page_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTerminalPage *self = IDE_TERMINAL_PAGE (object);

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

    case PROP_USE_RUNNER:
      g_value_set_boolean (value, self->use_runner);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_page_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTerminalPage *self = IDE_TERMINAL_PAGE (object);

  switch (prop_id)
    {
    case PROP_CWD:
      self->cwd = g_value_dup_string (value);
      break;

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

    case PROP_USE_RUNNER:
      self->use_runner = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_page_class_init (IdeTerminalPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdePageClass *page_class = IDE_PAGE_CLASS (klass);

  object_class->finalize = ide_terminal_page_finalize;
  object_class->get_property = ide_terminal_page_get_property;
  object_class->set_property = ide_terminal_page_set_property;

  widget_class->realize = gbp_terminal_realize;
  widget_class->get_preferred_width = gbp_terminal_get_preferred_width;
  widget_class->get_preferred_height = gbp_terminal_get_preferred_height;
  widget_class->grab_focus = gbp_terminal_grab_focus;

  page_class->create_split = gbp_terminal_create_split;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-page.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPage, terminal_top);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPage, top_scrollbar);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPage, terminal_overlay_top);

  properties [PROP_CWD] =
    g_param_spec_string ("cwd",
                         "CWD",
                         "The directory to spawn the terminal in",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_MANAGE_SPAWN] =
    g_param_spec_boolean ("manage-spawn",
                          "Manage Spawn",
                          "Manage Spawn",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PTY] =
    g_param_spec_object ("pty",
                         "Pty",
                         "The pseudo terminal to use",
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

  properties [PROP_USE_RUNNER] =
    g_param_spec_boolean ("use-runner",
                          "Use Runner",
                          "If we should use the runner interface and build target",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_terminal_page_init (IdeTerminalPage *self)
{
  GtkStyleContext *style_context;

  self->run_on_host = TRUE;
  self->manage_spawn = TRUE;

  self->tsearch = g_object_new (IDE_TYPE_TERMINAL_SEARCH,
                                "visible", TRUE,
                                NULL);
  self->search_revealer_top = ide_terminal_search_get_revealer (self->tsearch);

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_page_set_icon_name (IDE_PAGE (self), "utilities-terminal-symbolic");
  ide_page_set_can_split (IDE_PAGE (self), TRUE);
  ide_page_set_menu_id (IDE_PAGE (self), "terminal-page-document-menu");

  gtk_overlay_add_overlay (self->terminal_overlay_top, GTK_WIDGET (self->tsearch));

  ide_terminal_page_connect_terminal (self, VTE_TERMINAL (self->terminal_top));

  ide_terminal_search_set_terminal (self->tsearch, VTE_TERMINAL (self->terminal_top));

  ide_terminal_page_actions_init (self);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self->terminal_top));
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
ide_terminal_page_set_pty (IdeTerminalPage *self,
                           VtePty          *pty)
{
  g_return_if_fail (IDE_IS_TERMINAL_PAGE (self));
  g_return_if_fail (VTE_IS_PTY (pty));

  if (self->manage_spawn)
    {
      g_warning ("Cannot set pty when IdeTerminalPage manages tty");
      return;
    }

  if (self->terminal_top)
    {
      vte_terminal_reset (VTE_TERMINAL (self->terminal_top), TRUE, TRUE);
      vte_terminal_set_pty (VTE_TERMINAL (self->terminal_top), pty);
    }
}

void
ide_terminal_page_feed (IdeTerminalPage *self,
                        const gchar     *message)
{
  g_return_if_fail (IDE_IS_TERMINAL_PAGE (self));

  if (self->terminal_top != NULL)
    vte_terminal_feed (VTE_TERMINAL (self->terminal_top), message, -1);
}
