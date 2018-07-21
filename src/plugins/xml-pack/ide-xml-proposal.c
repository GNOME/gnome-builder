/* ide-xml-proposal.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "ide-xml-proposal"

#include <ide.h>

#include "../gi/ide-gi-objects.h"

#include "ide-xml-proposal.h"

struct _IdeXmlProposal
{
  GObject               parent_instance;
  gchar                *header;
  gchar                *label;
  gchar                *text;
  gchar                *prefix;
  gpointer              data;
  gint                  insert_position;
  IdeXmlCompletionType  completion_type;
  IdeXmlPositionKind    kind;
};

G_DEFINE_TYPE_WITH_CODE (IdeXmlProposal, ide_xml_proposal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
ide_xml_proposal_finalize (GObject *object)
{
  IdeXmlProposal *self = (IdeXmlProposal *)object;

  if (self->completion_type == IDE_XML_COMPLETION_TYPE_UI_PROPERTY ||
      self->completion_type == IDE_XML_COMPLETION_TYPE_UI_SIGNAL ||
      self->completion_type == IDE_XML_COMPLETION_TYPE_UI_GTYPE)
    {
      g_clear_pointer (&self->data, ide_gi_base_unref);
    }

  g_clear_pointer (&self->header, g_free);
  g_clear_pointer (&self->label, g_free);
  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->prefix, g_free);

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
ide_xml_proposal_new (const gchar          *text,
                      const gchar          *header,
                      const gchar          *label,
                      const gchar          *prefix,
                      gpointer              data,
                      gint                  insert_position,
                      IdeXmlPositionKind    kind,
                      IdeXmlCompletionType  completion_type)
{
  IdeXmlProposal *self;
  
  self = g_object_new (IDE_TYPE_XML_PROPOSAL, NULL);
  self->text = g_strdup (text);
  self->header = g_strdup (header);
  self->label = g_strdup (label);
  self->prefix = g_strdup (prefix);
  self->data = data;
  self->insert_position = insert_position;
  self->completion_type = completion_type;
  self->kind = kind;

  return self;
}

const gchar *
ide_xml_proposal_get_header (IdeXmlProposal *self)
{
  return self->header;
}

gpointer
ide_xml_proposal_get_data (IdeXmlProposal *self)
{
  return self->data;
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

const gchar *
ide_xml_proposal_get_prefix (IdeXmlProposal *self)
{
  return self->prefix;
}

IdeXmlPositionKind
ide_xml_proposal_get_kind (IdeXmlProposal *self)
{
  return self->kind;
}

/**
 * ide_xml_proposal_insert_position:
 *
 * Get the insert position, relative to the text given by ide_xml_proposal_get_text.
 * Value -1 mean the cursor will be at the end of the inserted text,
 * otherwise, move n characters forward.
 *
 * Returns: index in utf8 characters quantity.
 */
gint
ide_xml_proposal_get_insert_position (IdeXmlProposal *self)
{
  return self->insert_position;
}

IdeXmlCompletionType
ide_xml_proposal_get_completion_type (IdeXmlProposal *self)
{
  return self->completion_type;
}
