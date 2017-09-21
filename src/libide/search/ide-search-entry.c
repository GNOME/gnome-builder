/* ide-search-entry.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-entry"

#include "ide-context.h"
#include "ide-macros.h"

#include "editor/ide-editor-perspective.h"
#include "search/ide-search-engine.h"
#include "search/ide-search-entry.h"
#include "search/ide-search-result.h"
#include "util/ide-gtk.h"
#include "workbench/ide-workbench.h"

#define DEFAULT_SEARCH_MAX 25

struct _IdeSearchEntry
{
  DzlSuggestionEntry parent_instance;
  guint max_results;
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
  IdeContext *context;
  const gchar *typed_text;

  g_assert (IDE_IS_SEARCH_ENTRY (self));

  if (NULL == (context = ide_widget_get_context (GTK_WIDGET (self))))
    return;

  typed_text = dzl_suggestion_entry_get_typed_text (DZL_SUGGESTION_ENTRY (self));

  if (ide_str_empty0 (typed_text))
    {
      dzl_suggestion_entry_set_model (DZL_SUGGESTION_ENTRY (self), NULL);
      return;
    }

  engine = ide_context_get_search_engine (context);

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
  g_autoptr(IdeSourceLocation) location = NULL;

  g_assert (IDE_IS_SEARCH_ENTRY (entry));
  g_assert (IDE_IS_SEARCH_RESULT (suggestion));

  location = ide_search_result_get_source_location (IDE_SEARCH_RESULT (suggestion));

  if (location != NULL)
    {
      IdeWorkbench *workbench = ide_widget_get_workbench (GTK_WIDGET (entry));
      IdePerspective *perspective = ide_workbench_get_perspective_by_name (workbench, "editor");

      ide_editor_perspective_focus_location (IDE_EDITOR_PERSPECTIVE (perspective), location);
    }
}

static void
ide_search_entry_unfocus (IdeSearchEntry *self)
{
  GtkWidget *toplevel;

  g_assert (IDE_IS_SEARCH_ENTRY (self));

  g_signal_emit_by_name (self, "hide-suggestions");
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gtk_widget_grab_focus (toplevel);
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

static void
ide_search_entry_class_init (IdeSearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
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

  bindings = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (bindings, GDK_KEY_Escape, 0, "unfocus", 0);
}

static void
ide_search_entry_init (IdeSearchEntry *self)
{
  self->max_results = DEFAULT_SEARCH_MAX;

  g_signal_connect (self, "changed", G_CALLBACK (ide_search_entry_changed), NULL);
}
