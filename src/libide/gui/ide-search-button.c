/* ide-search-button.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-button"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-search.h>

#include "ide-gui-global.h"
#include "ide-search-button.h"
#include "ide-workbench.h"

#define DEFAULT_SEARCH_MAX 25
#define I_ g_intern_string

struct _IdeSearchButton
{
  DzlSuggestionButton parent_instance;
};

static const DzlShortcutEntry shortcuts[] = {
  { "org.gnome.builder.workspace.global-search",
    0, NULL,
    N_("Workspace shortcuts"),
    N_("Search"),
    N_("Focus to the global search entry") },
};

G_DEFINE_TYPE (IdeSearchButton, ide_search_button, DZL_TYPE_SUGGESTION_BUTTON)

static void
search_entry_search_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeSearchEngine *engine = (IdeSearchEngine *)object;
  g_autoptr(DzlSuggestionEntry) entry = user_data;
  g_autoptr(GListModel) suggestions = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));
  g_assert (IDE_IS_SEARCH_ENGINE (engine));

  suggestions = ide_search_engine_search_finish (engine, result, &error);

  if (error != NULL)
    {
      /* TODO: Elevate to workbench message once we have that capability */
      g_warning ("%s", error->message);
      return;
    }

  g_assert (suggestions != NULL);
  g_assert (G_IS_LIST_MODEL (suggestions));
  g_assert (g_type_is_a (g_list_model_get_item_type (suggestions), DZL_TYPE_SUGGESTION));

  dzl_suggestion_entry_set_model (entry, suggestions);
}


static void
search_entry_changed (DzlSuggestionEntry *entry)
{
  IdeSearchEngine *engine;
  IdeWorkbench *workbench;
  const gchar *typed_text;

  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));

  workbench = ide_widget_get_workbench (GTK_WIDGET (entry));
  engine = ide_workbench_get_search_engine (workbench);
  typed_text = dzl_suggestion_entry_get_typed_text (entry);

  if (dzl_str_empty0 (typed_text))
    dzl_suggestion_entry_set_model (entry, NULL);
  else
    ide_search_engine_search_async (engine,
                                    typed_text,
                                    DEFAULT_SEARCH_MAX,
                                    NULL,
                                    search_entry_search_cb,
                                    g_object_ref (entry));
}

static void
search_popover_position_func (DzlSuggestionEntry *entry,
                              GdkRectangle       *area,
                              gboolean           *is_absolute,
                              gpointer            user_data)
{
  gint new_width;

  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));
  g_assert (area != NULL);
  g_assert (is_absolute != NULL);
  g_assert (user_data == NULL);

#define RIGHT_MARGIN 6

  /* We want the search area to be the right 2/5ths of the window, with a bit
   * of margin on the popover.
   */

  dzl_suggestion_entry_window_position_func (entry, area, is_absolute, NULL);

  new_width = (area->width * 2 / 5);
  area->x += area->width - new_width;
  area->width = new_width - RIGHT_MARGIN;
  area->y -= 3;

#undef RIGHT_MARGIN
}

static void
unfocus_action (GSimpleAction *action,
                GVariant      *param,
                gpointer       user_data)
{
  IdeSearchButton *self = IDE_SEARCH_BUTTON (user_data);
  DzlSuggestionEntry *entry;
  GtkWidget *toplevel;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_SEARCH_BUTTON (self));

  entry = dzl_suggestion_button_get_entry (DZL_SUGGESTION_BUTTON (self));
  g_signal_emit_by_name (entry, "hide-suggestions");

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gtk_widget_grab_focus (toplevel);
  gtk_entry_set_text (GTK_ENTRY (entry), "");
}

static void
shortcut_grab_focus (GtkWidget *widget,
                     gpointer   user_data)
{
  IdeSearchButton *self = IDE_SEARCH_BUTTON (user_data);

  gtk_widget_grab_focus (GTK_WIDGET (self));
}

static void
suggestion_activated (DzlSuggestionEntry *entry,
                      DzlSuggestion      *suggestion)
{
  GtkWidget *toplevel;
  GtkWidget *focus;

  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));
  g_assert (IDE_IS_SEARCH_RESULT (suggestion));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (entry));
  focus = GTK_WIDGET (ide_workspace_get_most_recent_page (IDE_WORKSPACE (toplevel)));
  if (focus == NULL)
    focus = GTK_WIDGET (entry);

  ide_search_result_activate (IDE_SEARCH_RESULT (suggestion), focus);
}

static gboolean
search_entry_focus_in (GtkEntry      *entry,
                       GdkEventFocus *focus,
                       gpointer       user_data)
{
  IdeWorkbench *workbench;
  IdeSearchEngine *engine;

  g_assert (GTK_IS_ENTRY (entry));
  g_assert (focus != NULL);

  /* Load search engine if it is not already */
  workbench = ide_widget_get_workbench (GTK_WIDGET (entry));
  engine = ide_workbench_get_search_engine (workbench);
  (void)engine;

  return GDK_EVENT_PROPAGATE;
}

static void
ide_search_button_class_init (IdeSearchButtonClass *klass)
{
}

static void
ide_search_button_init (IdeSearchButton *self)
{
  DzlSuggestionEntry *entry = dzl_suggestion_button_get_entry (DZL_SUGGESTION_BUTTON (self));
  DzlShortcutController *controller;
  g_autoptr(GSimpleActionGroup) group = NULL;
  static GActionEntry actions[] = {
    { "unfocus", unfocus_action },
  };

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "search",
                                  G_ACTION_GROUP (group));

  dzl_gtk_widget_add_style_class (GTK_WIDGET (entry), "global-search");
  g_signal_connect (entry, "changed", G_CALLBACK (search_entry_changed), NULL);
  g_signal_connect (entry, "focus-in-event", G_CALLBACK (search_entry_focus_in), NULL);
  g_signal_connect (entry, "suggestion-activated", G_CALLBACK (suggestion_activated), NULL);
  dzl_suggestion_entry_set_position_func (entry, search_popover_position_func, NULL, NULL);

  controller = dzl_shortcut_controller_find (GTK_WIDGET (entry));

  dzl_shortcut_controller_add_command_callback (controller,
                                                I_("org.gnome.builder.workspace.global-search"),
                                                "<Primary>period",
                                                DZL_SHORTCUT_PHASE_CAPTURE | DZL_SHORTCUT_PHASE_GLOBAL,
                                                shortcut_grab_focus, self, NULL);
  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.workspace.unfocus"),
                                              "Escape",
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              "search.unfocus");

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             shortcuts,
                                             G_N_ELEMENTS (shortcuts),
                                             GETTEXT_PACKAGE);
}

GtkWidget *
ide_search_button_new (void)
{
  return g_object_new (IDE_TYPE_SEARCH_BUTTON, NULL);
}
