/* gbp-menu-search-result.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-menu-search-result"

#include "config.h"

#include <libide-sourceview.h>

#include "gbp-menu-search-result.h"

struct _GbpMenuSearchResult
{
  IdeSearchResult  parent_instance;
  char            *action;
  GVariant        *target;
};

G_DEFINE_FINAL_TYPE (GbpMenuSearchResult, gbp_menu_search_result, IDE_TYPE_SEARCH_RESULT)

static void
gbp_menu_search_result_activate (IdeSearchResult *result,
                                 GtkWidget       *last_focus)
{
  GbpMenuSearchResult *self = (GbpMenuSearchResult *)result;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MENU_SEARCH_RESULT (self));
  g_assert (GTK_IS_WIDGET (last_focus));

  g_debug ("Activating action \"%s\" starting from %s",
           self->action,
           G_OBJECT_TYPE_NAME (last_focus));

  gtk_widget_activate_action_variant (last_focus, self->action, self->target);

  IDE_EXIT;
}

static gboolean
gbp_menu_search_result_matches (IdeSearchResult *result,
                                const char      *query)
{
  const char *title;
  const char *subtitle;
  guint prio = 0;

  /* Only show items with descriptions otherwise they aren't really
   * meant for use in the global search.
   */
  if (!(title = ide_search_result_get_title (result)) ||
      !(subtitle = ide_search_result_get_subtitle (result)))
    return FALSE;

  if (gtk_source_completion_fuzzy_match (title, query, &prio) ||
      gtk_source_completion_fuzzy_match (subtitle, query, &prio))
    {
      ide_search_result_set_priority (result, prio);
      return TRUE;
    }

  return FALSE;
}

static void
gbp_menu_search_result_dispose (GObject *object)
{
  GbpMenuSearchResult *self = (GbpMenuSearchResult *)object;

  g_clear_pointer (&self->action, g_free);
  g_clear_pointer (&self->target, g_variant_unref);

  G_OBJECT_CLASS (gbp_menu_search_result_parent_class)->dispose (object);
}

static void
gbp_menu_search_result_class_init (GbpMenuSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *search_result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->dispose = gbp_menu_search_result_dispose;

  search_result_class->activate = gbp_menu_search_result_activate;
  search_result_class->matches = gbp_menu_search_result_matches;
}

static void
gbp_menu_search_result_init (GbpMenuSearchResult *self)
{
  ide_search_result_set_use_underline (IDE_SEARCH_RESULT (self), TRUE);
}

void
gbp_menu_search_result_set_action (GbpMenuSearchResult *self,
                                   const char          *action,
                                   GVariant            *target)
{
  g_return_if_fail (GBP_IS_MENU_SEARCH_RESULT (self));

  g_set_str (&self->action, action);
  g_clear_pointer (&self->target, g_variant_unref);
  self->target = target ? g_variant_ref_sink (target) : NULL;
}
