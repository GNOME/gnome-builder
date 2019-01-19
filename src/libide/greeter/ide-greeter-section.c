/* ide-greeter-section.c
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

#define G_LOG_DOMAIN "ide-greeter-section"

#include "config.h"

#include "ide-greeter-section.h"

G_DEFINE_INTERFACE (IdeGreeterSection, ide_greeter_section, GTK_TYPE_WIDGET)

enum {
  PROJECT_ACTIVATED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_greeter_section_default_init (IdeGreeterSectionInterface *iface)
{
  /**
   * IdeGreeterSection::project-activated:
   * @self: an #IdeGreeterSection
   * @project_info: an #IdeProjectInfo
   *
   * The "project-activated" signal is emitted when a project has been
   * selected by the user in the section.
   *
   * Use ide_greeter_section_emit_project_activated() to activate
   * this signal.
   *
   * Since: 3.32
   */
  signals [PROJECT_ACTIVATED] =
    g_signal_new ("project-activated",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeGreeterSectionInterface, project_activated),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_PROJECT_INFO);
}

/**
 * ide_greeter_section_get_priority:
 * @self: an #IdeGreeterSection
 *
 * Get the priority of the section. The lowest integral value is
 * sorted first in the list of sections.
 *
 * Returns: the priority for the section
 *
 * Since: 3.32
 */
gint
ide_greeter_section_get_priority (IdeGreeterSection *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_SECTION (self), 0);

  if (IDE_GREETER_SECTION_GET_IFACE (self)->get_priority)
    return IDE_GREETER_SECTION_GET_IFACE (self)->get_priority (self);

  return 0;
}

/**
 * ide_greeter_section_filter:
 * @self: a #IdeGreeterSection
 * @spec: (nullable): a #DzlPatternSpec or %NULL
 *
 * Refilter the visibile items based on the current search.
 *
 * Returns: %TRUE if at least one element matched.
 *
 * Since: 3.32
 */
gboolean
ide_greeter_section_filter (IdeGreeterSection *self,
                            DzlPatternSpec    *spec)
{
  g_return_val_if_fail (IDE_IS_GREETER_SECTION (self), FALSE);

  if (IDE_GREETER_SECTION_GET_IFACE (self)->filter)
    return IDE_GREETER_SECTION_GET_IFACE (self)->filter (self, spec);

  return FALSE;
}

void
ide_greeter_section_emit_project_activated (IdeGreeterSection *self,
                                            IdeProjectInfo    *project_info)
{
  g_return_if_fail (IDE_IS_GREETER_SECTION (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));

  g_signal_emit (self, signals [PROJECT_ACTIVATED], 0, project_info);
}

/**
 * ide_greeter_section_activate_first:
 * @self: a #IdeGreeterSection
 *
 * Active the first item in the section. This happens when the user
 * hits Enter within the search box to select the first visible item
 * in the search result set.
 *
 * Ensure the given item is visible before activating it.
 *
 * If no item matched, then return %FALSE.
 *
 * Returns: %TRUE if an item was activated
 *
 * Since: 3.32
 */
gboolean
ide_greeter_section_activate_first (IdeGreeterSection *self)
{
  g_return_val_if_fail (IDE_IS_GREETER_SECTION (self), FALSE);

  if (IDE_GREETER_SECTION_GET_IFACE (self)->activate_first)
    return IDE_GREETER_SECTION_GET_IFACE (self)->activate_first (self);

  return FALSE;
}

void
ide_greeter_section_set_selection_mode (IdeGreeterSection *self,
                                        gboolean           selection_mode)
{
  g_return_if_fail (IDE_IS_GREETER_SECTION (self));

  if (IDE_GREETER_SECTION_GET_IFACE (self)->set_selection_mode)
    IDE_GREETER_SECTION_GET_IFACE (self)->set_selection_mode (self, selection_mode);
}

void
ide_greeter_section_delete_selected (IdeGreeterSection *self)
{
  g_assert (IDE_IS_GREETER_SECTION (self));

  if (IDE_GREETER_SECTION_GET_IFACE (self)->delete_selected)
    IDE_GREETER_SECTION_GET_IFACE (self)->delete_selected (self);
}

void
ide_greeter_section_purge_selected (IdeGreeterSection *self)
{
  g_assert (IDE_IS_GREETER_SECTION (self));

  if (IDE_GREETER_SECTION_GET_IFACE (self)->purge_selected)
    IDE_GREETER_SECTION_GET_IFACE (self)->purge_selected (self);
}
