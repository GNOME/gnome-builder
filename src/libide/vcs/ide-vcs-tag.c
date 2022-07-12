/* ide-vcs-tag.c
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

#define G_LOG_DOMAIN "ide-vcs-tag"

#include "config.h"

#include "ide-vcs-tag.h"

G_DEFINE_INTERFACE (IdeVcsTag, ide_vcs_tag, G_TYPE_OBJECT)

static void
ide_vcs_tag_default_init (IdeVcsTagInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_string ("name", NULL, NULL, NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

/**
 * ide_vcs_tag_dup_name:
 * @self: an #IdeVcsTag
 *
 * Gets the name of the tag, which is used in various UI elements
 * to display to the user.
 *
 * Returns: (transfer full): a string containing the tag name
 */
char *
ide_vcs_tag_dup_name (IdeVcsTag *self)
{
  g_return_val_if_fail (IDE_IS_VCS_TAG (self), NULL);

  if (IDE_VCS_TAG_GET_IFACE (self)->dup_name)
    return IDE_VCS_TAG_GET_IFACE (self)->dup_name (self);

  return NULL;
}
