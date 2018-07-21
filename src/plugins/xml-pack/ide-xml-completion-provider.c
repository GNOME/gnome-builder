/* ide-xml-completion-provider.c
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
 */

#define G_LOG_DOMAIN "xml-completion-provider"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <ide.h>
#include <libpeas/peas.h>

#include "../gi/ide-gi-objects.h"
#include "../gi/ide-gi-repository.h"
#include "../gi/ide-gi-require.h"
#include "../gi/ide-gi-service.h"

#include "ide-xml-completion-attributes.h"
#include "ide-xml-completion-provider.h"
#include "ide-xml-completion-values.h"
#include "ide-xml-detail.h"
#include "ide-xml-path.h"
#include "ide-xml-position.h"
#include "ide-xml-proposal.h"
#include "ide-xml-rng-define.h"
#include "ide-xml-schema-cache-entry.h"
#include "ide-xml-service.h"
#include "ide-xml-symbol-node.h"
#include "ide-xml-types.h"
#include "ide-xml-utils.h"

struct _IdeXmlCompletionProvider
{
  IdeObject parent_instance;
};

typedef struct
{
  GArray           *stack;
  IdeXmlSymbolNode *parent_node;
  IdeXmlSymbolNode *candidate_node;
  IdeXmlPosition   *position;
  IdeXmlRngDefine  *define;
  GPtrArray        *children;
  GPtrArray        *items;
  gchar            *prefix;
  gint              child_cursor;
  gint              define_cursor;

  guint             is_initial_state : 1;
  guint             is_in_root_node  : 1;
  guint             retry : 1;
} MatchingState;

typedef struct
{
  GPtrArray        *children;
  IdeXmlSymbolNode *candidate_node;
} StateStackItem;

typedef struct
{
  IdeGiService *gi_service;
  IdeFile      *ifile;
  gint          line;
  gint          line_offset;
} PopulateState;

typedef struct
{
  gchar           *label;
  IdeXmlRngDefine *define;
} CompletionItem;

static void      completion_provider_init (IdeCompletionProviderInterface *iface);
static gboolean  process_matching_state   (MatchingState                  *state,
                                           IdeXmlRngDefine                *define);

G_DEFINE_TYPE_WITH_CODE (IdeXmlCompletionProvider,
                         ide_xml_completion_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, completion_provider_init))

static void
populate_state_free (PopulateState *state)
{
  g_assert (state != NULL);

  g_clear_object (&state->ifile);
  g_clear_object (&state->gi_service);

  g_slice_free (PopulateState, state);
}

static void
add_to_store (GListStore  *store,
              const GList *list)
{
  for (const GList *iter = list; iter; iter = iter->next)
    g_list_store_append (store, iter->data);
}

static GPtrArray *
copy_children (GPtrArray *children)
{
  GPtrArray *copy;

  g_assert (children != NULL);

  copy = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < children->len; ++i)
    {
      IdeXmlSymbolNode *node = g_ptr_array_index (children, i);
      g_ptr_array_add (copy, g_object_ref (node));
    }

  return copy;
}

static void
state_stack_item_free (gpointer *data)
{
  StateStackItem *item;

  g_assert (data != NULL);

  item = (StateStackItem *)data;
  g_clear_pointer (&item->children, g_ptr_array_unref);
  g_clear_object (&item->candidate_node);
}

static GArray *
state_stack_new (void)
{
  GArray *stack;

  stack = g_array_new (FALSE, TRUE, sizeof (StateStackItem));
  g_array_set_clear_func (stack, (GDestroyNotify)state_stack_item_free);

  return stack;
}

static void
state_stack_push (MatchingState *state)
{
  StateStackItem item;

  g_assert (state->stack != NULL);
  g_assert (IDE_IS_XML_SYMBOL_NODE (state->candidate_node));

  item.children = copy_children (state->children);
  item.candidate_node = g_object_ref (state->candidate_node);

  g_array_append_val (state->stack, item);
}

static gboolean
state_stack_pop (MatchingState *state)
{
  StateStackItem *item;
  guint len;

  g_assert (state->stack != NULL);

  if (0 == (len = state->stack->len))
    return FALSE;

  item = &g_array_index (state->stack, StateStackItem, len - 1);
  g_clear_pointer (&state->children, g_ptr_array_unref);
  state->children = g_steal_pointer (&item->children);
  g_set_object (&state->candidate_node, g_steal_pointer (&item->candidate_node));

  g_array_remove_index (state->stack, len - 1);
  return TRUE;
}

static gboolean
state_stack_drop (MatchingState *state)
{
  guint len;

  g_assert (state->stack != NULL);

  if (0 == (len = state->stack->len))
    return FALSE;

  g_array_remove_index (state->stack, len - 1);
  return TRUE;
}

static gboolean
state_stack_peek (MatchingState *state)
{
  StateStackItem *item;
  guint len;

  g_assert (state->stack != NULL);

  if (0 == (len = state->stack->len))
    return FALSE;

  item = &g_array_index (state->stack, StateStackItem, len - 1);

  g_clear_pointer (&state->children, g_ptr_array_unref);
  state->children = copy_children (item->children);
  g_set_object (&state->candidate_node, item->candidate_node);

  return TRUE;
}

static IdeXmlPath *
get_path (IdeXmlSymbolNode *node,
          IdeXmlSymbolNode *root_node)
{
  IdeXmlPath *path;
  IdeXmlSymbolNode *current = node;

  g_assert (IDE_IS_XML_SYMBOL_NODE (node));
  g_assert (IDE_IS_XML_SYMBOL_NODE (root_node));

  path = ide_xml_path_new ();
  while (current && current != root_node)
    {
      ide_xml_path_prepend_node (path, current);
      current = ide_xml_symbol_node_get_parent (current);
    }

  if (current != root_node)
    g_warning ("partial path, we don't reach the root node");

  if (path->nodes->len == 0)
    ide_xml_path_prepend_node (path, root_node);

  return path;
}

static void
move_candidates (GPtrArray *array,
                 GPtrArray *sub_array)
{
  g_assert (array != NULL);
  g_assert (sub_array != NULL);

  if (sub_array->len == 0)
    return;

  for (guint i = 0; i < sub_array->len; ++i)
    g_ptr_array_add (array, g_ptr_array_index (sub_array, i));

  g_ptr_array_remove_range (sub_array, 0, sub_array->len);
}

static void
get_matching_nodes (IdeXmlPath      *path,
                    guint            index,
                    IdeXmlRngDefine *define,
                    GPtrArray       *candidates)
{
  IdeXmlSymbolNode *node;

  g_assert (path != NULL);
  g_assert (define != NULL);
  g_assert (candidates != NULL);

  node = g_ptr_array_index (path->nodes, index);

  while (define != NULL)
    {
      IdeXmlRngDefine *child = NULL;
      gint current_index = index;
      IdeXmlRngDefineType type = define->type;

      switch (type)
        {
        case IDE_XML_RNG_DEFINE_ELEMENT:
          if (ide_xml_rng_define_is_nameclass_match (define, node))
            {
              ++current_index;
              child = define->content;
            }

          break;

        case IDE_XML_RNG_DEFINE_NOOP:
        case IDE_XML_RNG_DEFINE_NOTALLOWED:
        case IDE_XML_RNG_DEFINE_TEXT:
        case IDE_XML_RNG_DEFINE_DATATYPE:
        case IDE_XML_RNG_DEFINE_VALUE:
        case IDE_XML_RNG_DEFINE_EMPTY:
        case IDE_XML_RNG_DEFINE_ATTRIBUTE:
        case IDE_XML_RNG_DEFINE_START:
        case IDE_XML_RNG_DEFINE_PARAM:
        case IDE_XML_RNG_DEFINE_EXCEPT:
        case IDE_XML_RNG_DEFINE_LIST:
        case IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP:
          break;

        case IDE_XML_RNG_DEFINE_DEFINE:
        case IDE_XML_RNG_DEFINE_REF:
        case IDE_XML_RNG_DEFINE_PARENTREF:
        case IDE_XML_RNG_DEFINE_EXTERNALREF:
          child = define->content;
          break;

        case IDE_XML_RNG_DEFINE_ZEROORMORE:
        case IDE_XML_RNG_DEFINE_ONEORMORE:
        case IDE_XML_RNG_DEFINE_OPTIONAL:
        case IDE_XML_RNG_DEFINE_CHOICE:
        case IDE_XML_RNG_DEFINE_GROUP:
        case IDE_XML_RNG_DEFINE_INTERLEAVE:
          child = define->content;
          break;

        default:
          g_assert_not_reached ();
        }

      if (current_index == path->nodes->len)
        g_ptr_array_add (candidates, define);
      else if (child != NULL)
        get_matching_nodes (path, current_index, child, candidates);

      define = define->next;
    }
}

static GPtrArray *
get_matching_candidates (IdeXmlCompletionProvider *self,
                         GPtrArray                *schemas,
                         IdeXmlPath               *path)
{
  GPtrArray *candidates;
  g_autoptr(GPtrArray) candidates_tmp = NULL;
  g_autoptr(GPtrArray) defines = NULL;
  IdeXmlSchemaCacheEntry *schema_entry;
  IdeXmlSchema *schema;
  IdeXmlRngGrammar *grammar;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (schemas != NULL);
  g_assert (path != NULL && path->nodes->len > 0);

  candidates = g_ptr_array_new ();
  candidates_tmp = g_ptr_array_sized_new (16);
  defines = g_ptr_array_new ();

  for (guint i = 0; i < schemas->len; ++i)
    {
      schema_entry = g_ptr_array_index (schemas, i);
      /* TODO: only RNG for now */
      if (schema_entry->kind != SCHEMA_KIND_RNG ||
          schema_entry->state != SCHEMA_STATE_PARSED)
        continue;

      schema = schema_entry->schema;
      grammar = schema->top_grammar;

      if (path->nodes->len > 1)
        {
          get_matching_nodes (path, 0, grammar->start_defines, candidates_tmp);
          move_candidates (candidates, candidates_tmp);
        }
      else
        /* We add the start element that for the completion at root level */
        g_ptr_array_add (candidates, grammar->start_defines);
    }

  return candidates;
}

static CompletionItem *
completion_item_new (const gchar     *label,
                     IdeXmlRngDefine *define)
{
  CompletionItem *item;

  g_assert (!dzl_str_empty0 (label));
  g_assert (define != NULL);

  item = g_slice_new0 (CompletionItem);

  item->label = g_strdup (label);
  item->define = ide_xml_rng_define_ref (define);

  return item;
}

static void
completion_item_free (CompletionItem *item)
{
  g_clear_pointer (&item->label, g_free);
  g_clear_pointer (&item->define, ide_xml_rng_define_unref);
  g_slice_free (CompletionItem, item);
}

static MatchingState *
matching_state_new (IdeXmlPosition  *position,
                    IdeXmlRngDefine *define,
                    GPtrArray       *items)
{
  MatchingState *state;
  IdeXmlDetail *detail;

  g_assert (IDE_IS_XML_SYMBOL_NODE (position->node));
  g_assert (define != NULL);

  state = g_slice_new0 (MatchingState);

  state->parent_node = NULL;
  state->position = position;
  state->define = define;
  state->items = items;

  g_set_object (&state->candidate_node, ide_xml_position_get_child_node (position));

  state->children = g_ptr_array_new_with_free_func (g_object_unref);
  state->stack = state_stack_new ();

  detail = ide_xml_position_get_detail (position);
  state->prefix = g_strdup (detail->prefix);

  return state;
}

G_GNUC_UNUSED static MatchingState *
matching_state_copy (MatchingState *state)
{
  MatchingState *new_state;

  new_state = g_slice_new0 (MatchingState);

  g_set_object (&new_state->parent_node, state->parent_node);
  g_set_object (&new_state->candidate_node, state->candidate_node);

  new_state->position = state->position;
  new_state->define = state->define;
  new_state->define_cursor = state->define_cursor;
  new_state->child_cursor = state->child_cursor;
  new_state->prefix = g_strdup (state->prefix);
  new_state->items = state->items;

  new_state->children = copy_children (state->children);

  return new_state;
}

static void
matching_state_free (MatchingState *state)
{
  g_clear_object (&state->parent_node);
  g_clear_object (&state->candidate_node);

  g_clear_pointer (&state->prefix, g_free);
  g_clear_pointer (&state->children, g_ptr_array_unref);
  g_clear_pointer (&state->stack, g_array_unref);

  g_slice_free (MatchingState, state);
}

static MatchingState *
create_initial_matching_state (IdeXmlPosition  *position,
                               IdeXmlRngDefine *define,
                               GPtrArray       *items)
{
  MatchingState *state;
  IdeXmlSymbolNode *node, *pos_node;
  guint nb_nodes;
  gint child_pos;

  g_assert (position != NULL);
  g_assert (define != NULL);
  g_assert (items != NULL);

  state = matching_state_new (position, define, items);
  child_pos = ide_xml_position_get_child_pos (position);

  pos_node = ide_xml_position_get_node (position);
  g_assert (IDE_IS_XML_SYMBOL_NODE (pos_node));

  nb_nodes = ide_xml_symbol_node_get_n_direct_children (pos_node);
  for (guint i = 0; i <= nb_nodes; ++i)
    {
      /* Inject a fake node at child_pos */
      if (child_pos == i && state->candidate_node != NULL)
        g_ptr_array_add (state->children, g_object_ref (state->candidate_node));

      /* We loop one more times than the number of children just to be able to inject in last position */
      if (i == nb_nodes)
        break;

      node = IDE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_nth_direct_child (pos_node, i));
      g_ptr_array_add (state->children, node);
    }

  state->is_initial_state = TRUE;

  if (ide_xml_symbol_node_is_root (pos_node))
    state->is_in_root_node = TRUE;

  return state;
}

static gboolean
is_define_equal_node (IdeXmlRngDefine  *define,
                      IdeXmlSymbolNode *node)
{
  g_assert (define != NULL);
  g_assert (IDE_IS_XML_SYMBOL_NODE (node));

  return dzl_str_equal0 (ide_xml_symbol_node_get_element_name (node), define->name);
}

static gboolean
is_element_matching (MatchingState *state)
{
  IdeXmlSymbolNode *node;
  CompletionItem *item;
  const gchar *name;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_ELEMENT);

  /* XXX: we skip element without a name for now */
  if (dzl_str_empty0 ((gchar *)state->define->name))
    return FALSE;

  if (state->children->len == 0)
    return FALSE;

  state->retry = FALSE;
  node = g_ptr_array_index (state->children, 0);

  if (state->candidate_node == node)
    {
      name = (gchar *)state->define->name;
      if (dzl_str_empty0 (state->prefix) || g_str_has_prefix ((gchar *)state->define->name, state->prefix))
        {
          g_clear_object (&state->candidate_node);
          state->retry = TRUE;

          item = completion_item_new (name, state->define);
          g_ptr_array_add (state->items, item);

          return TRUE;
        }
      else
        return FALSE;
    }
  else if (is_define_equal_node (state->define, node))
    {
      g_ptr_array_remove_index (state->children, 0);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
is_choice_matching (MatchingState *state)
{
  IdeXmlRngDefine *defines;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_CHOICE);

  if (NULL == (defines = state->define->content))
    return TRUE;

  state_stack_push (state);

  while (defines != NULL)
    {
      if (process_matching_state (state, defines))
        {
          if (state->retry)
            state->retry = FALSE;
          else
            {
              state_stack_drop (state);
              return TRUE;
            }
        }

      if ((defines = defines->next))
        state_stack_peek (state);
      else
        state_stack_pop (state);
    }

  state->retry = FALSE;
  return FALSE;
}

static gboolean
is_n_matching (MatchingState *state)
{
  IdeXmlRngDefine *defines;
  IdeXmlRngDefineType type = state->define->type;
  gboolean is_child_matching;
  gboolean is_matching = FALSE;

  g_assert (type == IDE_XML_RNG_DEFINE_ZEROORMORE ||
            type == IDE_XML_RNG_DEFINE_ONEORMORE ||
            type == IDE_XML_RNG_DEFINE_OPTIONAL);

  state_stack_push (state);

loop:
  /* Only ZeroOrMore or optionnal match if there's no children */
  if (NULL == (defines = state->define->content))
    return !(type == IDE_XML_RNG_DEFINE_ONEORMORE);

  is_child_matching = TRUE;
  while (defines != NULL)
    {
      if (!process_matching_state (state, defines))
        {
          is_child_matching = FALSE;
          break;
        }

      defines = defines->next;
    }

  if (is_child_matching)
    {
      is_matching = TRUE;
      state_stack_drop (state);
      if ((type == IDE_XML_RNG_DEFINE_ONEORMORE || type == IDE_XML_RNG_DEFINE_ZEROORMORE) &&
          state->candidate_node != NULL)
        {
          state_stack_push (state);
          goto loop;
        }
    }
  else
    {
      state_stack_pop (state);
    }

  state->retry = FALSE;
  if (type == IDE_XML_RNG_DEFINE_OPTIONAL || type == IDE_XML_RNG_DEFINE_ZEROORMORE)
    return TRUE;

  return is_matching;
}

static gboolean
is_group_matching (MatchingState *state)
{
  IdeXmlRngDefine *defines;
  gboolean is_matching = TRUE;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_GROUP ||
            state->define->type == IDE_XML_RNG_DEFINE_ELEMENT);

  if (state->is_in_root_node)
    defines = state->define;
  else
    defines = state->define->content;

  if (defines == NULL)
    return TRUE;

  state->is_in_root_node = FALSE;
  state->is_initial_state = FALSE;

  state_stack_push (state);
  while (defines != NULL)
    {
      if (!process_matching_state (state, defines))
        {
          is_matching = FALSE;
          break;
        }

      if ((defines = defines->next))
        {
          state_stack_drop (state);
          state_stack_push (state);
        }
    }

  state->retry = FALSE;
  if (is_matching)
    {
      state_stack_drop (state);
      return TRUE;
    }

  return is_matching;
}

static gboolean
process_matching_state (MatchingState   *state,
                        IdeXmlRngDefine *define)
{
  IdeXmlRngDefine *old_define;
  IdeXmlRngDefineType type;
  gboolean is_matching = FALSE;

  g_assert (state != NULL);
  g_assert (define != NULL);

  if (state->candidate_node == NULL)
    return TRUE;

  old_define = state->define;
  state->define = define;

  if (state->is_initial_state)
    {
      type = IDE_XML_RNG_DEFINE_GROUP;
    }
  else
    type = define->type;

  switch (type)
    {
    case IDE_XML_RNG_DEFINE_ELEMENT:
      is_matching = is_element_matching (state);
      break;

    case IDE_XML_RNG_DEFINE_NOOP:
    case IDE_XML_RNG_DEFINE_NOTALLOWED:
    case IDE_XML_RNG_DEFINE_TEXT:
    case IDE_XML_RNG_DEFINE_DATATYPE:
    case IDE_XML_RNG_DEFINE_VALUE:
    case IDE_XML_RNG_DEFINE_EMPTY:
    case IDE_XML_RNG_DEFINE_ATTRIBUTE:
    case IDE_XML_RNG_DEFINE_PARAM:
    case IDE_XML_RNG_DEFINE_EXCEPT:
    case IDE_XML_RNG_DEFINE_LIST:
    case IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP:
      is_matching = FALSE;
      break;

    case IDE_XML_RNG_DEFINE_START:
    case IDE_XML_RNG_DEFINE_DEFINE:
    case IDE_XML_RNG_DEFINE_REF:
    case IDE_XML_RNG_DEFINE_PARENTREF:
    case IDE_XML_RNG_DEFINE_EXTERNALREF:
      is_matching = process_matching_state (state, define->content);
      break;

    case IDE_XML_RNG_DEFINE_ZEROORMORE:
    case IDE_XML_RNG_DEFINE_ONEORMORE:
    case IDE_XML_RNG_DEFINE_OPTIONAL:
      is_matching = is_n_matching (state);
      break;

    case IDE_XML_RNG_DEFINE_CHOICE:
      is_matching = is_choice_matching (state);
      break;

    case IDE_XML_RNG_DEFINE_INTERLEAVE:
      is_matching = FALSE;
      break;

    case IDE_XML_RNG_DEFINE_GROUP:
      is_matching = is_group_matching (state);
      break;

    default:
      g_assert_not_reached ();
    }

  state->define = old_define;

  return is_matching;
}

static GList *
get_element_proposals (IdeXmlPosition *position,
                       GPtrArray      *items,
                       const gchar    *prefix)
{
  IdeXmlPositionKind kind;
  IdeXmlDetail *detail;
  GList *results = NULL;

  g_assert (position != NULL);
  g_assert (items != NULL);

  kind = ide_xml_position_get_kind (position);
  detail = ide_xml_position_get_detail (position);

  for (guint i = 0; i < items->len; ++i)
    {
      g_autofree gchar *label = NULL;
      g_autoptr(GString) string = NULL;
      CompletionItem *completion_item;
      IdeXmlProposal *item;
      gint pos;

      /* TODO: the cursor pos need to depend on the attributes */

      completion_item = g_ptr_array_index (items, i);
      label = g_strconcat ("&lt;", completion_item->label, "&gt;", NULL);
      string = g_string_sized_new (8);

      if (kind == IDE_XML_POSITION_KIND_IN_CONTENT)
        {
          g_string_printf (string, "<%s></%s>", completion_item->label, completion_item->label);
          pos = g_utf8_strlen (completion_item->label, -1) + 1;
        }
      else if (kind == IDE_XML_POSITION_KIND_IN_START_TAG &&
               detail->member == IDE_XML_DETAIL_MEMBER_NAME)
        {
          IdeXmlSymbolNode *child_node = ide_xml_position_get_child_node (position);
          IdeXmlSymbolNodeState state;

          g_assert (child_node != NULL);

          state = ide_xml_symbol_node_get_state (child_node);

          g_string_append (string, completion_item->label);
          if (!ide_xml_symbol_node_has_end_tag (child_node))
            g_string_append_printf (string, "></%s", completion_item->label);

          if (state == IDE_XML_SYMBOL_NODE_STATE_NOT_CLOSED)
            g_string_append (string, ">");

          pos = g_utf8_strlen (completion_item->label, -1);
        }
      else
        continue;

      item = ide_xml_proposal_new (string->str,
                                   NULL,
                                   label,
                                   prefix,
                                   NULL,
                                   pos,
                                   kind,
                                   IDE_XML_COMPLETION_TYPE_ELEMENT);
      results = g_list_prepend (results, item);
    }

  return results;
}

static GList *
get_attributes_proposals (IdeXmlPosition  *position,
                          IdeXmlRngDefine *define)
{
  IdeXmlSymbolNode *node;
  IdeXmlDetail *detail;
  IdeXmlProposal *item;
  g_autoptr(GPtrArray) attributes = NULL;
  GList *results = NULL;

  node = ide_xml_position_get_child_node (position);
  detail = ide_xml_position_get_detail (position);

  if ((attributes = ide_xml_completion_attributes_get_matches (define, node, TRUE)))
    {
      IdeXmlPositionKind kind = ide_xml_position_get_kind (position);

      for (guint i = 0; i < attributes->len; ++i)
        {
          g_autofree gchar *name = NULL;
          g_autofree gchar *text = NULL;
          MatchItem *match_item;
          gint pos;

          match_item = g_ptr_array_index (attributes, i);
          name = g_strdup (match_item->name);
          text = g_strconcat (match_item->name, "=\"\"", NULL);
          pos = g_utf8_strlen (match_item->name, -1) + 2;
          item = ide_xml_proposal_new (text,
                                       match_item->is_optional ? "optional" : NULL,
                                       name,
                                       detail->prefix,
                                       NULL,
                                       pos,
                                       kind,
                                       IDE_XML_COMPLETION_TYPE_ATTRIBUTE);

          results = g_list_prepend (results, item);
        }
    }

  return results;
}

static GList *
get_values_proposals (IdeXmlPosition  *position,
                      IdeXmlRngDefine *define)
{
  IdeXmlSymbolNode *node;
  IdeXmlDetail *detail;
  IdeXmlRngDefine *attr_define = NULL;
  g_autoptr(GPtrArray) attributes = NULL;
  g_autoptr(GPtrArray) values = NULL;
  GList *results = NULL;

  node = ide_xml_position_get_child_node (position);
  detail = ide_xml_position_get_detail (position);
  g_assert (!dzl_str_empty0 (detail->name));

  if ((attributes = ide_xml_completion_attributes_get_matches (define, node, FALSE)))
    {
      MatchItem *match_item;
      const gchar *content;

      for (gint j = 0; j < attributes->len; ++j)
        {
          match_item = g_ptr_array_index (attributes, j);
          if (dzl_str_equal0 (detail->name, match_item->name))
            {
              attr_define = match_item->define;
              break;
            }
        }

      if (attr_define != NULL)
        {
          content = ide_xml_symbol_node_get_attribute_value (node, match_item->name);

          if ((values = ide_xml_completion_values_get_matches (attr_define, content, detail->value)))
            {
              IdeXmlPositionKind kind = ide_xml_position_get_kind (position);

              for (guint i = 0; i < values->len; ++i)
                {
                  ValueMatchItem *value_match_item = g_ptr_array_index (values, i);

                  results = g_list_prepend (results,
                                            ide_xml_proposal_new (value_match_item->name,
                                                                  NULL,
                                                                  value_match_item->name,
                                                                  detail->prefix,
                                                                  NULL,
                                                                  -1,
                                                                  kind,
                                                                  IDE_XML_COMPLETION_TYPE_VALUE));
                }
            }
        }
    }

  return results;
}

static GList *
try_get_gtype_proposals (IdeXmlCompletionProvider *self,
                         IdeGiVersion             *version,
                         IdeXmlSymbolNode         *node,
                         IdeXmlPosition           *position)
{
  g_autoptr(GArray) ar = NULL;
  g_autoptr(IdeGiRequire) version_req = NULL;
  g_autoptr(IdeGiRequire) req = NULL;
  IdeXmlAnalysis *analysis;
  IdeXmlDetail *detail;
  GList *results = NULL;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_GI_VERSION (version));
  g_assert (IDE_IS_XML_SYMBOL_NODE (node));
  g_assert (position != NULL);

  analysis = ide_xml_position_get_analysis (position);
  req = ide_gi_require_copy (ide_xml_analysis_get_require (analysis));
  version_req = ide_gi_version_get_highest_versions (version);
  ide_gi_require_merge (req, version_req, IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_SOURCE);

  detail = ide_xml_position_get_detail (position);

  ar = ide_gi_version_complete_gtype (version,
                                      req,
                                      IDE_GI_COMPLETE_ROOT_CLASS,
                                      FALSE,
                                      detail->value);

  if (ar != NULL && ar->len > 0)
    {
      IdeXmlPositionKind kind = ide_xml_position_get_kind (position);

      for (guint i = 0; i < ar->len; i++)
        {
          IdeGiCompleteGtypeItem *src_item = &g_array_index (ar, IdeGiCompleteGtypeItem, i);
          IdeXmlProposal *dst_item;

          if (src_item->is_buildable)
            {
              g_autofree gchar *namespace = NULL;
              IdeGiBase *object;

              namespace = g_strdup_printf ("%s %d.%d",
                                           ide_gi_namespace_get_name (src_item->ns),
                                           src_item->major_version,
                                           src_item->minor_version);
              object = ide_gi_namespace_get_object (src_item->ns,
                                                    src_item->object_type,
                                                    src_item->object_offset);
              dst_item = ide_xml_proposal_new (src_item->word,
                                               namespace,
                                               src_item->word,
                                               detail->value,
                                               object,
                                               -1,
                                               kind,
                                               IDE_XML_COMPLETION_TYPE_UI_GTYPE);
              results = g_list_prepend (results, dst_item);
            }
        }

      results = g_list_reverse (results);
    }

  return results;
}

static gint
alphabetical_sort_func (gconstpointer a,
                        gconstpointer b)
{
  IdeXmlProposal *item_a = (IdeXmlProposal *)a;
  IdeXmlProposal *item_b = (IdeXmlProposal *)b;

  g_assert (IDE_IS_XML_PROPOSAL (item_a));
  g_assert (IDE_IS_XML_PROPOSAL (item_b));

  return g_ascii_strcasecmp (ide_xml_proposal_get_label (item_a),
                             ide_xml_proposal_get_label (item_b));
}

static void
append_object_properties (IdeGiBase       *object,
                          const gchar     *word,
                          IdeXmlPosition  *position,
                          GList          **results)
{
  IdeXmlPositionKind kind;
  guint n_props;

  g_assert (object != NULL);
  g_assert (results != NULL);

  n_props = ide_gi_class_get_n_properties ((IdeGiClass *)object);
  kind = ide_xml_position_get_kind (position);
  for (guint i = 0; i < n_props; i++)
    {
      g_autoptr(IdeGiProperty) prop = ide_gi_class_get_property ((IdeGiClass *)object, i);
      const gchar *name = ide_gi_base_get_name ((IdeGiBase *)prop);

      if (dzl_str_empty0 (word) || g_str_has_prefix (name, word))
        *results = g_list_prepend (*results, ide_xml_proposal_new (name,
                                                                   NULL,
                                                                   name,
                                                                   word,
                                                                   g_steal_pointer (&prop),
                                                                   -1,
                                                                   kind,
                                                                   IDE_XML_COMPLETION_TYPE_UI_PROPERTY));
    }
}

static void
fetch_object_properties (IdeGiBase       *object,
                         const gchar     *word,
                         IdeXmlPosition  *position,
                         GList          **results,
                         GHashTable      *visited)
{
  g_autofree gchar *qname = NULL;
  IdeGiBlobType type;
  guint16 n_interfaces;

  g_assert (object != NULL);
  g_assert (results != NULL);

  qname = ide_gi_base_get_qualified_name (object);
  if (g_hash_table_contains (visited, qname))
    return;

  append_object_properties (object, word, position, results);
  g_hash_table_add (visited, g_steal_pointer (&qname));

  type = ide_gi_base_get_object_type (object);
  if (type == IDE_GI_BLOB_TYPE_CLASS)
    {
      g_autoptr(IdeGiClass) parent_class = ide_gi_class_get_parent ((IdeGiClass *)object);

      if ((parent_class ))
        fetch_object_properties ((IdeGiBase *)parent_class, word, position, results, visited);

      n_interfaces = ide_gi_class_get_n_interfaces ((IdeGiClass *)object);
      for (guint i = 0; i < n_interfaces; i++)
        {
          g_autoptr(IdeGiInterface) interface = ide_gi_class_get_interface ((IdeGiClass *)object, i);

          if (interface != NULL)
            fetch_object_properties ((IdeGiBase *)interface, word, position, results, visited);
        }
    }
  else if (type == IDE_GI_BLOB_TYPE_INTERFACE)
    {
      n_interfaces = ide_gi_interface_get_n_prerequisites ((IdeGiInterface *)object);
      for (guint i = 0; i < n_interfaces; i++)
        {
          g_autoptr(IdeGiBase) base = ide_gi_interface_get_prerequisite ((IdeGiInterface *)object, i);

          if (base != NULL)
            fetch_object_properties (base, word, position, results, visited);
        }
    }
}

static GList *
try_get_property_name_proposals (IdeXmlCompletionProvider *self,
                                 IdeGiVersion             *version,
                                 IdeXmlSymbolNode         *parent_node,
                                 IdeXmlSymbolNode         *node,
                                 IdeXmlPosition           *position)
{
  const gchar *gtype_name;
  g_autoptr(IdeGiRequire) version_req = NULL;
  g_autoptr(IdeGiRequire) req = NULL;
  g_autoptr(IdeGiBase) object = NULL;
  IdeXmlAnalysis *analysis;
  GList *results = NULL;
  IdeXmlDetail *detail;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_GI_VERSION (version));
  g_assert (IDE_IS_XML_SYMBOL_NODE (node));
  g_assert (position != NULL);

  gtype_name = ide_xml_symbol_node_get_attribute_value (parent_node, "class");
  if (dzl_str_empty0 (gtype_name))
    return NULL;

  analysis = ide_xml_position_get_analysis (position);
  req = ide_gi_require_copy (ide_xml_analysis_get_require (analysis));
  version_req = ide_gi_version_get_highest_versions (version);
  ide_gi_require_merge (req, version_req, IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_SOURCE);

  detail = ide_xml_position_get_detail (position);

  if ((object = ide_gi_version_lookup_gtype (version, req, gtype_name)) &&
      ide_gi_base_get_object_type (object) == IDE_GI_BLOB_TYPE_CLASS)
    {
      g_autoptr(GHashTable) visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      fetch_object_properties (object, detail->value, position, &results, visited);
    }

  results = g_list_sort (results, alphabetical_sort_func);
  return results;
}

static void
append_object_signals (IdeGiBase       *object,
                       const gchar     *word,
                       IdeXmlPosition  *position,
                       GList          **results)
{
  IdeXmlPositionKind kind;
  guint n_signals;

  g_assert (object != NULL);
  g_assert (results != NULL);

  n_signals = ide_gi_class_get_n_signals ((IdeGiClass *)object);
  kind = ide_xml_position_get_kind (position);
  for (guint i = 0; i < n_signals; i++)
    {
      g_autoptr(IdeGiSignal) signal = ide_gi_class_get_signal ((IdeGiClass *)object, i);
      const gchar *name = ide_gi_base_get_name ((IdeGiBase *)signal);

      if (dzl_str_empty0 (word) || g_str_has_prefix (name, word))
        {
          IdeXmlProposal *result = ide_xml_proposal_new (name,
                                                         NULL,
                                                         name,
                                                         word,
                                                         g_steal_pointer (&signal),
                                                         -1,
                                                         kind,
                                                         IDE_XML_COMPLETION_TYPE_UI_SIGNAL);
          *results = g_list_prepend (*results, result);
        }
    }
}

static void
fetch_object_signals (IdeGiBase       *object,
                      const gchar     *word,
                      IdeXmlPosition  *position,
                      GList          **results,
                      GHashTable      *visited)
{
  g_autofree gchar *qname = NULL;
  IdeGiBlobType type;
  guint16 n_interfaces;

  g_assert (object != NULL);
  g_assert (results != NULL);

  qname = ide_gi_base_get_qualified_name (object);
  if (g_hash_table_contains (visited, qname))
    return;

  append_object_signals (object, word, position, results);
  g_hash_table_add (visited, g_steal_pointer (&qname));

  type = ide_gi_base_get_object_type (object);
  if (type == IDE_GI_BLOB_TYPE_CLASS)
    {
      g_autoptr(IdeGiClass) parent_class = ide_gi_class_get_parent ((IdeGiClass *)object);

      if ((parent_class ))
        fetch_object_signals ((IdeGiBase *)parent_class, word, position, results, visited);

      n_interfaces = ide_gi_class_get_n_interfaces ((IdeGiClass *)object);
      for (guint i = 0; i < n_interfaces; i++)
        {
          g_autoptr(IdeGiInterface) interface = ide_gi_class_get_interface ((IdeGiClass *)object, i);

          if (interface != NULL)
            fetch_object_signals ((IdeGiBase *)interface, word, position, results, visited);
        }
    }
  else if (type == IDE_GI_BLOB_TYPE_INTERFACE)
    {
      n_interfaces = ide_gi_interface_get_n_prerequisites ((IdeGiInterface *)object);
      for (guint i = 0; i < n_interfaces; i++)
        {
          g_autoptr(IdeGiBase) base = ide_gi_interface_get_prerequisite ((IdeGiInterface *)object, i);

          if (base != NULL)
            fetch_object_signals (base, word, position, results, visited);
        }
    }
}

static GList *
try_get_signal_name_proposals (IdeXmlCompletionProvider *self,
                               IdeGiVersion             *version,
                               IdeXmlSymbolNode         *parent_node,
                               IdeXmlSymbolNode         *node,
                               IdeXmlPosition           *position)
{
  const gchar *gtype_name;
  g_autoptr(IdeGiRequire) version_req = NULL;
  g_autoptr(IdeGiRequire) req = NULL;
  g_autoptr(IdeGiBase) object = NULL;
  IdeXmlAnalysis *analysis;
  GList *results = NULL;
  IdeXmlDetail *detail;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_GI_VERSION (version));
  g_assert (IDE_IS_XML_SYMBOL_NODE (node));
  g_assert (position != NULL);

  gtype_name = ide_xml_symbol_node_get_attribute_value (parent_node, "class");
  if (dzl_str_empty0 (gtype_name))
    return NULL;

  analysis = ide_xml_position_get_analysis (position);
  req = ide_gi_require_copy (ide_xml_analysis_get_require (analysis));
  version_req = ide_gi_version_get_highest_versions (version);
  ide_gi_require_merge (req, version_req, IDE_GI_REQUIRE_MERGE_STRATEGY_KEEP_SOURCE);

  detail = ide_xml_position_get_detail (position);

  if ((object = ide_gi_version_lookup_gtype (version, req, gtype_name)) &&
      ide_gi_base_get_object_type (object) == IDE_GI_BLOB_TYPE_CLASS)
    {
      g_autoptr(GHashTable) visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      fetch_object_signals (object, detail->value, position, &results, visited);
    }

  results = g_list_sort (results, alphabetical_sort_func);
  return results;
}

static GList *
try_get_requires_lib_proposals (IdeXmlCompletionProvider *self,
                                IdeGiVersion             *version,
                                IdeXmlSymbolNode         *parent_node,
                                IdeXmlSymbolNode         *node,
                                IdeXmlPosition           *position)
{
  g_autoptr(GArray) ar = NULL;
  IdeXmlDetail *detail;
  GList *results = NULL;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_GI_VERSION (version));
  g_assert (IDE_IS_XML_SYMBOL_NODE (parent_node) || !parent_node);
  g_assert (IDE_IS_XML_SYMBOL_NODE (node));
  g_assert (position != NULL);

  detail = ide_xml_position_get_detail (position);

  if ((ar = ide_gi_version_complete_prefix (version,
                                            NULL,
                                            IDE_GI_PREFIX_TYPE_PACKAGE,
                                            FALSE,
                                            TRUE,
                                            detail->value)))
    {
      IdeXmlPositionKind kind = ide_xml_position_get_kind (position);

      for (guint i = 0; i < ar->len; i++)
        {
          IdeGiCompletePrefixItem *prefix_item = &g_array_index (ar, IdeGiCompletePrefixItem, i);
          IdeXmlProposal *proposal;

          proposal = ide_xml_proposal_new (prefix_item->word,
                                           NULL,
                                           prefix_item->word,
                                           detail->prefix,
                                           NULL,
                                           -1,
                                           kind,
                                           IDE_XML_COMPLETION_TYPE_UI_PACKAGE);
          results = g_list_prepend (results, proposal);
        }
    }

  return g_list_sort (results, alphabetical_sort_func);
}

static void
populate_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeXmlCompletionProvider *self;
  IdeXmlService *service = (IdeXmlService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeXmlPosition) position = NULL;
  g_autoptr(IdeXmlPath) path = NULL;
  g_autoptr(GPtrArray) candidates = NULL;
  g_autoptr(GListStore) proposals = NULL;
  g_autoptr(GError) error = NULL;
  PopulateState *state;
  IdeXmlSymbolNode *root_node;
  IdeXmlSymbolNode *node;
  IdeXmlSymbolNode *child_node;
  IdeXmlAnalysis *analysis;
  GPtrArray *schemas;
  IdeXmlDetail *detail;
  IdeXmlPositionKind kind;
  gboolean complete_attr_name = FALSE;
  gboolean complete_attr_value = FALSE;
  gint child_pos;
  gboolean is_ui;

  g_assert (IDE_IS_XML_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  if (!(position = ide_xml_service_get_position_from_cursor_finish (service, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  proposals = g_list_store_new (IDE_TYPE_COMPLETION_PROPOSAL);

  analysis = ide_xml_position_get_analysis (position);
  schemas = ide_xml_analysis_get_schemas (analysis);
  root_node = ide_xml_analysis_get_root_node (analysis);
  is_ui = ide_xml_analysis_get_is_ui (analysis);

  node = ide_xml_position_get_node (position);
  child_node = ide_xml_position_get_child_node (position);
  kind = ide_xml_position_get_kind (position);
  detail = ide_xml_position_get_detail (position);
  child_pos = ide_xml_position_get_child_pos (position);

  if (kind == IDE_XML_POSITION_KIND_IN_START_TAG || kind == IDE_XML_POSITION_KIND_IN_END_TAG)
    {
      if (detail->member == IDE_XML_DETAIL_MEMBER_ATTRIBUTE_NAME)
        complete_attr_name = TRUE;
      else if (detail->member == IDE_XML_DETAIL_MEMBER_ATTRIBUTE_VALUE)
        complete_attr_value = TRUE;
    }

  if (complete_attr_value && is_ui && state->gi_service != NULL)
    {
      IdeGiRepository *repository;
      g_autoptr(IdeGiVersion) version = NULL;

      g_assert (child_node != NULL);

      if ((repository = ide_gi_service_get_repository (state->gi_service)) &&
          (version = ide_gi_repository_get_current_version (repository)))
        {
          const gchar *element_name;
          GList *results = NULL;

          element_name = ide_xml_symbol_node_get_element_name (child_node);
          if (dzl_str_equal0 (element_name, "object") && dzl_str_equal0 (detail->name, "class"))
            results = try_get_gtype_proposals (self,
                                               version,
                                               child_node,
                                               position);
          else if (dzl_str_equal0 (element_name, "property") && dzl_str_equal0 (detail->name, "name"))
            results = try_get_property_name_proposals (self,
                                                       version,
                                                       node,
                                                       child_node,
                                                       position);
          else if (dzl_str_equal0 (element_name, "signal") && dzl_str_equal0 (detail->name, "name"))
            results = try_get_signal_name_proposals (self,
                                                     version,
                                                     node,
                                                     child_node,
                                                     position);
          else if (dzl_str_equal0 (element_name, "requires") && dzl_str_equal0 (detail->name, "lib"))
            results = try_get_requires_lib_proposals (self,
                                                      version,
                                                      node,
                                                      child_node,
                                                      position);
          add_to_store (proposals, results);
          g_list_free_full (results, g_object_unref);
        }
    }

  if (complete_attr_name || complete_attr_value)
    {
      g_assert (child_node != NULL);
      path = get_path (child_node, root_node);
    }
  else
    path = get_path (node, root_node);

  if (schemas == NULL)
    goto cleanup;

  if ((candidates = get_matching_candidates (self, schemas, path)))
    {
      IdeXmlRngDefine *def;

      if (complete_attr_name)
        {
          for (guint i = 0; i < candidates->len; ++i)
            {
              GList *results = NULL;

              def = g_ptr_array_index (candidates, i);
              results = get_attributes_proposals (position, def);
              add_to_store (proposals, results);
            }
        }
      else if (complete_attr_value)
        {
          for (guint i = 0; i < candidates->len; ++i)
            {
              GList *results = NULL;

              def = g_ptr_array_index (candidates, i);
              results = get_values_proposals (position, def);
              add_to_store (proposals, results);
            }
        }
      else
        {
          g_autoptr(GPtrArray) items = NULL;
          GList *results = NULL;

          items = g_ptr_array_new_with_free_func ((GDestroyNotify)completion_item_free);
          if (child_pos != -1)
            {
              g_autoptr(IdeXmlSymbolNode) candidate_node =
                candidate_node = ide_xml_symbol_node_new ("internal", NULL, "", IDE_SYMBOL_XML_ELEMENT);

              ide_xml_position_set_child_node (position, candidate_node);
            }

          for (guint i = 0; i < candidates->len; ++i)
            {
              MatchingState *initial_state;

              def = g_ptr_array_index (candidates, i);

              initial_state = create_initial_matching_state (position, def, items);
              process_matching_state (initial_state, def);
              matching_state_free (initial_state);
            }

          results = get_element_proposals (position, items, detail->prefix);
          add_to_store (proposals, results);
        }
    }

cleanup:

  ide_task_return_object (task, g_steal_pointer (&proposals));
}

static void
ide_xml_completion_provider_populate_async (IdeCompletionProvider *provider,
                                            IdeCompletionContext  *context,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data)
{
  IdeXmlCompletionProvider *self = (IdeXmlCompletionProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  GtkTextBuffer *buffer;
  IdeContext *ide_context;
  IdeXmlService *xml_service;
  IdeGiService *gi_service;
  GtkTextIter iter;
  PopulateState *state;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_completion_provider_populate_async);

  if (!(ide_context = ide_object_get_context (IDE_OBJECT (self))) ||
      !(xml_service = ide_context_get_service_typed (ide_context, IDE_TYPE_XML_SERVICE)))
    {
      ide_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Xml service not available");
      return;
    }

  gi_service = ide_context_get_service_typed (ide_context, IDE_TYPE_GI_SERVICE);

  buffer = ide_completion_context_get_buffer (context);
  if (gtk_text_buffer_get_has_selection (buffer))
    gtk_text_buffer_get_selection_bounds (buffer, &iter, NULL);
  else
    ide_completion_context_get_bounds (context, NULL, &iter);

  state = g_slice_new0 (PopulateState);
  state->ifile = g_object_ref (ide_buffer_get_file (IDE_BUFFER (buffer)));
  state->gi_service = (gi_service != NULL) ? g_object_ref (gi_service) : NULL;
  state->line = gtk_text_iter_get_line (&iter) + 1;
  state->line_offset = gtk_text_iter_get_line_offset (&iter) + 1;

  ide_task_set_task_data (task, state, populate_state_free);

  ide_xml_service_get_position_from_cursor_async (xml_service,
                                                  state->ifile,
                                                  IDE_BUFFER (buffer),
                                                  state->line,
                                                  state->line_offset,
                                                  cancellable,
                                                  populate_cb,
                                                  g_steal_pointer (&task));
}

static GListModel *
ide_xml_completion_provider_populate_finish (IdeCompletionProvider  *provider,
                                             GAsyncResult           *result,
                                             GError                **error)
{
  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
ide_xml_completion_provider_display_proposal (IdeCompletionProvider   *provider,
                                              IdeCompletionListBoxRow *row,
                                              IdeCompletionContext    *context,
                                              const gchar             *typed_text,
                                              IdeCompletionProposal   *proposal)
{
  IdeXmlProposal *item = (IdeXmlProposal *)proposal;
  IdeXmlCompletionType completion_type;
  const gchar *header;
  const gchar *label;
  const gchar *icon_name;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_LIST_BOX_ROW (row));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_COMPLETION_PROPOSAL (proposal));

  label = ide_xml_proposal_get_label (item);
  header = ide_xml_proposal_get_header (item);

  completion_type = ide_xml_proposal_get_completion_type (item);
  switch (completion_type)
    {
    case IDE_XML_COMPLETION_TYPE_NONE:
    case IDE_XML_COMPLETION_TYPE_ELEMENT:
    case IDE_XML_COMPLETION_TYPE_ATTRIBUTE:
    case IDE_XML_COMPLETION_TYPE_VALUE:
    case IDE_XML_COMPLETION_TYPE_UI_PACKAGE:
    case IDE_XML_COMPLETION_TYPE_UI_GTYPE:
      icon_name = NULL;
      break;

    case IDE_XML_COMPLETION_TYPE_UI_PROPERTY:
      icon_name = "ui-property-symbolic";
      break;

    case IDE_XML_COMPLETION_TYPE_UI_SIGNAL:
      icon_name = "ui-signal-symbolic";
      break;

    default:
      g_assert_not_reached ();
    }

  ide_completion_list_box_row_set_icon_name (row, icon_name);
  ide_completion_list_box_row_set_left (row, header);
  ide_completion_list_box_row_set_right (row, NULL);
  ide_completion_list_box_row_set_center_markup (row, label);
}

static void
adjust_bounds (GtkTextIter        *begin,
               GtkTextIter        *end,
               const gchar        *text,
               IdeXmlPositionKind  kind)
{
  gint count;
  GtkTextIter bound;

  if (text == NULL || 0 == (count = g_utf8_strlen (text, -1)))
    return;

  gtk_text_iter_backward_chars (begin, count);
  bound = *begin;

  /* FIXME: this assertion is still valid ? */
  if (kind == IDE_XML_POSITION_KIND_IN_CONTENT &&
      gtk_text_iter_backward_char (&bound) &&
      gtk_text_iter_get_char (&bound) == '<')
    {
      *begin = bound;
    }
}

static void
ide_xml_completion_provider_activate_proposal (IdeCompletionProvider *provider,
                                               IdeCompletionContext  *context,
                                               IdeCompletionProposal *proposal,
                                               const GdkEventKey     *key)
{
  IdeXmlProposal *item = (IdeXmlProposal *)proposal;
  IdeXmlPositionKind  position_kind;
  IdeXmlCompletionType  completion_type;
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;
  g_autofree gchar *tmp_text = NULL;
  const gchar *text;
  const gchar *prefix;
  gint insert_position;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_COMPLETION_PROPOSAL (proposal));

  if (ide_completion_context_get_bounds (context, NULL, &end))
    {
      begin = end;
      buffer = ide_completion_context_get_buffer (context);
      gtk_text_buffer_begin_user_action (buffer);

      if (gtk_text_buffer_get_has_selection (buffer))
        gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

      prefix = ide_xml_proposal_get_prefix (item);
      position_kind = ide_xml_proposal_get_kind (item);
      adjust_bounds (&begin, &end, prefix, position_kind);
      gtk_text_buffer_delete (buffer, &begin, &end);

      completion_type = ide_xml_proposal_get_completion_type (item);
      text = ide_xml_proposal_get_text (item);

      /* We add a space if there's more attributes following */
      if (completion_type == IDE_XML_COMPLETION_TYPE_ATTRIBUTE)
        {
          gunichar next_ch = gtk_text_iter_get_char (&begin);

          if (next_ch != '>' && !g_unichar_isspace (next_ch))
            text = tmp_text = g_strconcat (text, " ", NULL);
        }

      gtk_text_buffer_insert (buffer, &begin, text, -1);

      insert_position = ide_xml_proposal_get_insert_position (item);
      if (insert_position > -1)
       {
          g_assert (insert_position <= g_utf8_strlen (text, -1));

          gtk_text_iter_backward_chars (&begin, g_utf8_strlen (text, -1) - insert_position);
          gtk_text_buffer_place_cursor (buffer, &begin);
        }

      gtk_text_buffer_end_user_action (buffer);
    }
}

static gchar *
ide_xml_completion_provider_get_comment (IdeCompletionProvider *provider,
                                         IdeCompletionProposal *proposal)
{
  IdeXmlProposal *item = (IdeXmlProposal *)proposal;
  IdeXmlCompletionType type;
  g_autoptr(IdeGiDoc) doc = NULL;
  gpointer data;
  gboolean has_more;

  type = ide_xml_proposal_get_completion_type (item);

  if (type == IDE_XML_COMPLETION_TYPE_UI_PROPERTY ||
      type == IDE_XML_COMPLETION_TYPE_UI_SIGNAL ||
      type == IDE_XML_COMPLETION_TYPE_UI_GTYPE)
    {
      data = ide_xml_proposal_get_data (item);
      if ((doc = ide_gi_base_get_doc ((IdeGiBase *)data)))
        {
          const gchar *text = ide_gi_doc_get_doc (doc);
          gsize len;

          if (text != NULL && 0 != (len = ide_xml_utils_get_text_limit (text, 2, 20, &has_more)))
            {
              if (has_more)
                {
                  GString *str = g_string_sized_new (len + 20);

                  g_string_append_len (str, text, len);
                  g_string_append (str, _(" [More…]"));
                  return g_string_free (str, FALSE);
                }
              else
                return g_strndup (text, len);
            }
        }
    }

  return NULL;
}

static gchar *
ide_xml_completion_provider_get_title (IdeCompletionProvider *provider)
{
  return g_strdup ("Xml Completion");
}

static void
ide_xml_completion_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_xml_completion_provider_parent_class)->finalize (object);
}

static void
ide_xml_completion_provider_class_init (IdeXmlCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_completion_provider_finalize;
}

static void
ide_xml_completion_provider_init (IdeXmlCompletionProvider *self)
{
}

static void
completion_provider_init (IdeCompletionProviderInterface *iface)
{
  iface->activate_proposal = ide_xml_completion_provider_activate_proposal;
  iface->display_proposal = ide_xml_completion_provider_display_proposal;
  iface->populate_async = ide_xml_completion_provider_populate_async;
  iface->populate_finish = ide_xml_completion_provider_populate_finish;
  iface->get_comment = ide_xml_completion_provider_get_comment;
  iface->get_title = ide_xml_completion_provider_get_title;
  /* We don't use refilter currently because we don't get a good 'begin'
   * bound, so the searched word, so refilter triggering.
   */
}
