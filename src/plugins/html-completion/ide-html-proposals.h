/* ide-html-proposals.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_HTML_PROPOSALS (ide_html_proposals_get_type())

G_DECLARE_FINAL_TYPE (IdeHtmlProposals, ide_html_proposals, IDE, HTML_PROPOSALS, GObject)

typedef enum
{
  IDE_HTML_PROPOSAL_NONE,
  IDE_HTML_PROPOSAL_ELEMENT_START,
  IDE_HTML_PROPOSAL_ELEMENT_END,
  IDE_HTML_PROPOSAL_ATTRIBUTE_NAME,
  IDE_HTML_PROPOSAL_ATTRIBUTE_VALUE,
  IDE_HTML_PROPOSAL_CSS_PROPERTY,
} IdeHtmlProposalKind;

IdeHtmlProposals *ide_html_proposals_new      (void);
void              ide_html_proposals_refilter (IdeHtmlProposals    *self,
                                               IdeHtmlProposalKind  kind,
                                               const gchar         *element,
                                               const gchar         *casefold);

G_END_DECLS
