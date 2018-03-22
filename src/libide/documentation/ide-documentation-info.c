/* ide-documentation-info.c
 *
 * Copyright © 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define G_LOG_DOMAIN "ide-documentation-info"

#include "config.h"

#include "documentation/ide-documentation-info.h"
#include "documentation/ide-documentation-proposal.h"

struct _IdeDocumentationInfo
{
  GObject                  parent_instance;

  gchar                   *input;
  GPtrArray               *proposals;

  IdeDocumentationContext  context;
};

G_DEFINE_TYPE (IdeDocumentationInfo, ide_documentation_info, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_INPUT,
  PROP_CONTEXT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

IdeDocumentationInfo *
ide_documentation_info_new (const gchar            *input,
                            IdeDocumentationContext context)
{

  return g_object_new (IDE_TYPE_DOCUMENTATION_INFO,
                       "input", input,
                       "context", context,
                       NULL);
}

void
ide_documentation_info_take_proposal (IdeDocumentationInfo        *self,
                                      IdeDocumentationProposal    *proposal)
{
  g_return_if_fail (IDE_IS_DOCUMENTATION_INFO (self));
  g_return_if_fail (IDE_IS_DOCUMENTATION_PROPOSAL (proposal));

  g_ptr_array_add (self->proposals, proposal);
}

/**
 * ide_documentation_info_get_proposal:
 * @self: An #IdeDocumentationInfo
 * @index: the number of the proposal
 *
 * Requests proposal for the index.
 *
 * Returns: (transfer none): An #IdeDocumentationProposal
 */

IdeDocumentationProposal *
ide_documentation_info_get_proposal (IdeDocumentationInfo *self,
                                     guint                 index)
{
  g_return_val_if_fail (IDE_IS_DOCUMENTATION_INFO (self), NULL);
  g_return_val_if_fail (self->proposals != NULL, NULL);
  g_return_val_if_fail (self->proposals->len > index, NULL);

  return g_ptr_array_index (self->proposals, index);
}

static void
ide_documentation_info_finalize (GObject *object)
{
  IdeDocumentationInfo *self = (IdeDocumentationInfo *)object;

  g_clear_pointer (&self->input, g_free);
  g_clear_pointer (&self->proposals, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_documentation_info_parent_class)->finalize (object);
}

gchar *
ide_documentation_info_get_input (IdeDocumentationInfo *self)
{
  g_return_val_if_fail (IDE_IS_DOCUMENTATION_INFO (self), NULL);

  return self->input;
}

IdeDocumentationContext
ide_documentation_info_get_context (IdeDocumentationInfo *self)
{
  g_return_val_if_fail (IDE_IS_DOCUMENTATION_INFO (self), IDE_DOCUMENTATION_CONTEXT_NONE);

  return self->context;
}

guint
ide_documentation_info_get_size (IdeDocumentationInfo *self)
{
  g_return_val_if_fail (IDE_IS_DOCUMENTATION_INFO (self), 0);

  return self->proposals != NULL ? self->proposals->len : 0;
}

static void
ide_documentation_info_set_input (IdeDocumentationInfo *self,
                                  const gchar          *input)
{
  g_return_if_fail (IDE_IS_DOCUMENTATION_INFO (self));
  g_return_if_fail (self->input == NULL);

  self->input = g_strdup (input);
}

static void
ide_documentation_info_set_context (IdeDocumentationInfo    *self,
                                    IdeDocumentationContext  context)
{
  g_return_if_fail (IDE_IS_DOCUMENTATION_INFO (self));
  g_return_if_fail (self->context == IDE_DOCUMENTATION_CONTEXT_NONE);

  self->context = context;
}

static void
ide_documentation_info_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeDocumentationInfo *self = IDE_DOCUMENTATION_INFO (object);

  switch (prop_id)
    {
    case PROP_INPUT:
      g_value_set_string (value, ide_documentation_info_get_input (self));
      break;

    case PROP_CONTEXT:
      g_value_set_int (value, ide_documentation_info_get_context (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_documentation_info_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeDocumentationInfo *self = IDE_DOCUMENTATION_INFO (object);

  switch (prop_id)
    {
    case PROP_INPUT:
      ide_documentation_info_set_input (self, g_value_get_string (value));
      break;

    case PROP_CONTEXT:
      ide_documentation_info_set_context (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_documentation_info_class_init (IdeDocumentationInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_documentation_info_finalize;
  object_class->get_property = ide_documentation_info_get_property;
  object_class->set_property = ide_documentation_info_set_property;

  properties [PROP_INPUT] =
    g_param_spec_string ("input",
                         "Input",
                         "Input",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONTEXT] =
    g_param_spec_int ("context",
                      "Context",
                      "Context",
                      IDE_DOCUMENTATION_CONTEXT_NONE,
                      IDE_DOCUMENTATION_CONTEXT_LAST,
                      IDE_DOCUMENTATION_CONTEXT_NONE,
                      (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_documentation_info_init (IdeDocumentationInfo *self)
{
  self->proposals = g_ptr_array_new_with_free_func (g_object_unref);
  self->context = IDE_DOCUMENTATION_CONTEXT_NONE;
  self->input = NULL;
}
