/* gbp-devhelp-search.c
 *
 * Copyright 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define G_LOG_DOMAIN "gbp-devhelp-search"
#define MAX_SEARCH 100

#include <fcntl.h>
#include <glib/gi18n.h>
#include <libide-editor.h>
#include <webkit2/webkit2.h>

#include "gbp-devhelp-search.h"
#include "gbp-devhelp-search-private.h"

G_DEFINE_TYPE (GbpDevhelpSearch, gbp_devhelp_search, GTK_TYPE_BIN)

static void
close_clicked_cb (GtkButton        *button,
                  GbpDevhelpSearch *self)
{
  g_assert (GBP_IS_DEVHELP_SEARCH (self));

  webkit_find_controller_search_finish (self->web_controller);
  gtk_revealer_set_reveal_child (self->search_revealer, FALSE);
}

static void
search_button_clicked_cb (GtkButton        *button,
                          GbpDevhelpSearch *self)
{
  g_assert (GBP_IS_DEVHELP_SEARCH (self));

  if (button == self->search_prev_button)
    webkit_find_controller_search_previous (self->web_controller);
  else
    webkit_find_controller_search_next (self->web_controller);
}

static void
search_text_changed_cb (IdeTaggedEntry   *search_entry,
                        GbpDevhelpSearch *self)
{
  const char *search_text;

  g_assert (GBP_IS_DEVHELP_SEARCH (self));

  search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
  webkit_find_controller_search (self->web_controller,
                                 search_text,
                                 WEBKIT_FIND_OPTIONS_BACKWARDS |
                                 WEBKIT_FIND_OPTIONS_WRAP_AROUND |
                                 WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE,
                                 MAX_SEARCH);
}

static void
search_revealer_cb (GtkRevealer      *search_revealer,
                    GParamSpec       *pspec G_GNUC_UNUSED,
                    GbpDevhelpSearch *self)
{
  g_assert (GBP_IS_DEVHELP_SEARCH (self));

  if (gtk_revealer_get_child_revealed (search_revealer))
    {
      g_free (self->selected_text);
      self->selected_text = gtk_clipboard_wait_for_text (self->clipboard);
      if (self->selected_text != NULL)
        gtk_entry_set_text (GTK_ENTRY (self->search_entry), self->selected_text);

      gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
    }
  else
    gtk_revealer_set_reveal_child (self->search_revealer, FALSE);
}

static void
gbp_devhelp_search_class_init (GbpDevhelpSearchClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/devhelp/gbp-devhelp-search.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpSearch, search_prev_button);
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpSearch, search_next_button);
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpSearch, close_button);
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpSearch, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpSearch, search_revealer);
}

static void
gbp_devhelp_search_init (GbpDevhelpSearch *self)
{

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->search_prev_button, "clicked", G_CALLBACK (search_button_clicked_cb), self);
  g_signal_connect (self->search_next_button, "clicked", G_CALLBACK (search_button_clicked_cb), self);
  g_signal_connect (self->close_button, "clicked", G_CALLBACK (close_clicked_cb), self);
  g_signal_connect (self->search_entry, "search-changed", G_CALLBACK (search_text_changed_cb), self);
  g_signal_connect (self->search_revealer, "notify::child-revealed", G_CALLBACK (search_revealer_cb), self);
}

void
gbp_devhelp_search_set_devhelp (GbpDevhelpSearch     *self,
                                WebKitFindController *web_controller,
                                GtkClipboard         *clipboard)
{
  g_return_if_fail (GBP_IS_DEVHELP_SEARCH (self));

  self->clipboard = clipboard;
  self->web_controller = web_controller;
}

GtkRevealer *
gbp_devhelp_search_get_revealer (GbpDevhelpSearch *self)
{
  g_return_val_if_fail (GBP_IS_DEVHELP_SEARCH (self), NULL);

  return self->search_revealer;
}
