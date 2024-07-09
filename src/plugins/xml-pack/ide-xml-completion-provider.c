/* ide-xml-completion-provider.c
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

#define G_LOG_DOMAIN "xml-completion"

#include "config.h"

#include <gtksourceview/gtksource.h>
#include <libpeas.h>

#include <libide-sourceview.h>

#include "ide-xml-completion-attributes.h"
#include "ide-xml-completion-provider.h"
#include "ide-xml-completion-values.h"
#include "ide-xml-path.h"
#include "ide-xml-position.h"
#include "ide-xml-proposal.h"
#include "ide-xml-rng-define.h"
#include "ide-xml-schema-cache-entry.h"
#include "ide-xml-service.h"
#include "ide-xml-symbol-node.h"
#include "ide-xml-types.h"

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
  guint             retry : 1;
} MatchingState;

typedef struct _StateStackItem
{
  GPtrArray        *children;
  IdeXmlSymbolNode *candidate_node;
} StateStackItem;

typedef struct
{
  GFile *file;
  gint   line;
  gint   line_offset;
} PopulateState;

typedef struct
{
  gchar           *label;
  IdeXmlRngDefine *define;
} CompletionItem;

static void      completion_provider_init (GtkSourceCompletionProviderInterface *iface);
static gboolean  process_matching_state   (MatchingState                        *state,
                                           IdeXmlRngDefine                      *define);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeXmlCompletionProvider,
                               ide_xml_completion_provider,
                               IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_init))

static void
populate_state_free (PopulateState *state)
{
  g_assert (state != NULL);

  g_clear_object (&state->file);
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

  copy = g_ptr_array_new ();
  for (guint i = 0; i < children->len; ++i)
    g_ptr_array_add (copy, g_ptr_array_index (children, i));

  return copy;
}

static void
state_stack_item_clear (gpointer data)
{
  StateStackItem *item = data;

  g_assert (item != NULL);

  g_clear_pointer (&item->children, g_ptr_array_unref);
  item->candidate_node = NULL;
}

static GArray *
state_stack_new (void)
{
  GArray *stack;

  stack = g_array_new (FALSE, TRUE, sizeof (StateStackItem));
  g_array_set_clear_func (stack, state_stack_item_clear);

  return stack;
}

static void
state_stack_push (MatchingState *state)
{
  StateStackItem item;

  g_assert (state->stack != NULL);

  item.children = copy_children (state->children);
  item.candidate_node = state->candidate_node;

  g_array_append_val (state->stack, item);
}

static gboolean
state_stack_pop (MatchingState *state)
{
  StateStackItem *item;
  guint len;

  g_assert (state->stack != NULL);

  len = state->stack->len;
  if (len == 0)
    return FALSE;

  item = &g_array_index (state->stack, StateStackItem, len - 1);
  g_clear_pointer (&state->children, g_ptr_array_unref);

  state->children = g_steal_pointer (&item->children);
  state->candidate_node = item->candidate_node;

  g_array_remove_index (state->stack, len - 1);
  return TRUE;
}

static gboolean
state_stack_drop (MatchingState *state)
{
  guint len;

  g_assert (state->stack != NULL);

  len = state->stack->len;
  if (len == 0)
    return FALSE;

  g_array_remove_index (state->stack, len - 1);
  return TRUE;
}

static gboolean
state_stack_copy (MatchingState *state)
{
  StateStackItem *item;
  guint len;

  g_assert (state->stack != NULL);

  len = state->stack->len;
  if (len == 0)
    return FALSE;

  item = &g_array_index (state->stack, StateStackItem, len - 1);
  state->children = copy_children (item->children);
  state->candidate_node = item->candidate_node;

  g_array_remove_index (state->stack, len - 1);
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
  while (current != NULL && current != root_node)
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
  if (sub_array->len == 0)
    return;

  for (guint i = 0; i < sub_array->len; ++i)
    g_ptr_array_add (array, g_ptr_array_index (sub_array, i));

  g_ptr_array_remove_range (sub_array, 0, sub_array->len);
}

static void
get_matching_nodes (IdeXmlPath       *path,
                    gint              index,
                    IdeXmlRngDefine  *define,
                    GPtrArray        *candidates)
{
  IdeXmlSymbolNode *node;
  IdeXmlRngDefine *child;
  IdeXmlRngDefineType type;
  gint current_index;

  g_assert (path != NULL);
  g_assert (define != NULL);
  g_assert (candidates != NULL);

  if (define == NULL)
    return;

  node = g_ptr_array_index (path->nodes, index);

  while (define != NULL)
    {
      child = NULL;
      current_index = index;
      type = define->type;

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
  g_autoptr (GPtrArray) candidates_tmp = NULL;
  g_autoptr (GPtrArray) defines = NULL;
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

      get_matching_nodes (path, 0, grammar->start_defines, candidates_tmp);
      move_candidates (candidates, candidates_tmp);
    }

  return candidates;
}

static CompletionItem *
completion_item_new (const gchar     *label,
                     IdeXmlRngDefine *define)
{
  CompletionItem *item;

  g_assert (!ide_str_empty0 (label));
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
  const gchar *prefix;

  g_assert (IDE_IS_XML_SYMBOL_NODE (position->node));
  g_assert (define != NULL);

  state = g_slice_new0 (MatchingState);

  state->parent_node = NULL;
  state->position = position;
  state->define = define;
  state->items = items;
  state->candidate_node = ide_xml_position_get_child_node (position);

  state->children = g_ptr_array_new ();
  state->stack = state_stack_new ();

  prefix = ide_xml_position_get_prefix (position);
  state->prefix = (prefix != NULL) ? g_strdup (prefix) : NULL;

  return state;
}

G_GNUC_UNUSED static MatchingState *
matching_state_copy (MatchingState *state)
{
  MatchingState *new_state;

  new_state = g_slice_new0 (MatchingState);

  new_state->parent_node = (state->parent_node != NULL) ? g_object_ref (state->parent_node) : NULL;
  new_state->candidate_node = (state->candidate_node != NULL) ? g_object_ref (state->candidate_node) : NULL;
  new_state->position = state->position;

  new_state->define = state->define;
  new_state->define_cursor = state->define_cursor;

  new_state->child_cursor = state->child_cursor;

  new_state->prefix = (state->prefix != NULL) ? g_strdup (state->prefix) : NULL;
  new_state->items = state->items;

  if (state->children != NULL)
    {
      new_state->children = g_ptr_array_new ();
      for (guint i = 0; i < state->children->len; ++i)
        g_ptr_array_add (new_state->children, g_ptr_array_index (state->children, i));
    }

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
  IdeXmlSymbolNode *node, *pos_node, *candidate_node;
  guint nb_nodes;
  gint child_pos;

  g_assert (define != NULL);
  g_assert (items != NULL);

  state = matching_state_new (position, define, items);
  child_pos = ide_xml_position_get_child_pos (position);

  candidate_node = ide_xml_position_get_child_node (position);
  pos_node = ide_xml_position_get_node (position);
  g_assert (IDE_IS_XML_SYMBOL_NODE (pos_node));

  nb_nodes = ide_xml_symbol_node_get_n_direct_children (pos_node);
  for (guint i = 0; i < nb_nodes; ++i)
    {
      /* Inject a fake node at child_pos */
      if (child_pos == i)
        g_ptr_array_add (state->children, candidate_node);

      node = (IdeXmlSymbolNode *)ide_xml_symbol_node_get_nth_direct_child (pos_node, i);
      g_ptr_array_add (state->children, node);
    }

  state->candidate_node = g_object_ref (candidate_node);
  state->is_initial_state = TRUE;
  return state;
}

static gboolean
is_define_equal_node (IdeXmlRngDefine  *define,
                      IdeXmlSymbolNode *node)
{
  g_assert (define != NULL);
  g_assert (IDE_IS_XML_SYMBOL_NODE (node));

  return ide_str_equal0 (ide_xml_symbol_node_get_element_name (node), define->name);
}

static gboolean
is_element_matching (MatchingState *state)
{
  IdeXmlSymbolNode *node;
  CompletionItem *item;
  const gchar *name;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_ELEMENT);

  /* XXX: we skip element without a name for now */
  if (ide_str_empty0 ((gchar *)state->define->name))
    return FALSE;

  if (state->children->len == 0)
    return FALSE;

  state->retry = FALSE;
  node = g_ptr_array_index (state->children, 0);

  if (state->candidate_node == node)
    {
      name = (gchar *)state->define->name;
      if (ide_str_empty0 (state->prefix) || g_str_has_prefix ((gchar *)state->define->name, state->prefix))
        {
          state->candidate_node = NULL;
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
            {
              state->retry = FALSE;
            }
          else
            {
              state_stack_drop (state);
              return TRUE;
            }
        }

      if (NULL != (defines = defines->next))
        {
          state_stack_copy (state);
        }
      else
        {
          state_stack_pop (state);
        }
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

  if (NULL == (defines = state->define->content))
    return TRUE;

  state_stack_push (state);
  while (defines != NULL)
    {
      if (!process_matching_state (state, defines))
        {
          is_matching = FALSE;
          break;
        }

      if (NULL != (defines = defines->next))
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
      state->is_initial_state = FALSE;
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
    case IDE_XML_RNG_DEFINE_START:
    case IDE_XML_RNG_DEFINE_PARAM:
    case IDE_XML_RNG_DEFINE_EXCEPT:
    case IDE_XML_RNG_DEFINE_LIST:
    case IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP:
      is_matching = FALSE;
      break;

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
                       GPtrArray      *items)
{
  CompletionItem *completion_item;
  IdeXmlProposal *item;
  GList *results = NULL;
  const gchar *start = "";

  g_assert (position != NULL);
  g_assert (items != NULL);

  if (ide_xml_position_get_kind (position) == IDE_XML_POSITION_KIND_IN_CONTENT)
    start = "<";

  for (guint i = 0; i < items->len; ++i)
    {
      g_autofree gchar *label = NULL;
      g_autofree gchar *text = NULL;

      /* TODO: This should probably use snippets */

      completion_item = g_ptr_array_index (items, i);
      label = g_strconcat ("&lt;", completion_item->label, "&gt;", NULL);
      text = g_strconcat (start, completion_item->label, ">", "</", completion_item->label, ">", NULL);
      item = ide_xml_proposal_new (text, label);

      results = g_list_prepend (results, item);
    }

  return results;
}

static GList *
get_attributes_proposals (IdeXmlPosition  *position,
                          IdeXmlRngDefine *define)
{
  IdeXmlSymbolNode *node;
  IdeXmlProposal *item;
  g_autoptr(GPtrArray) attributes = NULL;
  GList *results = NULL;

  node = ide_xml_position_get_child_node (position);
  if (NULL != (attributes = ide_xml_completion_attributes_get_matches (define, node, TRUE)))
    {
      for (guint i = 0; i < attributes->len; ++i)
        {
          g_autofree gchar *name = NULL;
          g_autofree gchar *text = NULL;
          MatchItem *match_item;

          match_item = g_ptr_array_index (attributes, i);
          /* XXX: can't get the markup working, add () */
          if (match_item->is_optional)
            name = g_strconcat ("<i>(", match_item->name, ")</i>", NULL);
          else
            name = g_strdup (match_item->name);

          text = g_strconcat (match_item->name, "=\"\"", NULL);
          item = ide_xml_proposal_new (text, name);

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
  IdeXmlRngDefine *attr_define = NULL;
  g_autoptr(GPtrArray) attributes = NULL;
  g_autoptr(GPtrArray) values = NULL;
  IdeXmlProposal *item;
  GList *results = NULL;

  node = ide_xml_position_get_child_node (position);
  g_assert (!ide_str_empty0 (position->detail_name));

  if (NULL != (attributes = ide_xml_completion_attributes_get_matches (define, node, FALSE)))
    {
      MatchItem *match_item;
      const gchar *detail_name;
      const gchar *detail_value;
      const gchar *content;

      detail_name = ide_xml_position_get_detail_name (position);
      detail_value = ide_xml_position_get_detail_value (position);

      for (gint j = 0; j < attributes->len; ++j)
        {
          match_item = g_ptr_array_index (attributes, j);
          if (ide_str_equal0 (detail_name, match_item->name))
            {
              attr_define = match_item->define;
              break;
            }
        }

      if (attr_define != NULL)
        {
          ide_xml_symbol_node_print (node, 0, FALSE, TRUE, TRUE);
          content = ide_xml_symbol_node_get_attribute_value (node, match_item->name);

          if (NULL != (values = ide_xml_completion_values_get_matches (attr_define, content, detail_value)))
            {
              for (guint i = 0; i < values->len; ++i)
                {
                  ValueMatchItem *value_match_item = g_ptr_array_index (values, i);

                  item = ide_xml_proposal_new (value_match_item->name,
                                               value_match_item->name);

                  results = g_list_prepend (results, item);
                }
            }
        }
    }

  return results;
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
  g_autoptr(GPtrArray) items = NULL;
  g_autoptr(GListStore) proposals = NULL;
  g_autoptr(GError) error = NULL;
  IdeXmlSymbolNode *root_node;
  IdeXmlSymbolNode *node;
  IdeXmlSymbolNode *candidate_node;
  IdeXmlRngDefine *def;
  IdeXmlAnalysis *analysis;
  MatchingState *initial_state;
  GPtrArray *schemas;
  IdeXmlPositionDetail detail;
  IdeXmlPositionKind kind;
  gboolean complete_attributes = FALSE;
  gboolean complete_values = FALSE;
  gint child_pos;

  g_assert (IDE_IS_XML_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  position = ide_xml_service_get_position_from_cursor_finish (service, result, &error);

  if (error != NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  proposals = g_list_store_new (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL);

  analysis = ide_xml_position_get_analysis (position);
  schemas = ide_xml_analysis_get_schemas (analysis);
  root_node = ide_xml_analysis_get_root_node (analysis);
  node = ide_xml_position_get_node (position);
  kind = ide_xml_position_get_kind (position);
  detail = ide_xml_position_get_detail (position);
  child_pos = ide_xml_position_get_child_pos (position);

  if (kind == IDE_XML_POSITION_KIND_IN_START_TAG || kind == IDE_XML_POSITION_KIND_IN_END_TAG)
    {
      if (detail == IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_NAME)
        complete_attributes = TRUE;
      else if (detail == IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_VALUE)
        complete_values = TRUE;
    }

  if (complete_attributes || complete_values)
    {
      IdeXmlSymbolNode *child_node;

      child_node = ide_xml_position_get_child_node (position);
      g_assert (child_node != NULL);

      path = get_path (child_node, root_node);
    }
  else
    path = get_path (node, root_node);

  if (schemas == NULL)
    goto cleanup;

  if ((candidates = get_matching_candidates (self, schemas, path)))
    {
      if (complete_attributes)
        {
          for (guint i = 0; i < candidates->len; ++i)
            {
              g_autoptr(GList) results = NULL;

              def = g_ptr_array_index (candidates, i);
              results = get_attributes_proposals (position, def);
              add_to_store (proposals, results);
            }
        }
      else if (complete_values)
        {
          for (guint i = 0; i < candidates->len; ++i)
            {
              g_autoptr(GList) results = NULL;

              def = g_ptr_array_index (candidates, i);
              results = get_values_proposals (position, def);
              add_to_store (proposals, results);
            }
        }
      else
        {
          g_autoptr(GList) results = NULL;

          items = g_ptr_array_new_with_free_func ((GDestroyNotify)completion_item_free);
          if (child_pos != -1)
            {
              candidate_node = ide_xml_symbol_node_new ("internal", NULL, "", IDE_SYMBOL_KIND_XML_ELEMENT);
              ide_xml_position_set_child_node (position, candidate_node);
            }

          for (guint i = 0; i < candidates->len; ++i)
            {
              def = g_ptr_array_index (candidates, i);

              initial_state = create_initial_matching_state (position, def, items);
              process_matching_state (initial_state, def);
              matching_state_free (initial_state);
            }

          results = get_element_proposals (position, items);
          add_to_store (proposals, results);
        }
    }

cleanup:

  ide_task_return_object (task, g_steal_pointer (&proposals));
}

static void
ide_xml_completion_provider_populate_async (GtkSourceCompletionProvider *provider,
                                            GtkSourceCompletionContext  *context,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data)
{
  IdeXmlCompletionProvider *self = (IdeXmlCompletionProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  GtkSourceBuffer *buffer;
  IdeXmlService *service;
  PopulateState *state;
  IdeContext *ide_context;
  GtkTextIter iter;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_completion_provider_populate_async);

  ide_context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_xml_service_from_context (ide_context);

  buffer = gtk_source_completion_context_get_buffer (context);
  gtk_source_completion_context_get_bounds (context, &iter, NULL);

  state = g_slice_new0 (PopulateState);
  state->file = g_object_ref (ide_buffer_get_file (IDE_BUFFER (buffer)));
  state->line = gtk_text_iter_get_line (&iter) + 1;
  state->line_offset = gtk_text_iter_get_line_offset (&iter) + 1;

  ide_task_set_task_data (task, state, populate_state_free);

  ide_xml_service_get_position_from_cursor_async (service,
                                                  state->file,
                                                  IDE_BUFFER (buffer),
                                                  state->line,
                                                  state->line_offset,
                                                  cancellable,
                                                  populate_cb,
                                                  g_steal_pointer (&task));
}

static GListModel *
ide_xml_completion_provider_populate_finish (GtkSourceCompletionProvider  *provider,
                                             GAsyncResult           *result,
                                             GError                **error)
{
  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
ide_xml_completion_provider_refilter (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context,
                                      GListModel            *model)
{
  g_autofree gchar *word = NULL;
  g_autofree gchar *casefold = NULL;
  guint n_items;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (G_IS_LIST_MODEL (model));

  n_items = g_list_model_get_n_items (model);
  word = gtk_source_completion_context_get_word (context);
  casefold = g_utf8_casefold (word, -1);

  /* TODO: This doesn't work with backspace. We need to track things
   *       separately (with a custom model or something.
   */

  for (guint i = n_items; i > 0; i--)
    {
      g_autoptr(IdeXmlProposal) proposal = g_list_model_get_item (model, i - 1);
      const gchar *label = ide_xml_proposal_get_label (proposal);
      guint priority;

      if (!gtk_source_completion_fuzzy_match (label, casefold, &priority))
        g_list_store_remove (G_LIST_STORE (model), i - 1);
    }
}

static void
ide_xml_completion_provider_display (GtkSourceCompletionProvider *provider,
                                     GtkSourceCompletionContext  *context,
                                     GtkSourceCompletionProposal *proposal,
                                     GtkSourceCompletionCell     *cell)
{
  IdeXmlProposal *item = (IdeXmlProposal *)proposal;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal));

  switch (gtk_source_completion_cell_get_column (cell))
    {
    case GTK_SOURCE_COMPLETION_COLUMN_ICON:
      gtk_source_completion_cell_set_icon_name (cell, "text-xml-symbolic");
      break;

    case GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT:
      gtk_source_completion_cell_set_markup (cell, ide_xml_proposal_get_label (item));
      break;

    case GTK_SOURCE_COMPLETION_COLUMN_COMMENT:
    case GTK_SOURCE_COMPLETION_COLUMN_DETAILS:
    case GTK_SOURCE_COMPLETION_COLUMN_BEFORE:
    case GTK_SOURCE_COMPLETION_COLUMN_AFTER:
    default:
      gtk_source_completion_cell_set_text (cell, NULL);
      break;
    }
}

static void
ide_xml_completion_provider_activate (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context,
                                      GtkSourceCompletionProposal *proposal)
{
  IdeXmlProposal *item = (IdeXmlProposal *)proposal;
  GtkSourceBuffer *source_buffer;
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;
  const gchar *text;

  g_assert (IDE_IS_XML_COMPLETION_PROVIDER (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal));

  text = ide_xml_proposal_get_text (item);

  source_buffer = gtk_source_completion_context_get_buffer (context);
  gtk_source_completion_context_get_bounds (context, &begin, &end);

  buffer = GTK_TEXT_BUFFER (source_buffer);
  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, text, -1);
  gtk_text_buffer_end_user_action (buffer);
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
completion_provider_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->activate = ide_xml_completion_provider_activate;
  iface->display = ide_xml_completion_provider_display;
  iface->populate_async = ide_xml_completion_provider_populate_async;
  iface->populate_finish = ide_xml_completion_provider_populate_finish;
  iface->refilter = ide_xml_completion_provider_refilter;
}
