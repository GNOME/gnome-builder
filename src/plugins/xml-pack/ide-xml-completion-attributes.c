/* ide-xml-completion-attributes.c
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

#include "config.h"

#include "ide-xml-completion-attributes.h"
#include "ide-xml-position.h"

typedef struct _MatchingState
{
  IdeXmlSymbolNode *node;
  IdeXmlRngDefine  *define;
  IdeXmlRngDefine  *orig_define;
  GPtrArray        *node_attr;
  GPtrArray        *match_children;

  guint             is_initial_state : 1;
  guint             is_optional : 1;
} MatchingState;

static GPtrArray * process_matching_state (MatchingState   *state,
                                           IdeXmlRngDefine *define);

static MatchItem *
match_item_new (IdeXmlRngDefine *define,
                const gchar     *attr_name,
                gint             attr_pos,
                gboolean         is_optional)
{
  MatchItem *item;

  g_assert (!ide_str_empty0 (attr_name));

  item = g_slice_new0 (MatchItem);

  item->define = define;
  item->name = g_strdup (attr_name);
  item->pos = attr_pos;
  item->is_optional = is_optional;

  return item;
}

static void
match_item_free (gpointer data)
{
  MatchItem *item = (MatchItem *)data;

  g_clear_pointer (&item->name, g_free);
}

static GPtrArray *
match_children_new (void)
{
  GPtrArray *ar;

  ar = g_ptr_array_new_with_free_func ((GDestroyNotify)match_item_free);
  return ar;
}

static void
match_children_add (GPtrArray *to_children,
                    GPtrArray *from_children)
{
  MatchItem *to_item;
  MatchItem *from_item;

  g_assert (to_children != NULL);
  g_assert (from_children != NULL);

  for (gint i = 0; i < from_children->len; ++i)
    {
      from_item = g_ptr_array_index (from_children, i);
      to_item = match_item_new (from_item->define, from_item->name, from_item->pos, from_item->is_optional);
      g_ptr_array_add (to_children, to_item);
    }
}

static MatchingState *
matching_state_new (IdeXmlRngDefine  *define,
                    IdeXmlSymbolNode *node)
{
  MatchingState *state;

  g_assert (define != NULL);

  state = g_slice_new0 (MatchingState);

  state->define = define;
  state->orig_define = define;
  state->node = node;

  state->node_attr = g_ptr_array_new_with_free_func (g_free);
  state->is_initial_state = FALSE;
  state->is_optional = FALSE;

  return state;
}

static void
matching_state_free (MatchingState *state)
{
  g_clear_object (&state->node);

  g_clear_pointer (&state->node_attr, g_ptr_array_unref);
}

static GPtrArray *
process_attribute (MatchingState *state)
{
  GPtrArray *match_children;
  MatchItem *item;
  const gchar *name;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_ATTRIBUTE);

  match_children = match_children_new ();
  name = (gchar *)state->define->name;
  /* XXX: we skip element without a name for now */
  if (ide_str_empty0 (name))
    return match_children;

  item = match_item_new (state->define, name, state->define->pos, state->is_optional);
  g_ptr_array_add (match_children, item);

  return match_children;
}

static gint
get_match_children_min_pos (GPtrArray *match_children)
{
  MatchItem *item;
  gint min_pos = G_MAXINT;

  g_assert (match_children != NULL);

  for (gint i = 0; i < match_children->len; i++)
    {
      item = g_ptr_array_index (match_children, i);
      if (item->pos == -1)
        continue;

      if (item->pos < min_pos)
        min_pos = item->pos;
    }

  if (min_pos == G_MAXINT)
    return -1;
  else
    return min_pos;
}

static GPtrArray *
process_choice (MatchingState *state)
{
  GPtrArray *match_children;
  GPtrArray *match;
  GPtrArray *tmp_matches;
  GPtrArray *min_pos_match = NULL;
  IdeXmlRngDefine *defines;
  gboolean node_has_attr G_GNUC_UNUSED;
  gint min_pos = G_MAXINT;
  gint pos;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_CHOICE);

  match_children = match_children_new ();
  if (NULL == (defines = state->define->content))
    return match_children;

  tmp_matches = g_ptr_array_new_with_free_func ((GDestroyNotify)g_ptr_array_unref);
  node_has_attr = (state->node_attr->len > 0);
  while (defines != NULL)
    {
      if (NULL != (match = process_matching_state (state, defines)))
        {
          pos = get_match_children_min_pos (match);
          if (pos != -1 && pos < min_pos)
            {
              g_clear_pointer (&min_pos_match, g_ptr_array_unref);

              min_pos = pos;
              min_pos_match = g_ptr_array_ref (match);
            }

          g_ptr_array_add (tmp_matches, match);
        }

      defines = defines->next;
    }

  if (min_pos_match != NULL)
    {
      g_ptr_array_unref (match_children);
      g_ptr_array_unref (tmp_matches);

      return min_pos_match;
    }
  else
    {
      for (gint i = 0; i < tmp_matches->len; i++)
        match_children_add (match_children, g_ptr_array_index (tmp_matches, i));

      return match_children;
    }
}

static GPtrArray *
process_group (MatchingState *state)
{
  GPtrArray *match_children;
  GPtrArray *match;
  IdeXmlRngDefine *defines;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_GROUP ||
            state->define->type == IDE_XML_RNG_DEFINE_ATTRIBUTE ||
            state->define->type == IDE_XML_RNG_DEFINE_ZEROORMORE ||
            state->define->type == IDE_XML_RNG_DEFINE_ONEORMORE ||
            state->define->type == IDE_XML_RNG_DEFINE_OPTIONAL);

  match_children = match_children_new ();
  if (NULL == (defines = state->define->content))
    return match_children;

  while (defines != NULL)
    {
      if (NULL != (match = process_matching_state (state, defines)))
        {
          /* TODO: use move */
          match_children_add (match_children, match);
          g_ptr_array_unref (match);
        }

      defines = defines->next;
    }

  return match_children;
}

static GPtrArray *
process_attributes_group (MatchingState *state)
{
  GPtrArray *match_children;
  GPtrArray *match;
  IdeXmlRngDefine *defines;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_ELEMENT);

  match_children = match_children_new ();
  if (NULL == (defines = state->define->attributes))
    return match_children;

  while (defines != NULL)
    {
      if (NULL != (match = process_matching_state (state, defines)))
        {
          /* TODO: use move */
          match_children_add (match_children, match);
          g_ptr_array_unref (match);
        }

      defines = defines->next;
    }

  return match_children;
}

static GPtrArray *
process_matching_state (MatchingState   *state,
                        IdeXmlRngDefine *define)
{
  IdeXmlRngDefine *old_define;
  IdeXmlRngDefineType type;
  GPtrArray *match_children = NULL;
  gboolean old_optional;

  g_assert (state != NULL);
  g_assert (define != NULL);

  old_define = state->define;
  state->define = define;

  if (state->is_initial_state)
    {
      state->is_initial_state = FALSE;
      type = IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP;
    }
  else
    type = define->type;

  switch (type)
    {
    case IDE_XML_RNG_DEFINE_ATTRIBUTE:
      match_children = process_attribute (state);
      break;

    case IDE_XML_RNG_DEFINE_NOOP:
    case IDE_XML_RNG_DEFINE_NOTALLOWED:
    case IDE_XML_RNG_DEFINE_TEXT:
    case IDE_XML_RNG_DEFINE_DATATYPE:
    case IDE_XML_RNG_DEFINE_VALUE:
    case IDE_XML_RNG_DEFINE_EMPTY:
    case IDE_XML_RNG_DEFINE_ELEMENT:
    case IDE_XML_RNG_DEFINE_START:
    case IDE_XML_RNG_DEFINE_PARAM:
    case IDE_XML_RNG_DEFINE_EXCEPT:
    case IDE_XML_RNG_DEFINE_LIST:
      match_children = NULL;
      break;

    case IDE_XML_RNG_DEFINE_DEFINE:
    case IDE_XML_RNG_DEFINE_REF:
    case IDE_XML_RNG_DEFINE_PARENTREF:
    case IDE_XML_RNG_DEFINE_EXTERNALREF:
      match_children = process_matching_state (state, define->content);
      break;

    case IDE_XML_RNG_DEFINE_ZEROORMORE:
    case IDE_XML_RNG_DEFINE_ONEORMORE:
    case IDE_XML_RNG_DEFINE_OPTIONAL:
      old_optional = state->is_optional;
      if (define->is_mandatory)
        state->is_optional = FALSE;
      else if (type == IDE_XML_RNG_DEFINE_ZEROORMORE || type == IDE_XML_RNG_DEFINE_OPTIONAL)
        state->is_optional = TRUE;

      match_children = process_group (state);
      state->is_optional = old_optional;
      break;

    case IDE_XML_RNG_DEFINE_CHOICE:
      match_children = process_choice (state);
      break;

    case IDE_XML_RNG_DEFINE_INTERLEAVE:
    case IDE_XML_RNG_DEFINE_GROUP:
      match_children = process_group (state);
      break;

    case IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP:
      match_children = process_attributes_group (state);
      break;

    default:
      g_assert_not_reached ();
    }

  state->define = old_define;

  return match_children;
}

static MatchingState *
create_initial_matching_state (IdeXmlRngDefine  *define,
                               IdeXmlSymbolNode *node)
{
  MatchingState *state;
  gchar **attributes;

  g_assert (define != NULL);

  state = matching_state_new (define, node);
  if (node != NULL)
    {
      if (NULL != (attributes = ide_xml_symbol_node_get_attributes_names (node)))
        {
          for (gint i = 0; attributes [i] != NULL; i++)
            g_ptr_array_add (state->node_attr, (gchar *)attributes [i]);

          g_free (attributes);
        }
    }

  state->is_initial_state = TRUE;

  return state;
}

static gboolean
compare_attribute_names (gpointer a,
                         gpointer b)
{
  MatchItem *match;
  const gchar *attr_name;

  attr_name = (const gchar *)a;
  match = (MatchItem *)b;

  return ide_str_equal0 (match->name, attr_name);
}

static gint
alphabetical_sort_func (gpointer *a,
                        gpointer *b)
{
  MatchItem *match_a = (MatchItem *)*a;
  MatchItem *match_b = (MatchItem *)*b;

  return g_ascii_strcasecmp (match_b->name, match_a->name);
}

/* Remove completion items already in the current node and
 * filter the list by alphabetical order. */
static void
match_children_filter (GPtrArray *match_children,
                       GPtrArray *node_attributes)
{
  MatchItem *match;
  guint index;
  gint i = 0;

  g_assert (match_children != NULL);
  g_assert (node_attributes != NULL);

  while (i < match_children->len)
    {
      match = g_ptr_array_index (match_children, i);
      if (g_ptr_array_find_with_equal_func (node_attributes, match, (GEqualFunc)compare_attribute_names, &index))
        g_ptr_array_remove_index_fast (match_children, i);
      else
        ++i;
    }

  g_ptr_array_sort (match_children, (GCompareFunc)alphabetical_sort_func);
}

static void
propagate_mandatory (MatchingState *state)
{
  IdeXmlRngDefine *define = state->define;

  g_assert (state != NULL);

  while (define != state->orig_define)
    {
      if (define->type == IDE_XML_RNG_DEFINE_OPTIONAL || define->type == IDE_XML_RNG_DEFINE_ZEROORMORE)
        define->is_mandatory = TRUE;

      define = define->parent;
    }
}

static void
set_position (MatchingState *state)
{
  const gchar *attribute;
  guint index;

  g_assert (state != NULL);

  if (NULL == (attribute = (const gchar *)state->define->name) ||
      !g_ptr_array_find_with_equal_func (state->node_attr, attribute, (GEqualFunc)g_str_equal, &index))
    {
      index = -1;
    }
  else
    propagate_mandatory (state);

  state->define->pos = index;
}

static void
set_attributes_position (MatchingState   *state,
                         IdeXmlRngDefine *define)
{
  IdeXmlRngDefine *child_define;
  IdeXmlRngDefine *old_define;
  IdeXmlRngDefineType type;

  g_assert (state != NULL);
  g_assert (define != NULL);

  old_define = state->define;
  state->define = define;

  /* Reset the value from a previous completion */
  define->is_mandatory = FALSE;

  if (state->is_initial_state)
    {
      state->is_initial_state = FALSE;
      type = IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP;
    }
  else
    type = define->type;

  switch (type)
    {
    case IDE_XML_RNG_DEFINE_ATTRIBUTE:
      set_position (state);
      break;

    case IDE_XML_RNG_DEFINE_NOOP:
    case IDE_XML_RNG_DEFINE_NOTALLOWED:
    case IDE_XML_RNG_DEFINE_TEXT:
    case IDE_XML_RNG_DEFINE_DATATYPE:
    case IDE_XML_RNG_DEFINE_VALUE:
    case IDE_XML_RNG_DEFINE_EMPTY:
    case IDE_XML_RNG_DEFINE_ELEMENT:
    case IDE_XML_RNG_DEFINE_START:
    case IDE_XML_RNG_DEFINE_PARAM:
    case IDE_XML_RNG_DEFINE_EXCEPT:
    case IDE_XML_RNG_DEFINE_LIST:
      break;

    case IDE_XML_RNG_DEFINE_DEFINE:
    case IDE_XML_RNG_DEFINE_REF:
    case IDE_XML_RNG_DEFINE_PARENTREF:
    case IDE_XML_RNG_DEFINE_EXTERNALREF:
      set_attributes_position (state, define->content);
      break;

    case IDE_XML_RNG_DEFINE_INTERLEAVE:
    case IDE_XML_RNG_DEFINE_GROUP:
    case IDE_XML_RNG_DEFINE_CHOICE:
    case IDE_XML_RNG_DEFINE_ZEROORMORE:
    case IDE_XML_RNG_DEFINE_ONEORMORE:
    case IDE_XML_RNG_DEFINE_OPTIONAL:
      child_define = state->define->content;
      while (child_define != NULL)
        {
          set_attributes_position (state, child_define);
          child_define = child_define->next;
        }

      break;

    case IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP:
      child_define = state->define->attributes;
      while (child_define != NULL)
        {
          set_attributes_position (state, child_define);
          child_define = child_define->next;
        }

      break;

    default:
      g_assert_not_reached ();
    }

  state->define = old_define;
}

/* Return an array of MatchItem */
GPtrArray *
ide_xml_completion_attributes_get_matches (IdeXmlRngDefine  *define,
                                           IdeXmlSymbolNode *node,
                                           gboolean          filtered)
{
  MatchingState *initial_state;
  GPtrArray *match_children;

  g_return_val_if_fail (define != NULL, NULL);
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (node) || node == NULL, NULL);

  if (define->attributes == NULL)
    return NULL;

  initial_state = create_initial_matching_state (define, node);
  set_attributes_position (initial_state, define);

  initial_state->is_initial_state = TRUE;
  match_children = process_matching_state (initial_state, define);

  if (filtered)
    match_children_filter (match_children, initial_state->node_attr);

  matching_state_free (initial_state);
  return match_children;
}
