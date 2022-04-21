/* ide-xml-proposal.c
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

#define G_LOG_DOMAIN "ide-xml-proposal"

#include "config.h"

#include <libide-sourceview.h>

#include "ide-xml-proposal.h"

struct _IdeXmlProposal
{
  GObject parent_instance;
  gchar *label;
  gchar *text;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeXmlProposal, ide_xml_proposal, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
ide_xml_proposal_finalize (GObject *object)
{
  IdeXmlProposal *self = (IdeXmlProposal *)object;

  g_clear_pointer (&self->label, g_free);
  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (ide_xml_proposal_parent_class)->finalize (object);
}

static void
ide_xml_proposal_class_init (IdeXmlProposalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_proposal_finalize;
}

static void
ide_xml_proposal_init (IdeXmlProposal *self)
{
}

IdeXmlProposal *
ide_xml_proposal_new (const gchar *text,
                      const gchar *label)
{
  IdeXmlProposal *self;

  self = g_object_new (IDE_TYPE_XML_PROPOSAL, NULL);
  self->text = g_strdup (text);
  self->label = g_strdup (label);

  return self;
}

const gchar *
ide_xml_proposal_get_label (IdeXmlProposal *self)
{
  return self->label;
}

const gchar *
ide_xml_proposal_get_text (IdeXmlProposal *self)
{
  return self->text;
}
