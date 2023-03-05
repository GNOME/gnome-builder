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
#include <pcre2.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <vte/vte.h>

#include <libide-gtk.h>

#include "ide-terminal-search.h"
#include "ide-terminal-search-private.h"

G_DEFINE_FINAL_TYPE (IdeTerminalSearch, ide_terminal_search, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_REGEX,
  PROP_CASE_SENSITIVE,
  PROP_USE_REGEX,
  PROP_WHOLE_WORDS,
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

  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  caseless = !self->match_case;

  if (self->use_regex)
    pattern = g_strdup (search_text);
  else
    pattern = g_regex_escape_string (search_text, -1);

  if (self->entire_word)
    {
      char *new_pattern;
      new_pattern = g_strdup_printf ("\\b%s\\b", pattern);
      g_free (pattern);
      pattern = new_pattern;
    }

  if (self->regex_caseless == caseless && g_strcmp0 (self->regex_pattern, pattern) == 0)
    return;

  g_clear_pointer (&self->regex, vte_regex_unref);
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

  update_sensitivity (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REGEX]);
}

static void
search_notify_text_cb (IdeSearchEntry    *search_entry,
                       GParamSpec        *pspec,
                       IdeTerminalSearch *self)
{
  update_regex (self);
}

static void
search_parameters_changed_cb (IdeTerminalSearch *self)
{
  update_regex (self);
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
  g_assert (IDE_IS_TERMINAL_SEARCH (self));
  g_assert (VTE_IS_TERMINAL (terminal));

  vte_terminal_search_set_wrap_around (terminal, self->wrap_word);
}

static void
search_overlay_search_cb (VteTerminal       *terminal,
                          gboolean           backward,
                          IdeTerminalSearch *self)
{
  g_assert (VTE_IS_TERMINAL (terminal));

  if (backward)
    vte_terminal_search_find_previous (terminal);
  else
    vte_terminal_search_find_next (terminal);
}

static gboolean
on_search_key_pressed_cb (GtkEventControllerKey *key,
                          guint                  keyval,
                          guint                  keycode,
                          GdkModifierType        state,
                          IdeTerminalSearch     *self)
{
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));
  g_assert (IDE_IS_TERMINAL_SEARCH (self));

  if ((state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) == 0)
    {
      switch (keyval)
        {
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          perform_search (self, FALSE);
          return TRUE;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          perform_search (self, TRUE);
          return TRUE;

        default:
          break;
        }
    }

  return FALSE;
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
ide_terminal_search_dispose (GObject *object)
{
  IdeTerminalSearch *self = IDE_TERMINAL_SEARCH (object);

  g_clear_pointer ((GtkWidget **)&self->grid, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_terminal_search_parent_class)->dispose (object);
}

static void
ide_terminal_search_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_terminal_search_parent_class)->finalize (object);
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

    case PROP_USE_REGEX:
      g_value_set_boolean (value, self->use_regex);
      break;

    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, self->match_case);
      break;

    case PROP_WHOLE_WORDS:
      g_value_set_boolean (value, self->entire_word);
      break;

    case PROP_WRAP_AROUND:
      g_value_set_boolean (value, self->wrap_word);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_search_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeTerminalSearch *self = IDE_TERMINAL_SEARCH (object);

  switch (prop_id)
    {
   case PROP_USE_REGEX:
      self->use_regex = !self->use_regex;
      search_parameters_changed_cb (self);
      break;

    case PROP_CASE_SENSITIVE:
      self->match_case = !self->match_case;
      search_parameters_changed_cb (self);
      break;

    case PROP_WHOLE_WORDS:
      self->entire_word = !self->entire_word;
      search_parameters_changed_cb (self);
      break;

    case PROP_WRAP_AROUND:
      self->wrap_word = !self->wrap_word;
      search_parameters_changed_cb (self);
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

  object_class->dispose = ide_terminal_search_dispose;
  object_class->finalize = ide_terminal_search_finalize;
  object_class->get_property = ide_terminal_search_get_property;
  object_class->set_property = ide_terminal_search_set_property;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "searchbar");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-search.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, grid);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, search_prev_button);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, search_next_button);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, close_button);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalSearch, search_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_search_key_pressed_cb);

  signals[SEARCH] =
    g_signal_new ("search",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  properties [PROP_REGEX] =
    g_param_spec_boxed ("regex", NULL, NULL,
                          VTE_TYPE_REGEX,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CASE_SENSITIVE] =
    g_param_spec_boolean ("case-sensitive", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_REGEX] =
    g_param_spec_boolean ("use-regex", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_WHOLE_WORDS] =
    g_param_spec_boolean ("whole-words", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_WRAP_AROUND] =
    g_param_spec_boolean ("wrap-around", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_install_property_action (widget_class, "search.case-sensitive", "case-sensitive");
  gtk_widget_class_install_property_action (widget_class, "search.use-regex", "use-regex");
  gtk_widget_class_install_property_action (widget_class, "search.whole-words", "whole-words");
  gtk_widget_class_install_property_action (widget_class, "search.wrap-around", "wrap-around");

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "page.search.hide", NULL);
}

static void
ide_terminal_search_init (IdeTerminalSearch *self)
{
  self->regex_caseless = FALSE;
  self->regex_pattern = 0;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->search_prev_button, "clicked", G_CALLBACK (search_button_clicked_cb), self);
  g_signal_connect (self->search_next_button, "clicked", G_CALLBACK (search_button_clicked_cb), self);
  g_signal_connect (self->search_entry, "notify::text", G_CALLBACK (search_notify_text_cb), self);
}

/**
 * ide_terminal_search_set_terminal:
 * @self: a #IdeTerminalSearch
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
 */
gboolean
ide_terminal_search_get_wrap_around (IdeTerminalSearch *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_SEARCH (self), FALSE);

  return self->wrap_word;
}
