/* ide-gi-require.h
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib-object.h>

#include "ide-gi-types.h"

#include "ide-gi-namespace.h"

G_BEGIN_DECLS

struct _IdeGiRequire
{
  guint       ref_count;

  GHashTable *entries;
};

typedef enum
{
  IDE_GI_REQUIRE_COMP_EQUAL,
  IDE_GI_REQUIRE_COMP_LESS,
  IDE_GI_REQUIRE_COMP_LESS_OR_EQUAL,
  IDE_GI_REQUIRE_COMP_GREATER,
  IDE_GI_REQUIRE_COMP_GREATER_OR_EQUAL,
} IdeGiRequireComp;

typedef enum
{
  IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_SOURCE,
  IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_GREATEST,
} IdeGiRequireMergeStrategy;

typedef struct
{
  IdeGiRequireComp comp;

  guint16          major_version;
  guint16          minor_version;
} IdeGiRequireBound;

typedef struct _IdeGiRequireEntry
{
  IdeGiRequireBound  min;
  IdeGiRequireBound  max;
  guint              is_range : 1;
} IdeGiRequireEntry;

IdeGiRequire        *ide_gi_require_new                    (void);
IdeGiRequire        *ide_gi_require_ref                    (IdeGiRequire              *self);
void                 ide_gi_require_unref                  (IdeGiRequire              *self);

gboolean             ide_gi_require_add                    (IdeGiRequire              *self,
                                                            const gchar               *ns,
                                                            IdeGiRequireBound          bound);
gboolean             ide_gi_require_add_range              (IdeGiRequire              *self,
                                                            const gchar               *ns,
                                                            IdeGiRequireBound          min_bound,
                                                            IdeGiRequireBound          max_bound);
IdeGiRequire        *ide_gi_require_copy                   (IdeGiRequire              *self);
void                 ide_gi_require_dump                   (IdeGiRequire              *self);
void                 ide_gi_require_foreach                (IdeGiRequire              *self,
                                                            GHFunc                     func,
                                                            gpointer                   user_data);
IdeGiRequireEntry   *ide_gi_require_lookup                 (IdeGiRequire              *self,
                                                            const gchar               *ns);
gboolean             ide_gi_require_match                  (IdeGiRequire              *self,
                                                            const gchar               *ns,
                                                            guint16                    major_version,
                                                            guint16                    minor_version);
gboolean             ide_gi_require_match_namespace        (IdeGiRequire              *self,
                                                            IdeGiNamespace            *ns);
void                 ide_gi_require_merge                  (IdeGiRequire              *self,
                                                            IdeGiRequire              *added,
                                                            IdeGiRequireMergeStrategy  strategy);
void                 ide_gi_require_merge_namespace        (IdeGiRequire              *self,
                                                            IdeGiRequireMergeStrategy  strategy,
                                                            const gchar               *ns,
                                                            guint16                    major_version,
                                                            guint16                    minor_version);
gboolean             ide_gi_require_remove                 (IdeGiRequire              *self,
                                                            const gchar               *ns);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiRequire, ide_gi_require_unref)

G_END_DECLS
