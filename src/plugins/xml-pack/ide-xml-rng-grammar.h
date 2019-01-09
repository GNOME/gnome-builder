/* ide-xml-rng-grammar.h
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include "ide-xml-hash-table.h"
#include "ide-xml-rng-define.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_RNG_GRAMMAR (ide_xml_rng_grammar_get_type())

typedef struct _IdeXmlRngGrammar IdeXmlRngGrammar;

struct _IdeXmlRngGrammar
{
  volatile gint     ref_count;

  IdeXmlRngDefine  *start_defines;

  IdeXmlHashTable  *defines;
  IdeXmlHashTable  *refs;

  IdeXmlRngGrammar *parent;
  IdeXmlRngGrammar *next;
  IdeXmlRngGrammar *children;
};

GType                 ide_xml_rng_grammar_get_type  (void);
IdeXmlRngGrammar     *ide_xml_rng_grammar_new       (void);
void                  ide_xml_rng_grammar_add_child (IdeXmlRngGrammar *self,
                                                     IdeXmlRngGrammar *child);
void                  ide_xml_rng_grammar_dump_tree (IdeXmlRngGrammar *self);
IdeXmlRngGrammar     *ide_xml_rng_grammar_ref       (IdeXmlRngGrammar *self);
void                  ide_xml_rng_grammar_unref     (IdeXmlRngGrammar *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlRngGrammar, ide_xml_rng_grammar_unref)

G_END_DECLS
