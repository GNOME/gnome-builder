/* ide-documentation-proposal.c
 *
 * Copyright (C) 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define G_LOG_DOMAIN "ide-documentation-proposal"

#include "documentation/ide-documentation-proposal.h"

typedef struct
{
  gchar       *header;
  gchar       *text;
  gchar       *uri;
} IdeDocumentationProposalPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeDocumentationProposal, ide_documentation_proposal, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_HEADER,
  PROP_TEXT,
  PROP_URI,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

IdeDocumentationProposal *
ide_documentation_proposal_new (const gchar *uri)
{
  return g_object_new (IDE_TYPE_DOCUMENTATION_PROPOSAL,
                       "uri", uri,
                       NULL);
}

const gchar *
ide_documentation_proposal_get_header (IdeDocumentationProposal *self)
{
  IdeDocumentationProposalPrivate *priv = ide_documentation_proposal_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCUMENTATION_PROPOSAL (self), NULL);

  return priv->header;
}

const gchar *
ide_documentation_proposal_get_text (IdeDocumentationProposal *self)
{
  IdeDocumentationProposalPrivate *priv = ide_documentation_proposal_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCUMENTATION_PROPOSAL (self), NULL);

  return priv->text;
}

const gchar *
ide_documentation_proposal_get_uri (IdeDocumentationProposal *self)
{
  IdeDocumentationProposalPrivate *priv = ide_documentation_proposal_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCUMENTATION_PROPOSAL (self), NULL);

  return priv->uri;
}


void
ide_documentation_proposal_set_header (IdeDocumentationProposal *self,
                                       const gchar              *header)
{
  IdeDocumentationProposalPrivate *priv = ide_documentation_proposal_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCUMENTATION_PROPOSAL (self));

  if (g_strcmp0 (priv->header, header) != 0)
    {
      g_free (priv->header);
      priv->header = g_strdup (header);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HEADER]);
    }
}

void
ide_documentation_proposal_set_text (IdeDocumentationProposal *self,
                                     const gchar              *text)
{
  IdeDocumentationProposalPrivate *priv = ide_documentation_proposal_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCUMENTATION_PROPOSAL (self));

    if (g_strcmp0 (priv->text, text) != 0)
      {
        g_free (priv->text);
        priv->text = g_strdup (text);
        g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TEXT]);
      }
}

void
ide_documentation_proposal_set_uri (IdeDocumentationProposal *self,
                                    const gchar              *uri)
{
  IdeDocumentationProposalPrivate *priv = ide_documentation_proposal_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCUMENTATION_PROPOSAL (self));

  priv->uri = g_strdup (uri);
}

static void
ide_documentation_proposal_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeDocumentationProposal *self = IDE_DOCUMENTATION_PROPOSAL (object);

  switch (prop_id)
    {
    case PROP_HEADER:
      g_value_set_string (value, ide_documentation_proposal_get_header (self));
      break;

    case PROP_TEXT:
      g_value_set_string (value, ide_documentation_proposal_get_text (self));
      break;

    case PROP_URI:
      g_value_set_string (value, ide_documentation_proposal_get_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_documentation_proposal_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdeDocumentationProposal *self = IDE_DOCUMENTATION_PROPOSAL (object);

  switch (prop_id)
    {
    case PROP_HEADER:
      ide_documentation_proposal_set_header (self, g_value_get_string (value));
      break;

    case PROP_TEXT:
      ide_documentation_proposal_set_text (self, g_value_get_string (value));
      break;

    case PROP_URI:
      ide_documentation_proposal_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_documentation_proposal_finalize (GObject *object)
{
  IdeDocumentationProposal *self = (IdeDocumentationProposal *)object;
  IdeDocumentationProposalPrivate *priv = ide_documentation_proposal_get_instance_private (self);

  g_clear_pointer (&priv->header, g_free);
  g_clear_pointer (&priv->text, g_free);
  g_clear_pointer (&priv->uri, g_free);

  G_OBJECT_CLASS (ide_documentation_proposal_parent_class)->finalize (object);
}

static void
ide_documentation_proposal_class_init (IdeDocumentationProposalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_documentation_proposal_finalize;
  object_class->get_property = ide_documentation_proposal_get_property;
  object_class->set_property = ide_documentation_proposal_set_property;

  properties [PROP_HEADER] =
    g_param_spec_string ("header",
                         "Header",
                         "Header",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "Text",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_URI] =
    g_param_spec_string ("uri",
                         "Uri",
                         "Uri",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

void
ide_documentation_proposal_init (IdeDocumentationProposal *self)
{
  IdeDocumentationProposalPrivate *priv = ide_documentation_proposal_get_instance_private (self);

  priv->header = NULL;
  priv->text = NULL;
  priv->uri = NULL;
}

