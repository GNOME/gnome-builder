/* ide-gi-base.c
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <stdio.h>

#include "ide-gi-base.h"

G_DEFINE_BOXED_TYPE (IdeGiBase,
                     ide_gi_base,
                     ide_gi_base_ref,
                     ide_gi_base_unref)


/* Keep in Sync with ide-gi-types.h enumerations */
static const gchar * STABILITY_NAME [] =
{
  "STABLE", // IDE_GI_STABILITY_STABLE
  "UNSTABLE", // IDE_GI_STABILITY_UNSTABLE
  "PRIVATE" // IDE_GI_STABILITY_PRIVATE
};

void
ide_gi_base_dump (IdeGiBase *self)
{
  const gchar *stability_name;
  const gchar *blob_name;

  g_return_if_fail (self != NULL);

  stability_name = STABILITY_NAME [ide_gi_base_stability (self)];
  blob_name = ide_gi_blob_get_name (ide_gi_base_get_object_type (self));

  g_print ("object:%s type:%s\n"
           "version:%s deprecated version:%s\n"
           "introspectable:%d\n"
           "deprecated:%d\n"
           "stability:%s\n",
           ide_gi_base_get_name (self),
           blob_name,
           ide_gi_base_get_version (self),
           ide_gi_base_get_deprecated_version (self),
           ide_gi_base_is_introspectable (self),
           ide_gi_base_is_deprecated (self),
           stability_name
         );
}

IdeGiBlobType
ide_gi_base_get_object_type (IdeGiBase *self)
{
  g_assert (self != NULL);

  return self->type;
}

const gchar *
ide_gi_base_get_name (IdeGiBase *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_get_string (self->ns, self->common_blob->name);
}

gchar *
ide_gi_base_get_qualified_name (IdeGiBase *self)
{
  gchar *qname;

  g_return_val_if_fail (self != NULL, NULL);

  qname = g_strconcat (ide_gi_namespace_get_name (self->ns),
                       ".",
                       ide_gi_base_get_name (self),
                       NULL);

  return qname;
}

const gchar *
ide_gi_base_get_version (IdeGiBase *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_get_string (self->ns, self->common_blob->version);
}

const gchar *
ide_gi_base_get_deprecated_version (IdeGiBase *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_get_string (self->ns, self->common_blob->deprecated_version);
}

gboolean
ide_gi_base_is_deprecated (IdeGiBase *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->common_blob->deprecated;
}

gboolean
ide_gi_base_is_introspectable (IdeGiBase *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->common_blob->introspectable;
}

IdeGiStability
ide_gi_base_stability (IdeGiBase *self)
{
  g_return_val_if_fail (self != NULL, IDE_GI_STABILITY_UNSTABLE);

  return self->common_blob->stability;
}

IdeGiDoc *
ide_gi_base_get_doc (IdeGiBase *self)
{
  IdeGiDoc *doc;
  gint32  offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = self->common_blob->doc;
  if (offset ==  -1)
    return NULL;

  doc = ide_gi_doc_new (self->ns, offset);
  return doc;
}

/**
 * ide_gi_base_get_namespace:
 *
 * @self: a  #IdeGiBase
 *
 * Returns: (transfer full): a #IdeGiNamespace
 */
IdeGiNamespace *
ide_gi_base_get_namespace (IdeGiBase *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_ref (self->ns);
}

const gchar *
ide_gi_base_get_namespace_name (IdeGiBase *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return ide_gi_namespace_get_name (self->ns);
}

IdeGiBase *
ide_gi_base_new (IdeGiNamespace *ns,
                 IdeGiBlobType   type,
                 gint32          offset)
{
  IdeGiObjectConstructor constructor;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  if ((constructor = ide_gi_blob_get_constructor (type)))
    return constructor (ns, type, offset);
  else
    return NULL;
}

static void
ide_gi_base_free (IdeGiBase *self)
{
  IdeGiObjectDestructor destructor;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  destructor = ide_gi_blob_get_destructor (self->type);
  g_assert (destructor != NULL);

  destructor (self);
}

IdeGiBase *
ide_gi_base_ref (IdeGiBase *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_base_unref (IdeGiBase *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_base_free (self);
}
