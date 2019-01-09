/* ide-search-entry.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-entry"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-core.h>
#include <libide-search.h>

#include "ide-gui-global.h"
#include "ide-search-entry.h"
#include "ide-workbench.h"

#define DEFAULT_SEARCH_MAX 25
#define I_ g_intern_string

struct _IdeSearchEntry
{
  DzlSuggestionEntry parent_instance;
  guint              max_results;
};

G_DEFINE_TYPE (IdeSearchEntry, ide_search_entry, DZL_TYPE_SUGGESTION_ENTRY)

enum {
  PROP_0,
  PROP_MAX_RESULTS,
  N_PROPS
};

enum {
  UNFOCUS,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

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
ide_search_entry_search_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeSearchEngine *engine = (IdeSearchEngine *)object;
  g_autoptr(IdeSearchEntry) self = user_data;
  g_autoptr(GListModel) suggestions = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SEARCH_ENTRY (self));
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

  dzl_suggestion_entry_set_model (DZL_SUGGESTION_ENTRY (self), suggestions);
}

static void
ide_search_entry_changed (IdeSearchEntry *self)
{
  IdeSearchEngine *engine;
  IdeWorkbench *workbench;
  const gchar *typed_text;

  g_assert (IDE_IS_SEARCH_ENTRY (self));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  engine = ide_workbench_get_search_engine (workbench);
  typed_text = dzl_suggestion_entry_get_typed_text (DZL_SUGGESTION_ENTRY (self));

  if (dzl_str_empty0 (typed_text))
    dzl_suggestion_entry_set_model (DZL_SUGGESTION_ENTRY (self), NULL);
  else
    ide_search_engine_search_async (engine,
                                    typed_text,
                                    self->max_results,
                                    NULL,
                                    ide_search_entry_search_cb,
                                    g_object_ref (self));
}

static void
suggestion_activated (DzlSuggestionEntry *entry,
                      DzlSuggestion      *suggestion)
{
  g_assert (IDE_IS_SEARCH_ENTRY (entry));
  g_assert (IDE_IS_SEARCH_RESULT (suggestion));

  /* TODO: Get last focus from workspace */
  ide_search_result_activate (IDE_SEARCH_RESULT (suggestion), GTK_WIDGET (entry));

  /* Chain up to properly clear entry buffer */
  if (DZL_SUGGESTION_ENTRY_CLASS (ide_search_entry_parent_class)->suggestion_activated)
    DZL_SUGGESTION_ENTRY_CLASS (ide_search_entry_parent_class)->suggestion_activated (entry, suggestion);
}

static void
ide_search_entry_unfocus (IdeSearchEntry *self)
{
  GtkWidget *toplevel;

  g_assert (IDE_IS_SEARCH_ENTRY (self));

  g_signal_emit_by_name (self, "hide-suggestions");
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gtk_widget_grab_focus (toplevel);
  gtk_entry_set_text (GTK_ENTRY (self), "");
}

static void
ide_search_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeSearchEntry *self = IDE_SEARCH_ENTRY (object);

  switch (prop_id)
    {
    case PROP_MAX_RESULTS:
      g_value_set_uint (value, self->max_results);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeSearchEntry *self = IDE_SEARCH_ENTRY (object);

  switch (prop_id)
    {
    case PROP_MAX_RESULTS:
      self->max_results = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static DzlShortcutEntry shortcuts[] = {
  { "org.gnome.builder.workspace.global-search",
    0, NULL,
    NC_("shortcut window", "Workspace shortcuts"),
    NC_("shortcut window", "Search"),
    NC_("shortcut window", "Focus to the global search entry") },
};

static void
ide_search_entry_init_shortcuts (IdeSearchEntry *self)
{
  DzlShortcutController *controller;

  g_assert (IDE_IS_SEARCH_ENTRY (self));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_callback (controller,
                                                I_("org.gnome.builder.workspace.global-search"),
                                                "<Primary>period",
                                                DZL_SHORTCUT_PHASE_CAPTURE | DZL_SHORTCUT_PHASE_GLOBAL,
                                                (GtkCallback)gtk_widget_grab_focus,
                                                NULL,
                                                NULL);

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             shortcuts,
                                             G_N_ELEMENTS (shortcuts),
                                             GETTEXT_PACKAGE);
}

static void
ide_search_entry_class_init (IdeSearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  DzlSuggestionEntryClass *suggestion_entry_class = DZL_SUGGESTION_ENTRY_CLASS (klass);
  GtkBindingSet *bindings;

  object_class->get_property = ide_search_entry_get_property;
  object_class->set_property = ide_search_entry_set_property;

  suggestion_entry_class->suggestion_activated = suggestion_activated;

  properties [PROP_MAX_RESULTS] =
    g_param_spec_uint ("max-results",
                       "Max Results",
                       "Maximum number of search results to display",
                       1,
                       1000,
                       DEFAULT_SEARCH_MAX,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [UNFOCUS] =
    g_signal_new_class_handler ("unfocus",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_search_entry_unfocus),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-search-entry.ui");

  bindings = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (bindings, GDK_KEY_Escape, 0, "unfocus", 0);
}

static void
ide_search_entry_init (IdeSearchEntry *self)
{
  self->max_results = DEFAULT_SEARCH_MAX;

  gtk_widget_init_template (GTK_WIDGET (self));

  dzl_gtk_widget_add_style_class (GTK_WIDGET (self), "global-search");

  g_signal_connect (self,
                    "changed",
                    G_CALLBACK (ide_search_entry_changed),
                    NULL);

  dzl_suggestion_entry_set_position_func (DZL_SUGGESTION_ENTRY (self),
                                          search_popover_position_func,
                                          NULL,
                                          NULL);

  ide_search_entry_init_shortcuts (self);
}
