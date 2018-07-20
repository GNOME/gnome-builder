/* ide-gi-version.c
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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
#define G_LOG_DOMAIN "ide-gi-version"

#include <dazzle.h>
#include <ide.h>
#include <string.h>

#include "ide-gi.h"
#include "ide-gi-types.h"

#include "ide-gi-index.h"
#include "ide-gi-namespace.h"
#include "ide-gi-namespace-private.h"

#include "ide-gi-version-private.h"
#include "ide-gi-version.h"

enum {
  PROP_0,
  PROP_CACHE_DIR,
  PROP_COUNT,
  PROP_INDEX,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void initable_iface_init  (GInitableIface      *iface);
static void async_initable_init  (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGiVersion, ide_gi_version, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_init))

typedef struct
{
  IdeGiVersion     *version;
  IdeGiNamespaceId  id;
} NsRequest;

static NsState *
namespace_state_new (IdeGiVersion     *self,
                     IdeGiNamespaceId  ns_id)
{
  NsState *state;

  state = g_slice_new0 (NsState);
  state->once = (GOnce)G_ONCE_INIT;

  return state;
}

static void
namespace_state_free (gpointer data)
{
  NsState *state = (NsState *)data;

  g_assert (data != NULL);

  g_slice_free (NsState, state);
}

static gboolean
is_object_type_match_flags (IdeGiBlobType          type,
                            IdeGiCompleteRootFlags flags)
{
  static IdeGiCompleteRootFlags type_to_flag [] =
    {
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_UNKNOW
      IDE_GI_COMPLETE_ROOT_ALIAS,     // IDE_GI_BLOB_TYPE_ALIAS
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_ARRAY
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_BOXED
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_CALLBACK
      IDE_GI_COMPLETE_ROOT_CLASS,     // IDE_GI_BLOB_TYPE_CLASS
      IDE_GI_COMPLETE_ROOT_CONSTANT,  // IDE_GI_BLOB_TYPE_CONSTANT
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_CONSTRUCTOR
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_DOC,IDE_TRACE_MSG
      IDE_GI_COMPLETE_ROOT_ENUM,      // IDE_GI_BLOB_TYPE_ENUM
      IDE_GI_COMPLETE_ROOT_FIELD,     // IDE_GI_BLOB_TYPE_FIELD
      IDE_GI_COMPLETE_ROOT_FUNCTION,  // IDE_GI_BLOB_TYPE_FUNCTION
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_HEADER
      IDE_GI_COMPLETE_ROOT_INTERFACE, // IDE_GI_BLOB_TYPE_INTERFACE
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_METHOD
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_PARAMETER
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_PROPERTYIdeGiNsIndexHeader
      IDE_GI_COMPLETE_ROOT_RECORD,    // IDE_GI_BLOB_TYPE_RECORD
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_SIGNAL
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_TYPE
      IDE_GI_COMPLETE_ROOT_UNION,     // IDE_GI_BLOB_TYPE_UNION
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_VALUE
      IDE_GI_COMPLETE_TYPE_NONE,      // IDE_GI_BLOB_TYPE_VFUNC
    };

  return type_to_flag[type] & flags;
}

static gpointer
init_namespace (gpointer data)
{
  IdeGiNamespace *ns;
  NsRequest *request = (NsRequest *)data;
  IdeGiNamespaceId id;

  id = request->id;
  ns = _ide_gi_namespace_new (request->version, id);

  IDE_TRACE_MSG ("ch Version @%i namespace id:%x %d %d",
                 request->version->version_count,
                 id.offset64b,
                 id.major_version,
                 id.minor_version);

  return ns;
}

static IdeGiNamespace *
get_namespace_from_id (IdeGiVersion     *self,
                       IdeGiNamespaceId  ns_id)
{
  NsRequest request = (NsRequest){self, ns_id};
  NsState *state;

  g_assert (IDE_IS_GI_VERSION (self));

  state = g_hash_table_lookup (self->ns_table, &ns_id);
  g_assert (state != NULL);

  g_once (&state->once, init_namespace, &request);
  return ide_gi_namespace_ref (state->once.retval);
}

static const gchar *
get_namespace_name_from_id (IdeGiVersion     *self,
                            IdeGiNamespaceId  ns_id)
{
  NamespaceChunk chunk;

  g_assert (IDE_IS_GI_VERSION (self));

  chunk = _ide_gi_version_get_namespace_chunk_from_id (self, ns_id);
  return _namespacechunk_get_namespace (&chunk);
}

/**
 * ide_gi_version_lookup_namespace:
 *
 * @self: a #IdeGiVersion.
 * @name: a namespace name.
 * @ns_major_version: namespace major version.
 * @ns_minor_version: namespace minor version.
 *
 * Returns: (nullable) (transfer full): a #IdeGiNamespace or %NULL if none.
 */
IdeGiNamespace *
ide_gi_version_lookup_namespace (IdeGiVersion *self,
                                 const gchar  *name,
                                 guint16       ns_major_version,
                                 guint16       ns_minor_version)
{
  DtPayload *payload;
  guint64 *payloads;
  guint nb_payloads;
  guint nb_dt_payloads;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  if (ide_gi_flat_radix_tree_lookup (self->index_dt, name, &payloads, &nb_payloads))
    {
      payload = (DtPayload *)payloads;
      nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;

      for (guint i = 0; i < nb_dt_payloads; i++, payload++)
        if (payload->type & IDE_GI_PREFIX_TYPE_NAMESPACE &&
            payload->id.major_version == ns_major_version &&
            payload->id.minor_version == ns_minor_version)
          {
            return get_namespace_from_id (self, payload->id);
          }
    }

  return NULL;
}

/**
 * ide_gi_version_lookup_namespaces:
 *
 * @self: a #IdeGiVersion.
 * @name: a namespace name.
 * @req: requirements for the searched namespace or %NULL
 *
 * Get an array of all the matchinng namespaces.
 * the @name need to b e exact.
 *
 * Returns: (nullable) (transfer full) (element-type Ide.GiNamespace):
 *          a #GPtrArray of #IdeGiNamespace or %NULL if none.
 */
GPtrArray *
ide_gi_version_lookup_namespaces (IdeGiVersion *self,
                                  const gchar  *name,
                                  IdeGiRequire *req)
{
  GPtrArray *ar = NULL;
  guint64 *payloads;
  guint nb_payloads;
  DtPayload *payload;
  guint nb_dt_payloads;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  if (ide_gi_flat_radix_tree_lookup (self->index_dt, name, &payloads, &nb_payloads))
    {
      payload = (DtPayload *)payloads;
      nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;

      for (guint i = 0; i < nb_dt_payloads; i++, payload++)
        {
          if ((payload->type & IDE_GI_PREFIX_TYPE_NAMESPACE) &&
              req != NULL &&
              ide_gi_require_match (req, name, payload->id.major_version, payload->id.minor_version))
            {
              if (ar == NULL)
                ar = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_gi_namespace_unref);

              g_ptr_array_add (ar, get_namespace_from_id (self, payload->id));
            }
        }
    }

  return ar;
}

/**
 * ide_gi_version_lookup_root_object:
 *
 * @self: an #IdeGiVersion.
 * @qname: a qualified name namespace.object
 * @ns_major_version: namespace major version.
 * @ns_minor_version: namespace minor version.
 *
 * Returns: (nullable) (transfer full): the corresponding #IdeGiBase in the version or %NULL.
 */
IdeGiBase *
ide_gi_version_lookup_root_object (IdeGiVersion *self,
                                   const gchar  *qname,
                                   guint16       ns_major_version,
                                   guint16       ns_minor_version)
{
  g_autofree gchar *searched_ns = NULL;
  g_autoptr(IdeGiFlatRadixTree) ro_tree = NULL;
  g_autoptr(IdeGiNamespace) ns = NULL;
  const gchar *ptr;
  guint64 *ro_tree_data;
  RoTreePayload *payload;
  guint64 *payloads;
  guint nb_payloads;
  guint nb_rot_payloads;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);

  if (dzl_str_empty0 (qname) || NULL == (ptr = strchr (qname, '.')))
    return NULL;

  searched_ns = g_strndup (qname, ptr - qname);
  ns = ide_gi_version_lookup_namespace (self,
                                        searched_ns,
                                        ns_major_version,
                                        ns_minor_version);
  if (ns == NULL)
    return NULL;

  ro_tree = ide_gi_flat_radix_tree_new ();
NO_CAST_ALIGN_PUSH
  ro_tree_data = (guint64 *)ns->head_header + ns->head_header->ro_tree_offset64b;
NO_CAST_ALIGN_POP
  ide_gi_flat_radix_tree_init (ro_tree, ro_tree_data, ns->head_header->ro_tree_size64b);

  if (ide_gi_flat_radix_tree_lookup (ro_tree, ptr + 1, &payloads, &nb_payloads))
    {
      payload = (RoTreePayload *)payloads;
      nb_rot_payloads = nb_payloads / RO_TREE_PAYLOAD_N64_SIZE;

      g_assert (nb_rot_payloads == 1);
      return ide_gi_base_new (ns, payload->type, payload->offset);
    }

  return NULL;
}

/**
 * ide_gi_version_lookup_gtype:
 * @self: #IdeGiVersion
 * @req: #IdeGiRequire or %NULL
 * @name: name to search
 *
 * Return an object corresponding to the GType searched.
 * Can be a class, an interface, an enum, a boxed...
 *
 * Returns: (nullable) (transfer full): a #IdeGiBase object.
 */
IdeGiBase *
ide_gi_version_lookup_gtype (IdeGiVersion *self,
                             IdeGiRequire *req,
                             const gchar  *name)
{
  guint64 *payloads;
  guint nb_payloads;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  if (name == NULL || *name == '\0')
    return NULL;

  if (ide_gi_flat_radix_tree_lookup (self->index_dt, name, &payloads, &nb_payloads))
    {
      DtPayload *payload = (DtPayload *)payloads;
      guint nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;
      IdeGiBase *base_object;

      for (guint i = 0; i < nb_dt_payloads; i++, payload++)
        {
          g_autoptr(IdeGiNamespace) ns = NULL;

          if (payload->type & IDE_GI_PREFIX_TYPE_GTYPE)
            {
              if (req != NULL &&
                  !ide_gi_require_match (req,
                                         get_namespace_name_from_id (self, payload->id),
                                         payload->id.major_version,
                                         payload->id.minor_version))
                continue;

              ns = get_namespace_from_id (self, payload->id);
              base_object =  ide_gi_base_new (ns,
                                              payload->object_type,
                                              payload->object_offset);
              g_assert (base_object != NULL);
              return base_object;
            }
        }
    }

  return NULL;
}

IdeGiBase *
ide_gi_version_lookup_gtype_in_ns (IdeGiVersion   *self,
                                   IdeGiNamespace *ns,
                                   const gchar    *name)
{
  guint64 *payloads;
  guint nb_payloads;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  if (name == NULL || *name == '\0')
    return NULL;

  if (ide_gi_flat_radix_tree_lookup (self->index_dt, name, &payloads, &nb_payloads))
    {
      DtPayload *payload = (DtPayload *)payloads;
      guint nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;
      IdeGiNamespaceId id;
      IdeGiBase *base_object;

      id = _ide_gi_namespace_get_id (ns);
      for (guint i = 0; i < nb_dt_payloads; i++, payload++)
        {
          if (payload->type & IDE_GI_PREFIX_TYPE_GTYPE)
            {
              if (payload->id.value != id.value)
                continue;

              base_object =  ide_gi_base_new (ns,
                                              payload->object_type,
                                              payload->object_offset);
              g_assert (base_object != NULL);
              return base_object;
            }
        }
    }

  return NULL;
}

typedef struct {
  IdeGiVersion           *version;
  IdeGiRequire           *req;
  GArray                 *ar;
  IdeGiCompleteRootFlags  flags;
} CompleteGtypeState;

static void
complete_gtype_filter_func (const gchar *word,
                            guint64     *payloads,
                            guint        nb_payloads,
                            gpointer     user_data)
{
  CompleteGtypeState *state = (CompleteGtypeState *)user_data;
  DtPayload *payload;
  guint nb_dt_payloads;

  g_assert (word != NULL && *word != '\0');
  g_assert (state != NULL);
  g_assert (payloads != NULL);
  g_assert (nb_payloads > 0);

  payload = (DtPayload *)payloads;
  nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;

  for (guint i = 0; i < nb_dt_payloads; i++, payload++)
    if ((payload->type & IDE_GI_PREFIX_TYPE_GTYPE) &&
        is_object_type_match_flags (payload->object_type, state->flags))
      {
        IdeGiNamespace *ns;
        IdeGiCompleteGtypeItem item;

        if (state->req != NULL &&
            !ide_gi_require_match (state->req,
                                   get_namespace_name_from_id (state->version, payload->id),
                                   payload->id.major_version,
                                   payload->id.minor_version))
          continue;

        ns = get_namespace_from_id (state->version, payload->id);
        item = (IdeGiCompleteGtypeItem) {.word = g_strdup (word),
                                         .object_type = payload->object_type,
                                         .object_offset = payload->object_offset,
                                         .is_buildable = payload->is_buildable,
                                         .ns = ns,
                                         .major_version = payload->id.major_version,
                                         .minor_version = payload->id.minor_version};

        g_array_append_val (state->ar, item);
      }
}

/**
 * ide_gi_version_complete_gtype:
 *
 * @self: a #IdeGiVersion.
 * @req; a #IdeGiRequire or %NULL for no requirements.
 * @flags: #IdeGiCompleteRootFlags flags to filter the results.
 * @case_sensitive: %TRUE if the search is case sensitive, %FALSE otherwise.
 * @word: the partial word to search.
 *
 * Returns: (transfer full) (element-type Ide.GiCompleteGtypeItem):
 *          a #GArray of #IdeGiCompleteGtypeItem items.
 */
GArray *
ide_gi_version_complete_gtype (IdeGiVersion           *self,
                               IdeGiRequire           *req,
                               IdeGiCompleteRootFlags  flags,
                               gboolean                case_sensitive,
                               const gchar            *word)
{
  GArray *ar;
  CompleteGtypeState state;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  ar = g_array_new (FALSE, TRUE, sizeof (IdeGiCompleteGtypeItem));
  g_array_set_clear_func (ar, (GDestroyNotify)ide_gi_complete_gtype_item_clear);
  state = (CompleteGtypeState) { self, req, ar, flags };

  ide_gi_flat_radix_tree_complete_custom (self->index_dt,
                                          word,
                                          FALSE,
                                          case_sensitive,
                                          complete_gtype_filter_func,
                                          &state);

  return ar;
}

typedef struct {
  IdeGiVersion            *version;
  IdeGiRequire            *req;
  IdeGiNamespace          *ns;
  GArray                  *ar;
  const gchar             *word;
  IdeGiCompleteRootFlags   flags;
} CompleteRootObjectState;

/* TODO: resolve collision between root objects with same name */
static void
complete_complete_root_objects_filter_func (const gchar *word,
                                            guint64     *payloads,
                                            guint        nb_payloads,
                                            gpointer     user_data)
{
  CompleteRootObjectState *state = (CompleteRootObjectState *)user_data;
  RoTreePayload *payload;
  guint nb_rot_payloads;

  g_assert (word != NULL && *word != '\0');
  g_assert (state != NULL);
  g_assert (payloads != NULL);
  g_assert (nb_payloads > 0);

  payload = (RoTreePayload *)payloads;
  nb_rot_payloads = nb_payloads / RO_TREE_PAYLOAD_N64_SIZE;

  for (guint i = 0; i < nb_rot_payloads; i++, payload++)
    if (is_object_type_match_flags (payload->type, state->flags))
      {
        IdeGiBase *base_object;
        IdeGiCompleteObjectItem item;

        base_object = ide_gi_base_new (state->ns, payload->type, payload->offset);
        item = (IdeGiCompleteObjectItem) {.word = g_strdup (word),
                                          .type = payload->type,
                                          .object = base_object};
        g_array_append_val (state->ar, item);
      }
}

/**
 * ide_gi_version_complete_root_objects:
 *
 * @self: a #IdeGiVersion.
 * @req; a #IdeGiRequire or %NULL for no requirements.
 * @ns: a #IdeGiNamespace
 * @flags: #IdeGiCompleteRootFlags flags to filter the results.
 * @case_sensitive: %TRUE if the search is case sensitive, %FALSE otherwise.
 * @word: the partial word to search.
 *
 * Returns: (transfer full) (element-type Ide.GiCompleteObjectItem):
 *          a #GArray of #IdeGiCompleteObjectItem items.
 */
GArray *
ide_gi_version_complete_root_objects (IdeGiVersion           *self,
                                      IdeGiRequire           *req,
                                      IdeGiNamespace         *ns,
                                      IdeGiCompleteRootFlags  flags,
                                      gboolean                case_sensitive,
                                      const gchar            *word)
{
  GArray *ar;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);
  g_return_val_if_fail (ns != NULL, NULL);

  ar = g_array_new (FALSE, TRUE, sizeof (IdeGiCompleteObjectItem));
  g_array_set_clear_func (ar, (GDestroyNotify)ide_gi_complete_object_item_clear);

  if (req != NULL && !ide_gi_require_match_namespace (req, ns))
    return ar;

  if (ns->head_header->ro_tree_size64b > 0)
    {
      g_autoptr(IdeGiFlatRadixTree) ro_tree = NULL;
      CompleteRootObjectState state = (CompleteRootObjectState) { self, req, ns, ar, word, flags };
      guint64 *ro_tree_data;

      ro_tree = ide_gi_flat_radix_tree_new ();
NO_CAST_ALIGN_PUSH
      ro_tree_data = (guint64 *)ns->head_header + ns->head_header->ro_tree_offset64b;
NO_CAST_ALIGN_POP
      ide_gi_flat_radix_tree_init (ro_tree, ro_tree_data, ns->head_header->ro_tree_size64b);
      ide_gi_flat_radix_tree_complete_custom (ro_tree,
                                              word,
                                              FALSE,
                                              case_sensitive,
                                              complete_complete_root_objects_filter_func,
                                              &state);
    }

  return ar;
}

typedef struct {
  IdeGiVersion    *version;
  IdeGiRequire    *req;
  GArray          *ar;
  IdeGiPrefixType  flags;
} CompletePrefixState;

static void
complete_prefix_filter_func (const gchar *word,
                             guint64     *payloads,
                             guint        nb_payloads,
                             gpointer     user_data)
{
  CompletePrefixState *state = (CompletePrefixState *)user_data;
  DtPayload *payload;
  guint nb_dt_payloads;

  g_assert (word != NULL && *word != '\0');
  g_assert (state != NULL);
  g_assert (payloads != NULL);
  g_assert (nb_payloads > 0);

  payload = (DtPayload *)payloads;
  nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;

  for (guint i = 0; i < nb_dt_payloads; i++, payload++)
    {
      const gchar *ns_name = get_namespace_name_from_id (state->version, payload->id);

      if (payload->type & state->flags &&
          (state->req == NULL || ide_gi_require_match (state->req,
                                                       ns_name,
                                                       payload->id.major_version,
                                                       payload->id.minor_version)))
        {
          IdeGiCompletePrefixItem item =
            (IdeGiCompletePrefixItem) {.word = g_strdup (word),
                                       .type = payload->type,
                                       .major_version = payload->id.major_version,
                                       .minor_version = payload->id.minor_version,
                                       .ns = get_namespace_from_id (state->version, payload->id)};
          g_array_append_val (state->ar, item);
        }
    }
}

/**
 * ide_gi_version_complete_prefix:
 *
 * @self: a #IdeGiVersion
 * @req; a #IdeGiRequire or %NULL for no requirements
 * @flags: #IdeGiPrefixType flags to filter the results
 * @get_prefix: select the completion or prefix mode
 * @case_sensitive: %TRUE if the search is case sensitive, %FALSE otherwise
 * @word: the partial word to search
 *
 * if get_prefix == %FALSE: we get the names equal or longer than @word.
 * if get_prefix == %TRUE: we get the names equal or shorter than @word.
 *
 * Returns: (transfer full) (element-type Ide.GiCompletePrefixItem):
 *          a #GArray of #IdeGiCompletePrefixItem items.
 */
GArray *
ide_gi_version_complete_prefix (IdeGiVersion    *self,
                                IdeGiRequire    *req,
                                IdeGiPrefixType  flags,
                                gboolean         get_prefix,
                                gboolean         case_sensitive,
                                const gchar     *word)
{
  GArray *ar;
  CompletePrefixState state;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  ar = g_array_new (FALSE, TRUE, sizeof (IdeGiCompletePrefixItem));
  g_array_set_clear_func (ar, (GDestroyNotify)ide_gi_complete_prefix_item_clear);
  state = (CompletePrefixState){ self, req, ar, flags };

  ide_gi_flat_radix_tree_complete_custom (self->index_dt,
                                          word,
                                          get_prefix,
                                          case_sensitive,
                                          complete_prefix_filter_func,
                                          &state);

  return ar;
}

static void
namespaces_basenames_filter_func (const gchar *word,
                                  guint64     *payloads,
                                  guint        nb_payloads,
                                  gpointer     user_data)
{
  GPtrArray *ar = (GPtrArray *)user_data;
  DtPayload *payload;
  guint nb_dt_payloads;

  g_assert (word != NULL && *word != '\0');
  g_assert (payloads != NULL);
  g_assert (nb_payloads > 0);
  g_assert (ar != NULL);

  payload = (DtPayload *)payloads;
  nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;

  for (guint i = 0; i < nb_dt_payloads; i++, payload++)
    if (payload->type & IDE_GI_PREFIX_TYPE_NAMESPACE)
      {
        gchar *name;

        if (payload->id.no_minor_version)
          name = g_strdup_printf ("%s-%d@%d%s",
                                  word,
                                  payload->id.major_version,
                                  payload->id.file_version,
                                  INDEX_NAMESPACE_EXTENSION);
        else
          name = g_strdup_printf ("%s-%d.%d@%d%s",
                                  word,
                                  payload->id.major_version,
                                  payload->id.minor_version,
                                  payload->id.file_version,
                                  INDEX_NAMESPACE_EXTENSION);

        g_ptr_array_add (ar, name);
      }
}

/**
 * ide_gi_version_get_namespaces_basenames:
 * @self: #IdeGiVersion
 *
 * Return an array of namespaces name strings.
 * (the same names as the files containing the namespace datas)
 *
 * Returns: (transfer full) (element-type utf8): a #GPtrArray
 */
GPtrArray *
ide_gi_version_get_namespaces_basenames (IdeGiVersion *self)
{
  GPtrArray *ar;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  ar = g_ptr_array_new_with_free_func (g_free);
  if (self->index_dt != NULL)
    ide_gi_flat_radix_tree_foreach (self->index_dt,
                                    namespaces_basenames_filter_func,
                                    ar);
  return ar;
}

static const gchar *
ide_gi_version_get_string (IdeGiVersion *self,
                           guint32       offset)
{
  const gchar *base;

  g_assert (IDE_IS_GI_VERSION (self));
  g_assert (self->index_header != NULL);

NO_CAST_ALIGN_PUSH
  base = (const gchar *)((guint64 *)self->index_header + self->index_header->strings_offset64b);
NO_CAST_ALIGN_POP

  return base + offset;
}

/* Here we compute the hightest namespaces versions
 * and ccollect the nnamespacces ids.
 */
static void
compute_namespaces_info_filter_func (const gchar *word,
                                     guint64     *payloads,
                                     guint        nb_payloads,
                                     gpointer     user_data)
{
  IdeGiVersion *self = (IdeGiVersion *)user_data;
  IdeGiRequire *req = self->req_highest_versions;
  DtPayload *payload;
  guint nb_dt_payloads;
  guint16 max_minor_version = 0;
  guint16 max_major_version = 0;
  gboolean has_namespace = FALSE;

  g_assert (word != NULL && *word != '\0');
  g_assert (payloads != NULL);
  g_assert (nb_payloads > 0);
  g_assert (req != NULL);

  payload = (DtPayload *)payloads;
  nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;

  for (guint i = 0; i < nb_dt_payloads; i++, payload++)
    {
      if (payload->type & IDE_GI_PREFIX_TYPE_NAMESPACE)
        {
          IdeGiNamespaceId *key;
          NsState *state;

          has_namespace = TRUE;

          if (payload->id.major_version > max_major_version ||
              (payload->id.major_version == max_major_version &&
               payload->id.minor_version > max_minor_version))
            {
              max_major_version = payload->id.major_version;
              max_minor_version = payload->id.minor_version;
            }

          /* The namespace id is 64b large, we need to use an indirection
           * so that it can fit in a hash table on 32b systems.
           */
          key = g_new (IdeGiNamespaceId, 1);
          *key = payload->id;

          state = namespace_state_new (self, payload->id);
          g_hash_table_insert (self->ns_table, key, state);
        }
    }

  if (has_namespace)
    ide_gi_require_add (req,
                        word,
                        (IdeGiRequireBound){.comp = IDE_GI_REQUIRE_COMP_EQUAL,
                                            .major_version = max_major_version,
                                            .minor_version = max_minor_version});
}

static void
compute_namespaces_info (IdeGiVersion *self)
{
  g_assert (IDE_IS_GI_VERSION (self));

  if (self->index_dt != NULL)
    ide_gi_flat_radix_tree_foreach (self->index_dt,
                                    compute_namespaces_info_filter_func,
                                    self);
}

gboolean
ide_gi_version_setup (IdeGiVersion  *self,
                      GFile         *cache_dir,
                      GCancellable  *cancellable,
                      GError       **error)
{
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GError) tmp_error = NULL;
  g_autoptr(GMappedFile) index_map = NULL;
  gchar *data;
  guint64 *dt_data;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (cache_dir), FALSE);

  file = g_file_get_child (cache_dir, self->index_name);
  path = g_file_get_path (file);

  if (NULL == (index_map = g_mapped_file_new (path, FALSE, &tmp_error)))
    {
      if (g_error_matches (tmp_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          g_set_error (error,
                       IDE_GI_VERSION_ERROR,
                       IDE_GI_VERSION_ERROR_INDEX_NOT_FOUND,
                       "Index file '%s' not found", path);
          return FALSE;
        }

      g_propagate_error (error, tmp_error);
      tmp_error = NULL;
      return FALSE;
    }

  data = g_mapped_file_get_contents (index_map);
  g_assert (IS_64B_MULTIPLE (data));

NO_CAST_ALIGN_PUSH
  self->index_header = (IndexHeader *)data;
NO_CAST_ALIGN_POP
  IDE_TRACE_MSG ("Index mapped:%s", ide_gi_version_get_string (self, self->index_header->id_offset64b));

  if (self->index_header->abi_version != INDEX_ABI_VERSION)
    {
      self->index_header =  NULL;

      g_set_error (error,
                   IDE_GI_VERSION_ERROR,
                   IDE_GI_VERSION_ERROR_WRONG_ABI,
                   "Index ABI version has changed (found '%d' wanted '%d'), update needed",
                   self->index_header->abi_version,
                   INDEX_ABI_VERSION);

      return FALSE;
    }

NO_CAST_ALIGN_PUSH
  self->index_namespaces = (guint64 *)data + self->index_header->namespaces_offset64b;
  dt_data = (guint64 *)data + self->index_header->dt_offset64b;
NO_CAST_ALIGN_POP

  ide_gi_flat_radix_tree_init (self->index_dt,
                               dt_data,
                               self->index_header->dt_size64b << 3);

  compute_namespaces_info (self);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    {
      self->index_header =  NULL;
      self->index_namespaces = NULL;
      dzl_clear_pointer (&self->index_dt, ide_gi_flat_radix_tree_clear);

      IDE_TRACE_MSG ("Version @%i cancelled", self->version_count);
      return FALSE;
    }

  IDE_TRACE_MSG ("Version @%i:index loaded from %s", self->version_count, path);
  self->index_map = g_steal_pointer (&index_map);
  return TRUE;
}

/**
 * ide_gi_version_get_index:
 * @self: #IdeGiVersion
 *
 * Get the version parent #IdeGiIndex.
 *
 * Returns: (transfer full): a #IdeGiIndex
 */
IdeGiIndex *
ide_gi_version_get_index (IdeGiVersion *self)
{
  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  return g_object_ref (self->index);
}

/**
 * ide_gi_version_new:
 * @index: the parent #IdeGiIndex
 * @cache_dir: the folder used to store cached files.
 * @count: the version count
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Create a new #IdeGiVersion with @index as parent #IdeGiIndex.
 *
 * Returns: a #IdeGiVersion or %NULL with @error set.
 */
IdeGiVersion *
ide_gi_version_new (IdeGiIndex    *index,
                    GFile         *cache_dir,
                    guint          count,
                    GCancellable  *cancellable,
                    GError       **error)
{
  IdeGiVersion *self;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_GI_INDEX (index), NULL);
  g_return_val_if_fail (G_IS_FILE (cache_dir), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  self = g_initable_new (IDE_TYPE_GI_VERSION,
                         cancellable,
                         error,
                         "cache-dir", cache_dir,
                         "count", count,
                         "index", index,
                         NULL);

  IDE_RETURN (self);
}

/**
 * ide_gi_version_new_async:
 * @index: the parent #IdeGiIndex
 * @cache_dir: the folder used to store cached files.
 * @count: the version count
 * @cancellable: a #GCancellable
 * @callback: a #GAsyncReadyCallback
 * @user_data:
 *
 * Create a new #IdeGiVersion with @index as parent #IdeGiIndex.
 *
 * Returns: none
 */
void
ide_gi_version_new_async (IdeGiIndex          *index,
                          GFile               *cache_dir,
                          guint                count,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GI_INDEX (index));
  g_return_if_fail (G_IS_FILE (cache_dir));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* TODO: protect against multiple requests of new versions on the same index:
   * the previous version must have returned to launch a new one.
   */
  g_async_initable_new_async (IDE_TYPE_GI_VERSION,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "cache-dir", cache_dir,
                              "count", count,
                              "index", index,
                              NULL);

  IDE_EXIT;
}

/**
 * ide_gi_version_new_finish:
 * @initable: the object received from the #GAsyncReadyCallback
 * @result: the #GAsyncResult received from the #GAsyncReadyCallback
 * @error: a #GError
 *
 * Returns: a #IdeGiVersion or %NULL with @error set.
 */
IdeGiVersion *
ide_gi_version_new_finish (GAsyncInitable  *initable,
                           GAsyncResult    *result,
                           GError         **error)
{
  GObject *obj;

  IDE_ENTRY;

  g_return_val_if_fail (G_IS_ASYNC_INITABLE (initable), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  if ((obj = g_async_initable_new_finish (initable, result, error)))
    IDE_RETURN (IDE_GI_VERSION (obj));
  else
    IDE_RETURN (NULL);
};

static gboolean
ide_gi_version_initable_init (GInitable     *initable,
                              GCancellable  *cancellable,
                              GError       **error)
{
  IdeGiVersion *self = (IdeGiVersion *)initable;

  g_return_val_if_fail (IDE_IS_GI_VERSION (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return ide_gi_version_setup (self, self->cache_dir, cancellable, error);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_gi_version_initable_init;
}

static void
async_initable_init (GAsyncInitableIface *iface)
{
  /* We use the default implementations chaining on GInitable ones in a thread */
}

/* If some smart people have a less twisted system than this one,
 * don't hesitate to do a proposal...
 */
static void
ide_gi_version_dispose (GObject *object)
{
  IdeGiVersion *self = (IdeGiVersion *)object;

  g_assert (self->has_keep_alive_ref == FALSE);

  /* We use the dispose has a way to avoid a toggle ref, already used by bindings
   * or weakref, returning an already freed object.
   */
  if (self->is_removing)
    {
      /* This time this is the good one, we can free it,
       * no lock needed, we are the only user.
       */
      g_assert (self->ns_used_count == 0);

      g_clear_object (&self->index);
      G_OBJECT_CLASS (ide_gi_version_parent_class)->dispose (object);
      return;
    }

  g_object_ref (self);

  /* Protect ns_used_count and has_keep_alive_ref */
  g_mutex_lock (&self->ns_used_count_mutex);
  IDE_TRACE_MSG ("version %d ns_used_count:%d", self->version_count, self->ns_used_count);
  if (self->ns_used_count == 0)
    {
      self->is_removing = TRUE;
      g_mutex_unlock (&self->ns_used_count_mutex);
      _ide_gi_index_version_remove (self->index, self);
    }
  else
    {
      self->has_keep_alive_ref = TRUE;
      g_mutex_unlock (&self->ns_used_count_mutex);
    }
}

static void
ide_gi_version_finalize (GObject *object)
{
  IdeGiVersion *self = (IdeGiVersion *)object;

  G_OBJECT_CLASS (ide_gi_version_parent_class)->finalize (object);

  dzl_clear_pointer (&self->index_map, g_mapped_file_unref);
  dzl_clear_pointer (&self->index_dt, ide_gi_flat_radix_tree_unref);
  dzl_clear_pointer (&self->ns_table, g_hash_table_unref);
  dzl_clear_pointer (&self->index_name, g_free);
  dzl_clear_pointer (&self->file_suffix, g_free);
  dzl_clear_pointer (&self->req_highest_versions, ide_gi_require_unref);

  g_mutex_clear (&self->ns_used_count_mutex);
}

static void
ide_gi_version_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeGiVersion *self = IDE_GI_VERSION (object);

  switch (prop_id)
    {
    case PROP_CACHE_DIR:
      g_value_set_object (value, self->cache_dir);
      break;

    case PROP_COUNT:
      g_value_set_uint (value, self->version_count);
      break;

    case PROP_INDEX:
      g_value_set_object (value, self->index);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gi_version_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeGiVersion *self = IDE_GI_VERSION (object);

  switch (prop_id)
    {
    case PROP_CACHE_DIR:
      self->cache_dir = g_value_dup_object (value);
      break;

    case PROP_COUNT:
      self->version_count = g_value_get_uint (value);
      break;

    case PROP_INDEX:
      self->index = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gi_version_constructed (GObject *object)
{
  IdeGiVersion *self = IDE_GI_VERSION (object);

  G_OBJECT_CLASS (ide_gi_version_parent_class)->constructed (object);

  self->index_name = g_strdup_printf ("%s@%d%s", INDEX_FILE_NAME, self->version_count, INDEX_FILE_EXTENSION);
  self->file_suffix = g_strdup_printf ("@%d%s", self->version_count, INDEX_NAMESPACE_EXTENSION);
}

static void
ide_gi_version_class_init (IdeGiVersionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_gi_version_dispose;
  object_class->finalize = ide_gi_version_finalize;
  object_class->constructed = ide_gi_version_constructed;
  object_class->get_property = ide_gi_version_get_property;
  object_class->set_property = ide_gi_version_set_property;

  properties [PROP_CACHE_DIR] =
    g_param_spec_object ("cache-dir",
                         "Files cache directory",
                         "The directory where index and objects files are cached.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_COUNT] =
    g_param_spec_uint ("count",
                         "Count",
                         "The version count.",
                         0, G_MAXUINT16 >> 1, 0,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INDEX] =
    g_param_spec_object ("index",
                         "Index",
                         "The parent index.",
                         IDE_TYPE_GI_INDEX,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_gi_version_init (IdeGiVersion *self)
{
  self->index_dt = ide_gi_flat_radix_tree_new ();
  self->ns_table = g_hash_table_new_full (g_int64_hash,
                                          g_int64_equal,
                                          g_free,
                                          (GDestroyNotify)namespace_state_free);

  self->req_highest_versions = ide_gi_require_new ();
  g_mutex_init (&self->ns_used_count_mutex);
}

/**
 * ide_gi_version_get_count:
 * @self: a #IdeGiVersion
 *
 * Get the count of the version.
 *
 * Returns: The count
 */
guint
ide_gi_version_get_count (IdeGiVersion *self)
{
  g_return_val_if_fail (IDE_IS_GI_VERSION (self), 0);

  return self->version_count;
}

/**
 * ide_gi_version_get_versionned_filename:
 * @self: a s#IdeGiVersion
 * @name: name part of the file
 *
 * Get a basename for this version with the syntax:
 * {name}@{version_count}.ns
 *
 * Returns: (transfer full)
 */
gchar *
ide_gi_version_get_versionned_filename (IdeGiVersion *self,
                                        const gchar  *name)
{
  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  return g_strconcat (name, self->file_suffix, NULL);
}

/**
 * ide_gi_version_get_versionned_index_name:
 * @self: #IdeGiVersion
 *
 * Get the index basename for this version with the syntax:
 * index@{version_count}.tree
 *
 * Returns: (transfer full)
 */
gchar *
ide_gi_version_get_versionned_index_name (IdeGiVersion *self)
{
  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  return g_strdup (self->index_name);
}

/**
 * ide_gi_version_get_highest_versions:
 * @self: a #IdeGiRequire
 *
 * Get a require containing the greatest namespace versions for this #IdeGiVersion.
 *
 * Returns: (transfer full): A #IdeGiRequire
 */
IdeGiRequire *
ide_gi_version_get_highest_versions (IdeGiVersion *self)
{
  g_return_val_if_fail (IDE_IS_GI_VERSION (self), NULL);

  return ide_gi_require_ref (self->req_highest_versions);
}

GQuark
ide_gi_version_error_quark (void)
{
  static GQuark quark = 0;

  if G_UNLIKELY (quark == 0)
    quark =  g_quark_from_static_string ("ide-gi-version-error-quark");

  return quark;
}
