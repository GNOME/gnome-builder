/* ide-terminal-search.c
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

#define G_LOG_DOMAIN "ide-terminal-search"
#define PCRE2_CODE_UNIT_WIDTH 0

#include "config.h"

#include <fcntl.h>
#include <glib/gi18n.h>
#include <pcre2.h>
#include <stdlib.h>
#include <vte/vte.h>
#include <unistd.h>

#include "ide-terminal-search.h"
#include "ide-terminal-search-private.h"

G_DEFINE_TYPE (IdeTerminalSearch, ide_terminal_search, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_REGEX,
  PROP_WRAP_AROUND,
  LAST_PROP
};

enum {
  SEARCH,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GParamSpec *properties[LAST_PROP];

static void
update_sensitivity (IdeTerminalSearch *self)
{
  gboolean can_search;

  can_search = self->regex != NULL;

  gtk_widget_set_sensitive (GTK_WIDGET (self->search_prev_button), can_search);
  gtk_widget_set_sensitive (GTK_WIDGET (self->search_next_button), can_search);
}

static void
perform_search (IdeTerminalSearch *self,
                gboolean       backward)
{
  g_assert (IDE_IS_TERMINAL_SEARCH (self));

  if (self->regex == NULL)
    return;

  g_signal_emit (self, signals[SEARCH], 0, backward);
}

static void
close_clicked_cb (GtkButton        *button,
                  IdeTerminalSearch *self)
{
  g_assert (IDE_IS_TERMINAL_SEARCH (self));

  gtk_revealer_set_reveal_child(self->search_revealer, FALSE);
}

static void
search_button_clicked_cb (GtkButton        *button,
                          IdeTerminalSearch *self)
{
  g_assert (IDE_IS_TERMINAL_SEARCH (self));

  perform_search (self, button == self->search_prev_button);
}

static void
update_regex (IdeTerminalSearch *self)
{
  const char *search_text;
  gboolean caseless;
  g_autofree gchar *pattern = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TERMINAL_SEARCH (self));

  search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
  caseless = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->match_case_checkbutton));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->regex_checkbutton)))
    pattern = g_strdup (search_text);
  else
    pattern = g_regex_escape_string (search_text, -1);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->entire_word_checkbutton)))
    {
      char *new_pattern;
      new_pattern = g_strdup_printf ("\\b%s\\b", pattern);
      g_free (pattern);
      pattern = new_pattern;
    }

  if (self->regex_caseless == caseless &&
      g_strcmp0 (self->regex_pattern, pattern) == 0)
    return;

  if (self->regex)
    {
      vte_regex_unref (self->regex);
    }

  g_clear_pointer (&self->regex_pattern, g_free);

  if (search_text[0] != '\0')
    {
      guint32 compile_flags;

      compile_flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_MULTILINE;
      if (caseless)
        compile_flags |= PCRE2_CASELESS;

      self->regex = vte_regex_new_for_search (pattern, -1, compile_flags, &error);

      if (self->regex != NULL)
        self->regex_pattern = g_steal_pointer (&pattern);
    }
  else
    self->regex = NULL;

  update_sensitivity (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REGEX]);
}

static void
search_text_changed_cb (IdeTaggedEntry  *search_entry,
                        IdeTerminalSearch *self)
{
  update_regex (self);
}

static void
search_parameters_changed_cb (GtkToggleButton *button,
                              IdeTerminalSearch  *self)
{
  update_regex (self);
}

static void
wrap_around_toggled_cb (GtkToggleButton *button,
                        IdeTerminalSearch  *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WRAP_AROUND]);
}

static void
reveal_options_changed_cb (GtkToggleButton *button,
                           IdeTerminalSearch  *self)
{
  g_assert (IDE_IS_TERMINAL_SEARCH (self));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->reveal_button)))
    gtk_widget_set_visible (GTK_WIDGET (self->search_options), TRUE);
  else
    gtk_widget_set_visible (GTK_WIDGET (self->search_options), FALSE);
}

static void
search_overlay_notify_regex_cb (VteTerminal    *terminal,
                                GParamSpec     *pspec G_GNUC_UNUSED,
                                IdeTerminalSearch *self)
{
  VteRegex *regex;

  g_assert (IDE_IS_TERMINAL_SEARCH (self));
  g_assert (VTE_IS_TERMINAL (terminal));

  regex = ide_terminal_search_get_regex (self);
  vte_terminal_search_set_regex (VTE_TERMINAL (terminal), regex, 0);
}

static void
search_overlay_notify_wrap_around_cb (VteTerminal    *terminal,
                                      GParamSpec     *pspec G_GNUC_UNUSED,
                                      IdeTerminalSearch *self)
{
  gboolean wrap;

  g_assert (IDE_IS_TERMINAL_SEARCH (self));
  g_assert (VTE_IS_TERMINAL (terminal));

  wrap = ide_terminal_search_get_wrap_around (self);
  vte_terminal_search_set_wrap_around (terminal, wrap);
}

static void
search_overlay_search_cb (VteTerminal    *terminal,
                          gboolean        backward,
                          IdeTerminalSearch *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));

  if (backward)
    vte_terminal_search_find_previous (terminal);
  else
    vte_terminal_search_find_next (terminal);
}

static void
search_revealer_cb (GtkRevealer       *search_revealer,
                    GParamSpec        *pspec,
                    IdeTerminalSearch *self)
{
  g_assert (IDE_IS_TERMINAL_SEARCH (self));

  if (gtk_revealer_get_child_revealed (search_revealer))
    {
      if (vte_terminal_get_has_selection (self->terminal))
        {
          vte_terminal_copy_primary (self->terminal);
          self->selected_text = gtk_clipboard_wait_for_text (self->clipboard);
          gtk_entry_set_text (GTK_ENTRY (self->search_entry), self->selected_text);
        }
      gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
    }
  else
    {
      gtk_revealer_set_reveal_child (self->search_revealer, FALSE);
    }
}

static void
ide_terminal_search_connect_terminal (IdeTerminalSearch *self)
{
  g_signal_connect_object (self,
                           "notify::regex",
                           G_CALLBACK (search_overlay_notify_regex_cb),
                           self->terminal,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self,
                           "notify::wrap-around",
                           G_CALLBACK (search_overlay_notify_wrap_around_cb),
                           self->terminal,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self,
                           "search",
                           G_CALLBACK (search_overlay_search_cb),
                           self->terminal,
                           G_CONNECT_SWAPPED);
}

static void
ide_terminal_search_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeTerminalSearch *self = IDE_TERMINAL_SEARCH (object);

  switch (prop_id)
    {
    case PROP_REGEX:
      g_value_set_boxed (value, ide_terminal_search_get_regex (self));
      break;

    case PROP_WRAP_AROUND:
      g_value_set_boolean (value, ide_terminal_search_get_wrap_around (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_search_class_init (IdeTerminalSearchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_terminal_search_get_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-search.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, search_prev_button);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, search_next_button);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, close_button);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, match_case_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, entire_word_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, regex_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, wrap_around_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, reveal_button);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, search_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, search_options);

  signals[SEARCH] =
    g_signal_new ("search",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  properties[PROP_REGEX] =
    g_param_spec_boxed ("regex", NULL, NULL,
                        VTE_TYPE_REGEX,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_WRAP_AROUND] =
    g_param_spec_boolean ("wrap-around", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_terminal_search_init (IdeTerminalSearch *self)
{
  self->regex_caseless = FALSE;
  self->regex_pattern = 0;

  self->clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->search_prev_button, "clicked", G_CALLBACK (search_button_clicked_cb), self);
  g_signal_connect (self->search_next_button, "clicked", G_CALLBACK (search_button_clicked_cb), self);
  g_signal_connect (self->close_button, "clicked", G_CALLBACK (close_clicked_cb), self);
  g_signal_connect (self->search_entry, "search-changed", G_CALLBACK (search_text_changed_cb), self);
  g_signal_connect (self->match_case_checkbutton, "toggled", G_CALLBACK (search_parameters_changed_cb), self);
  g_signal_connect (self->entire_word_checkbutton, "toggled", G_CALLBACK (search_parameters_changed_cb), self);
  g_signal_connect (self->regex_checkbutton, "toggled", G_CALLBACK (search_parameters_changed_cb), self);
  g_signal_connect (self->reveal_button, "toggled", G_CALLBACK (reveal_options_changed_cb), self);
  g_signal_connect (self->wrap_around_checkbutton, "toggled", G_CALLBACK (wrap_around_toggled_cb), self);
  g_signal_connect (self->search_revealer, "notify::child-revealed", G_CALLBACK (search_revealer_cb), self);
}

/**
 * ide_terminal_search_set_terminal:
 * @self: a #IdeTerminalSearch
 *
 * Since: 3.32
 */
void
ide_terminal_search_set_terminal (IdeTerminalSearch *self,
                                 VteTerminal      *terminal)
{
  g_assert (IDE_IS_TERMINAL_SEARCH (self));

  self->terminal = terminal;
  ide_terminal_search_connect_terminal (self);
}

/**
 * ide_terminal_search_get_regex:
 * @self: a #IdeTerminalSearch
 *
 * Returns: (transfer none) (nullable): a #VteRegex or %NULL.
 *
 * Since: 3.32
 */
VteRegex *
ide_terminal_search_get_regex (IdeTerminalSearch *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_SEARCH (self), NULL);

  return self->regex;
}

/**
 * ide_terminal_search_get_wrap_around:
 * @self: a #IdeTerminalSearch
 *
 *
 * Since: 3.32
 */
gboolean
ide_terminal_search_get_wrap_around (IdeTerminalSearch *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_SEARCH (self), FALSE);

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->wrap_around_checkbutton));
}

/**
 * ide_terminal_search_get_revealer:
 * @self: a #IdeTerminalSearch
 *
 * Gets the revealer widget used for the terminal search.
 *
 * Returns: (transfer none): a #GtkRevealer
 *
 * Since: 3.32
 */
GtkRevealer *
ide_terminal_search_get_revealer (IdeTerminalSearch *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_SEARCH (self), FALSE);

  return self->search_revealer;
}
