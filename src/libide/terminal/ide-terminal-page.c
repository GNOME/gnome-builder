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

#include <glib/gi18n.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <vte/vte.h>

#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-terminal.h>

#include "ide-terminal-page.h"
#include "ide-terminal-page-private.h"
#include "ide-terminal-page-actions.h"
#include "ide-terminal-run-command-private.h"

#define FLAPPING_DURATION_USEC (G_USEC_PER_SEC / 20)

G_DEFINE_FINAL_TYPE (IdeTerminalPage, ide_terminal_page, IDE_TYPE_PAGE)

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

enum {
  PROP_0,
  PROP_CLOSE_ON_EXIT,
  PROP_LAUNCHER,
  PROP_MANAGE_SPAWN,
  PROP_RESPAWN_ON_EXIT,
  PROP_PTY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void ide_terminal_page_connect_terminal (IdeTerminalPage *self,
                                                VteTerminal     *terminal);

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

static gboolean
destroy_widget_in_idle (GtkWidget *widget)
{
  IdeTerminalPage *self = (IdeTerminalPage *)widget;

  IDE_ENTRY;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  if (!self->destroyed)
    panel_widget_close (PANEL_WIDGET (self));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_terminal_page_spawn_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeTerminalLauncher *launcher = (IdeTerminalLauncher *)object;
  g_autoptr(IdeTerminalPage) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *title = NULL;
  gboolean maybe_flapping;
  gint64 now;

  IDE_ENTRY;

  g_assert (IDE_IS_TERMINAL_LAUNCHER (launcher));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  self->exited = TRUE;

  ide_terminal_launcher_spawn_finish (launcher, result, &error);

  if (self->destroyed)
    IDE_EXIT;

  if (error != NULL)
    {
      g_autofree gchar *format = NULL;

      format = g_strdup_printf ("%s\r\n%s\r\n", _("Failed to launch subprocess. You may need to rebuild your project."), error->message);
      ide_terminal_page_feed (self, format);
    }

  title = g_strdup_printf ("%s (%s)",
                           panel_widget_get_title (PANEL_WIDGET (self)) ?: _("Untitled terminal"),
                           /* translators: exited describes that the terminal shell process has exited */
                           _("Exited"));
  panel_widget_set_title (PANEL_WIDGET (self), title);

  now = g_get_monotonic_time ();
  maybe_flapping = ABS (now - self->last_respawn) < FLAPPING_DURATION_USEC;

  if (!self->respawn_on_exit)
    {
      if (self->close_on_exit && !maybe_flapping)
        g_idle_add_full (G_PRIORITY_LOW + 1000,
                         (GSourceFunc) destroy_widget_in_idle,
                         g_object_ref (self),
                         g_object_unref);
      else
        vte_terminal_set_input_enabled (VTE_TERMINAL (self->terminal), FALSE);
      IDE_EXIT;
    }

  if (maybe_flapping)
    {
      ide_terminal_page_feed (self, _("Subprocess launcher failed too quickly, will not respawn."));
      ide_terminal_page_feed (self, "\r\n");
      IDE_EXIT;
    }

  g_clear_object (&self->pty);
  vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
  self->pty = ide_pty_new_sync (NULL);
  vte_terminal_set_pty (VTE_TERMINAL (self->terminal), self->pty);

  /* Spawn our terminal and wait for it to exit */
  self->last_respawn = now;
  self->exited = FALSE;
  panel_widget_set_title (PANEL_WIDGET (self), _("Untitled terminal"));
  ide_terminal_launcher_spawn_async (self->launcher,
                                     self->pty,
                                     NULL,
                                     ide_terminal_page_spawn_cb,
                                     g_object_ref (self));

  IDE_EXIT;
}

static gboolean
ide_terminal_page_do_spawn_in_idle (IdeTerminalPage *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  if (self->destroyed)
    IDE_RETURN (G_SOURCE_REMOVE);

  self->last_respawn = g_get_monotonic_time ();

  if (self->pty == NULL)
    {
      g_autoptr(GError) error = NULL;

      if (!(self->pty = ide_pty_new_sync (&error)))
        {
          g_critical ("Failed to create PTY for terminal: %s", error->message);
          IDE_RETURN (G_SOURCE_REMOVE);
        }
    }

  vte_terminal_set_pty (VTE_TERMINAL (self->terminal), self->pty);

  if (!self->manage_spawn)
    IDE_RETURN (G_SOURCE_REMOVE);

  /* Spawn our terminal and wait for it to exit */
  ide_terminal_launcher_spawn_async (self->launcher,
                                     self->pty,
                                     NULL,
                                     ide_terminal_page_spawn_cb,
                                     g_object_ref (self));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_terminal_page_realize (GtkWidget *widget)
{
  IdeTerminalPage *self = (IdeTerminalPage *)widget;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  GTK_WIDGET_CLASS (ide_terminal_page_parent_class)->realize (widget);

  if (self->did_defered_setup_in_realize)
    return;

  self->did_defered_setup_in_realize = TRUE;

  /* We don't want to process this in realize as it could be holding things
   * up from being mapped. Instead, wait until the GDK backend has finished
   * reacting to realize/etc and then spawn from idle.
   */
  g_idle_add_full (G_PRIORITY_LOW,
                   (GSourceFunc)ide_terminal_page_do_spawn_in_idle,
                   g_object_ref (self),
                   g_object_unref);
}

static void
notification_received_cb (VteTerminal     *terminal,
                          const gchar     *summary,
                          const gchar     *body,
                          IdeTerminalPage *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  if (self->destroyed)
    return;

  if (!gtk_widget_has_focus (GTK_WIDGET (terminal)))
    panel_widget_set_needs_attention (PANEL_WIDGET (self), TRUE);
}

static void
focus_in_event_cb (GtkEventControllerFocus *focus,
                   IdeTerminalPage         *self)
{
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  panel_widget_set_needs_attention (PANEL_WIDGET (self), FALSE);
}

static void
window_title_changed_cb (VteTerminal     *terminal,
                         IdeTerminalPage *self)
{
  const gchar *title;

  g_assert (VTE_IS_TERMINAL (terminal));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  if (self->destroyed)
    return;

  title = vte_terminal_get_window_title (VTE_TERMINAL (self->terminal));

  if (title == NULL || title[0] == '\0')
    title = _("Untitled terminal");

  panel_widget_set_title (PANEL_WIDGET (self), title);
}

static IdePage *
ide_terminal_page_create_split (IdePage *page)
{
  IdeTerminalPage *self = (IdeTerminalPage *)page;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  return g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "close-on-exit", self->close_on_exit,
                       "launcher", self->launcher,
                       "manage-spawn", self->manage_spawn,
                       "pty", NULL,
                       "respawn-on-exit", self->respawn_on_exit,
                       "visible", TRUE,
                       NULL);
}

static gboolean
ide_terminal_page_grab_focus (GtkWidget *widget)
{
  IdeTerminalPage *self = (IdeTerminalPage *)widget;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  return gtk_widget_grab_focus (GTK_WIDGET (self->terminal));
}

static void
ide_terminal_page_connect_terminal (IdeTerminalPage *self,
                                    VteTerminal     *terminal)
{
  GtkEventController *controller;

  g_assert (IDE_IS_TERMINAL_PAGE (self));
  g_assert (IDE_IS_TERMINAL (terminal));

  if (self->destroyed)
    return;

  controller = gtk_event_controller_focus_new ();
  g_signal_connect_object (controller,
                           "enter",
                           G_CALLBACK (focus_in_event_cb),
                           self,
                           0);
  gtk_widget_add_controller (GTK_WIDGET (terminal), controller);


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
ide_terminal_page_context_set (GtkWidget  *widget,
                               IdeContext *context)
{
  IdeTerminalPage *self = (IdeTerminalPage *)widget;

  g_assert (IDE_IS_TERMINAL_PAGE (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    return;

  if (self->launcher == NULL)
    {
      g_autoptr(IdeRunCommand) run_command = ide_terminal_run_command_new (IDE_TERMINAL_RUN_ON_HOST);
      self->launcher = ide_terminal_launcher_new (context, run_command);
    }
}

static GFile *
ide_terminal_page_get_file_or_directory (IdePage *page)
{
  IdeTerminalPage *self = (IdeTerminalPage *)page;
  const char *uri;

  g_assert (IDE_IS_TERMINAL_PAGE (self));

  if (self->destroyed)
    return NULL;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (!(uri = vte_terminal_get_current_file_uri (VTE_TERMINAL (self->terminal))))
    uri = vte_terminal_get_current_directory_uri (VTE_TERMINAL (self->terminal));
  G_GNUC_END_IGNORE_DEPRECATIONS

  if (uri != NULL)
    return g_file_new_for_uri (uri);

  return NULL;
}

static void
set_search_visible (IdeTerminalPage          *self,
                    gboolean                search_visible)
{
  g_return_if_fail (IDE_IS_TERMINAL_PAGE (self));

  gtk_revealer_set_reveal_child (self->search_revealer, search_visible);

  if (search_visible)
    gtk_widget_grab_focus (GTK_WIDGET (self->search_bar));
}

static void
search_hide_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *param)
{
  IdeTerminalPage *self = IDE_TERMINAL_PAGE (widget);

  set_search_visible (self, FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (self->terminal));
}

static void
search_show_action (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  set_search_visible (IDE_TERMINAL_PAGE (widget), TRUE);
}

static void
ide_terminal_page_dispose (GObject *object)
{
  IdeTerminalPage *self = IDE_TERMINAL_PAGE (object);

  self->destroyed = TRUE;

  g_clear_object (&self->launcher);
  g_clear_object (&self->save_as_file);
  g_clear_pointer (&self->selection_buffer, g_free);
  g_clear_object (&self->pty);

  G_OBJECT_CLASS (ide_terminal_page_parent_class)->dispose (object);
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
    case PROP_CLOSE_ON_EXIT:
      g_value_set_boolean (value, self->close_on_exit);
      break;

    case PROP_LAUNCHER:
      g_value_set_object (value, self->launcher);
      break;

    case PROP_MANAGE_SPAWN:
      g_value_set_boolean (value, self->manage_spawn);
      break;

    case PROP_PTY:
      g_value_set_object (value, self->pty);
      break;

    case PROP_RESPAWN_ON_EXIT:
      g_value_set_boolean (value, self->respawn_on_exit);
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
    case PROP_CLOSE_ON_EXIT:
      self->close_on_exit = g_value_get_boolean (value);
      break;

    case PROP_MANAGE_SPAWN:
      self->manage_spawn = g_value_get_boolean (value);
      break;

    case PROP_PTY:
      self->pty = g_value_dup_object (value);
      break;

    case PROP_RESPAWN_ON_EXIT:
      self->respawn_on_exit = g_value_get_boolean (value);
      break;

    case PROP_LAUNCHER:
      ide_terminal_page_set_launcher (self, g_value_get_object (value));
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

  object_class->dispose = ide_terminal_page_dispose;
  object_class->get_property = ide_terminal_page_get_property;
  object_class->set_property = ide_terminal_page_set_property;

  widget_class->realize = ide_terminal_page_realize;
  widget_class->grab_focus = ide_terminal_page_grab_focus;

  page_class->create_split = ide_terminal_page_create_split;
  page_class->get_file_or_directory = ide_terminal_page_get_file_or_directory;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-page.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPage, terminal);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPage, search_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalPage, search_revealer);

  gtk_widget_class_install_action (widget_class, "search.hide", NULL, search_hide_action);
  gtk_widget_class_install_action (widget_class, "terminal.search", NULL, search_show_action);

  g_type_ensure (IDE_TYPE_TERMINAL_SEARCH);

  properties [PROP_CLOSE_ON_EXIT] =
    g_param_spec_boolean ("close-on-exit",
                          "Close on Exit",
                          "Close on Exit",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MANAGE_SPAWN] =
    g_param_spec_boolean ("manage-spawn",
                          "Manage Spawn",
                          "Manage Spawn",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RESPAWN_ON_EXIT] =
    g_param_spec_boolean ("respawn-on-exit",
                          "Respawn on Exit",
                          "Respawn on Exit",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PTY] =
    g_param_spec_object ("pty",
                         "Pty",
                         "The pseudo terminal to use",
                         VTE_TYPE_PTY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LAUNCHER] =
    g_param_spec_object ("launcher",
                         "Launcher",
                         "The launcher to use for spawning",
                         IDE_TYPE_TERMINAL_LAUNCHER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

}

static void
ide_terminal_page_init (IdeTerminalPage *self)
{
  self->close_on_exit = TRUE;
  self->respawn_on_exit = TRUE;
  self->manage_spawn = TRUE;

  gtk_widget_init_template (GTK_WIDGET (self));

  panel_widget_set_icon_name (PANEL_WIDGET (self), "builder-terminal-symbolic");
  ide_page_set_can_split (IDE_PAGE (self), TRUE);
  ide_page_set_menu_id (IDE_PAGE (self), "ide-terminal-page-document-menu");

  ide_terminal_page_connect_terminal (self, VTE_TERMINAL (self->terminal));

  ide_terminal_search_set_terminal (self->search_bar, VTE_TERMINAL (self->terminal));

  ide_terminal_page_actions_init (self);

  ide_widget_set_context_handler (self, ide_terminal_page_context_set);

  /* This is needed so terminal pages get a color that match
   * what the "view" area is of the center grid in Builder. All
   * made more complicated because the colors come from restyling
   * GSV themes rather than terminal things like in Ptyxis.
   */
  gtk_widget_add_css_class (GTK_WIDGET (self), "view");
}

/**
 * ide_terminal_page_get_pty:
 * @self: a #IdeTerminalPage
 *
 * Gets the #VtePty for the page.
 *
 * Returns: (transfer none): a #VtePty
 */
VtePty *
ide_terminal_page_get_pty (IdeTerminalPage *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_PAGE (self), NULL);

  return self->pty;
}

void
ide_terminal_page_set_pty (IdeTerminalPage *self,
                           VtePty          *pty)
{
  g_return_if_fail (IDE_IS_TERMINAL_PAGE (self));
  g_return_if_fail (VTE_IS_PTY (pty));

  if (self->destroyed)
    return;

  if (g_set_object (&self->pty, pty))
    {
      vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
      vte_terminal_set_pty (VTE_TERMINAL (self->terminal), pty);
    }
}

void
ide_terminal_page_feed (IdeTerminalPage *self,
                        const gchar     *message)
{
  g_return_if_fail (IDE_IS_TERMINAL_PAGE (self));
  g_return_if_fail (self->destroyed == FALSE);

  if (self->terminal != NULL)
    vte_terminal_feed (VTE_TERMINAL (self->terminal), message, -1);
}

void
ide_terminal_page_set_launcher (IdeTerminalPage     *self,
                                IdeTerminalLauncher *launcher)
{
  g_return_if_fail (IDE_IS_TERMINAL_PAGE (self));
  g_return_if_fail (!launcher || IDE_IS_TERMINAL_LAUNCHER (launcher));
  g_return_if_fail (self->destroyed == FALSE);

  if (g_set_object (&self->launcher, launcher))
    {
      ide_page_set_can_split (IDE_PAGE (self), TRUE);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAUNCHER]);
    }
}

const gchar *
ide_terminal_page_get_current_directory_uri (IdeTerminalPage *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_PAGE (self), NULL);
  g_return_val_if_fail (self->destroyed == FALSE, NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return vte_terminal_get_current_directory_uri (VTE_TERMINAL (self->terminal));
  G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * ide_terminal_page_get_launcher:
 * @self: a #IdeTerminalPage
 *
 * Gets the launcher for the page.
 *
 * Returns: (transfer none) (nullable): an #IdeTerminalLauncher or %NULL
 */
IdeTerminalLauncher *
ide_terminal_page_get_launcher (IdeTerminalPage *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_PAGE (self), NULL);

  return self->launcher;
}

gboolean
ide_terminal_page_has_exited (IdeTerminalPage *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_PAGE (self), FALSE);

  return self->exited;
}

/**
 * ide_terminal_page_get_terminal:
 * @self: a #IdeTerminalPage
 *
 * Gets the underlying terminal widget.
 *
 * Returns: (transfer none): an #IdeTerminal
 */
IdeTerminal *
ide_terminal_page_get_terminal (IdeTerminalPage *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_PAGE (self), NULL);

  return self->terminal;
}

IdeTerminalPage *
ide_terminal_page_new_completed (const char *title,
                                 const char *text,
                                 int         columns,
                                 int         rows)
{
  g_autofree char *copy = g_strdup (text);
  IdeTerminalPage *self;
  IdeLineReader reader;
  char *line;
  gsize line_len;

  self = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "title", title,
                       NULL);

  vte_terminal_set_input_enabled (VTE_TERMINAL (self->terminal), FALSE);

  self->close_on_exit = FALSE;
  self->manage_spawn = FALSE;
  g_clear_object (&self->launcher);
  self->respawn_on_exit = FALSE;
  self->exited = TRUE;

  if (columns > 0 && rows > 0)
    vte_terminal_set_size (VTE_TERMINAL (self->terminal), columns, rows);

  ide_line_reader_init (&reader, copy, -1);
  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      gboolean had_newline = line[line_len] != 0;

      line[line_len] = 0;
      ide_terminal_page_feed (self, line);

      if (had_newline)
        ide_terminal_page_feed (self, "\r\n");
    }

  return self;
}

G_GNUC_END_IGNORE_DEPRECATIONS
