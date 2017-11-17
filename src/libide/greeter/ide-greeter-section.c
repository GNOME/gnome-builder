/* ide-greeter-section.c
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

#define G_LOG_DOMAIN "ide-greeter-section"

#include "greeter/ide-greeter-section.h"

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
   *
   * The "project-activated" signal is emitted when a project has been
   * selected by the user in the section.
   *
   * Use ide_greeter_section_emit_project_activated() to activate
   * this signal.
   *
   * Since: 3.28
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
 * Since: 3.28
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
 * Since: 3.28
 */
void
ide_greeter_section_filter (IdeGreeterSection *self,
                            DzlPatternSpec    *spec)
{
  g_return_if_fail (IDE_IS_GREETER_SECTION (self));

  if (IDE_GREETER_SECTION_GET_IFACE (self)->filter)
    IDE_GREETER_SECTION_GET_IFACE (self)->filter (self, spec);
}

void
ide_greeter_section_emit_project_activated (IdeGreeterSection *self,
                                            IdeProjectInfo    *project_info)
{
  g_return_if_fail (IDE_IS_GREETER_SECTION (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));

  g_signal_emit (self, signals [PROJECT_ACTIVATED], 0, project_info);
}
