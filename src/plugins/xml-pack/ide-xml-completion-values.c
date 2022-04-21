/* ide-xml-completion-values.c
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

#include "ide-xml-completion-values.h"
#include "ide-xml-position.h"

typedef struct
{
  IdeXmlRngDefine *define;
  gchar           *values;
  gchar           *prefix;

  guint            is_initial_state : 1;
} MatchingState;

static GPtrArray * process_matching_state (MatchingState   *state,
                                           IdeXmlRngDefine *define);

static ValueMatchItem *
value_match_item_new (const gchar *value)
{
  ValueMatchItem *item;

  g_assert (!ide_str_empty0 (value));

  item = g_slice_new0 (ValueMatchItem);
  item->name = g_strdup (value);

  return item;
}

static void
value_match_item_free (gpointer data)
{
  ValueMatchItem *item = (ValueMatchItem *)data;

  g_clear_pointer (&item->name, g_free);
  g_slice_free (ValueMatchItem, item);
}

static GPtrArray *
match_values_new (void)
{
  GPtrArray *ar;

  ar = g_ptr_array_new_with_free_func ((GDestroyNotify)value_match_item_free);
  return ar;
}

static void
match_values_add (GPtrArray *to_values,
                  GPtrArray *from_values)
{
  ValueMatchItem *to_item;
  ValueMatchItem *from_item;

  g_assert (to_values != NULL);

  if (from_values == NULL)
    return;

  for (gint i = 0; i < from_values->len; ++i)
    {
      from_item = g_ptr_array_index (from_values, i);
      to_item = value_match_item_new (from_item->name);
      g_ptr_array_add (to_values, to_item);
    }
}

static MatchingState *
matching_state_new (IdeXmlRngDefine  *define,
                    const gchar      *values,
                    const gchar      *prefix)
{
  MatchingState *state;

  g_assert (define != NULL);

  state = g_slice_new0 (MatchingState);

  state->define = define;
  state->values = g_strdup (values);
  state->prefix = g_strdup (prefix);

  state->is_initial_state = FALSE;

  return state;
}

static void
matching_state_free (MatchingState *state)
{
  g_clear_pointer (&state->values, g_free);
  g_clear_pointer (&state->prefix, g_free);
  g_slice_free (MatchingState, state);
}

static GPtrArray *
process_value (MatchingState *state)
{
  GPtrArray *match_values = NULL;
  ValueMatchItem *item;
  const gchar *value;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_VALUE);

  value = (gchar *)state->define->name;

  if (!ide_str_empty0 (value) &&
      (ide_str_empty0 (state->prefix) || g_str_has_prefix (value, state->prefix)))
    {
      match_values = match_values_new ();
      item = value_match_item_new (value);
      g_ptr_array_add (match_values, item);
    }

  return match_values;
}

static GPtrArray *
process_choice (MatchingState *state)
{
  GPtrArray *match_values = NULL;
  IdeXmlRngDefine *defines;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_CHOICE);

  if (NULL != (defines = state->define->content))
    {
      match_values = match_values_new ();
      while (defines != NULL)
        {
          g_autoptr (GPtrArray) match = NULL;

          if (NULL != (match = process_matching_state (state, defines)))
            {
              /* TODO: use move */
              match_values_add (match_values, match);
            }

          defines = defines->next;
        }
    }

  return match_values;
}

static GPtrArray *
process_group (MatchingState *state)
{
  GPtrArray *match_values = NULL;
  IdeXmlRngDefine *defines;

  g_assert (state->define->type == IDE_XML_RNG_DEFINE_GROUP ||
            state->define->type == IDE_XML_RNG_DEFINE_ATTRIBUTE ||
            state->define->type == IDE_XML_RNG_DEFINE_ZEROORMORE ||
            state->define->type == IDE_XML_RNG_DEFINE_ONEORMORE ||
            state->define->type == IDE_XML_RNG_DEFINE_OPTIONAL);

  if (NULL != (defines = state->define->content))
    {
      while (defines != NULL)
        {
          g_autoptr (GPtrArray) match = NULL;

          match_values = match_values_new ();
          if (NULL != (match = process_matching_state (state, defines)))
            {
              match_values_add (match_values, match);
            }

          defines = defines->next;
        }
    }

  return match_values;
}

static GPtrArray *
process_matching_state (MatchingState   *state,
                        IdeXmlRngDefine *define)
{
  IdeXmlRngDefine *old_define;
  IdeXmlRngDefineType type;
  GPtrArray *match_values = NULL;

  g_assert (state != NULL);
  g_assert (define != NULL);

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
    case IDE_XML_RNG_DEFINE_VALUE:
      match_values = process_value (state);
      break;

    case IDE_XML_RNG_DEFINE_ATTRIBUTE:
    case IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP:
    case IDE_XML_RNG_DEFINE_NOOP:
    case IDE_XML_RNG_DEFINE_NOTALLOWED:
    case IDE_XML_RNG_DEFINE_TEXT:
    case IDE_XML_RNG_DEFINE_DATATYPE:
    case IDE_XML_RNG_DEFINE_EMPTY:
    case IDE_XML_RNG_DEFINE_ELEMENT:
    case IDE_XML_RNG_DEFINE_START:
    case IDE_XML_RNG_DEFINE_PARAM:
    case IDE_XML_RNG_DEFINE_EXCEPT:
    case IDE_XML_RNG_DEFINE_LIST:
      match_values = NULL;
      break;

    case IDE_XML_RNG_DEFINE_DEFINE:
    case IDE_XML_RNG_DEFINE_REF:
    case IDE_XML_RNG_DEFINE_PARENTREF:
    case IDE_XML_RNG_DEFINE_EXTERNALREF:
      match_values = process_matching_state (state, define->content);
      break;

    case IDE_XML_RNG_DEFINE_INTERLEAVE:
    case IDE_XML_RNG_DEFINE_GROUP:
    case IDE_XML_RNG_DEFINE_ZEROORMORE:
    case IDE_XML_RNG_DEFINE_ONEORMORE:
    case IDE_XML_RNG_DEFINE_OPTIONAL:
      match_values = process_group (state);
      break;

    case IDE_XML_RNG_DEFINE_CHOICE:
      match_values = process_choice (state);
      break;

    default:
      g_assert_not_reached ();
    }

  state->define = old_define;

  return match_values;
}

static MatchingState *
create_initial_matching_state (IdeXmlRngDefine  *define,
                               const gchar      *values,
                               const gchar      *prefix)
{
  MatchingState *state;

  g_assert (define != NULL);

  state = matching_state_new (define, values, prefix);
  state->is_initial_state = TRUE;

  return state;
}

/* Return an array of ValueMatchItem */
GPtrArray *
ide_xml_completion_values_get_matches (IdeXmlRngDefine *define,
                                       const gchar     *values,
                                       const gchar     *prefix)
{
  MatchingState *initial_state;
  GPtrArray *match_values = NULL;

  g_return_val_if_fail (define != NULL, NULL);

  if (define->content != NULL)
    {
      initial_state = create_initial_matching_state (define, values, prefix);

      initial_state->is_initial_state = TRUE;
      match_values = process_matching_state (initial_state, define);
      matching_state_free (initial_state);
    }

  return match_values;
}
