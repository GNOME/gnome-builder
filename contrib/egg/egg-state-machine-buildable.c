/* egg-state-machine-buildable.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "egg-state-machine"

#include <errno.h>
#include <glib/gi18n.h>

#include "egg-state-machine.h"
#include "egg-state-machine-buildable.h"
#include "egg-state-machine-private.h"

typedef struct
{
  EggStateMachine *self;
  GtkBuilder      *builder;
  GQueue          *stack;
} StatesParserData;

typedef enum
{
  STACK_ITEM_OBJECT,
  STACK_ITEM_STATE,
  STACK_ITEM_PROPERTY,
} StackItemType;

typedef struct
{
  StackItemType type;
  union {
    struct {
      gchar  *id;
      GSList *classes;
      GSList *properties;
    } object;
    struct {
      gchar  *name;
      GSList *objects;
    } state;
    struct {
      gchar *name;
      gchar *bind_source;
      gchar *bind_property;
      gchar *text;
      GBindingFlags bind_flags;
    } property;
  } u;
} StackItem;

static GtkBuildableIface *egg_state_machine_parent_buildable;

static void
stack_item_free (StackItem *item)
{
  switch (item->type)
    {
    case STACK_ITEM_OBJECT:
      g_free (item->u.object.id);
      g_slist_free_full (item->u.object.classes, g_free);
      break;

    case STACK_ITEM_STATE:
      g_free (item->u.state.name);
      g_slist_free_full (item->u.state.objects, (GDestroyNotify)stack_item_free);
      break;

    case STACK_ITEM_PROPERTY:
      g_free (item->u.property.name);
      g_free (item->u.property.bind_source);
      g_free (item->u.property.bind_property);
      g_free (item->u.property.text);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  g_slice_free (StackItem, item);
}

static StackItem *
stack_item_new (StackItemType type)
{
  StackItem *item;

  item = g_slice_new0 (StackItem);
  item->type = type;

  return item;
}

static void
add_state (StatesParserData  *parser_data,
           StackItem         *item,
           GError           **error)
{
  GSList *iter;

  g_assert (parser_data != NULL);
  g_assert (item != NULL);
  g_assert (item->type == STACK_ITEM_STATE);

  for (iter = item->u.state.objects; iter; iter = iter->next)
    {
      StackItem *stack_obj = iter->data;
      GObject *object;
      GSList *prop_iter;
      GSList *style_iter;

      g_assert (stack_obj->type == STACK_ITEM_OBJECT);
      g_assert (stack_obj->u.object.id != NULL);

      object = gtk_builder_get_object (parser_data->builder, stack_obj->u.object.id);

      if (object == NULL)
        {
          g_critical ("Failed to locate object %s for binding.", stack_obj->u.object.id);
          continue;
        }

      if (GTK_IS_WIDGET (object))
        for (style_iter = stack_obj->u.object.classes; style_iter; style_iter = style_iter->next)
          egg_state_machine_add_style (parser_data->self,
                                       item->u.state.name,
                                       GTK_WIDGET (object),
                                       style_iter->data);

      for (prop_iter = stack_obj->u.object.properties; prop_iter; prop_iter = prop_iter->next)
        {
          StackItem *stack_prop = prop_iter->data;
          GObject *bind_source;

          g_assert (stack_prop->type == STACK_ITEM_PROPERTY);

          if ((stack_prop->u.property.bind_source != NULL) &&
              (stack_prop->u.property.bind_property != NULL) &&
              (bind_source = gtk_builder_get_object (parser_data->builder, stack_prop->u.property.bind_source)))
            {
              egg_state_machine_add_binding (parser_data->self,
                                             item->u.state.name,
                                             bind_source,
                                             stack_prop->u.property.bind_property,
                                             object,
                                             stack_prop->u.property.name,
                                             stack_prop->u.property.bind_flags);
            }
          else if (stack_prop->u.property.text != NULL)
            {
              GParamSpec *pspec;
              GValue value = { 0 };

              pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), stack_prop->u.property.name);

              if (pspec == NULL)
                {
                  g_set_error (error,
                               GTK_BUILDER_ERROR,
                               GTK_BUILDER_ERROR_INVALID_PROPERTY,
                               "No such property: %s",
                               stack_prop->u.property.name);
                  return;
                }

              if (g_type_is_a (pspec->value_type, G_TYPE_OBJECT))
                {
                  GObject *relative;

                  relative = gtk_builder_get_object (parser_data->builder, stack_prop->u.property.text);
                  g_value_init (&value, pspec->value_type);
                  g_value_set_object (&value, relative);
                }
              else if (!gtk_builder_value_from_string (parser_data->builder,
                                                       pspec,
                                                       stack_prop->u.property.text,
                                                       &value,
                                                       error))
                {
                  return;
                }

              egg_state_machine_add_property (parser_data->self,
                                              item->u.state.name,
                                              object,
                                              stack_prop->u.property.name,
                                              &value);

              g_value_unset (&value);
            }
        }
    }
}

static void
add_object (StatesParserData *parser_data,
            StackItem        *parent,
            StackItem        *item)
{
  g_assert (parser_data != NULL);
  g_assert (parent != NULL);
  g_assert (parent->type == STACK_ITEM_STATE);
  g_assert (item != NULL);
  g_assert (item->type == STACK_ITEM_OBJECT);

  parent->u.state.objects = g_slist_prepend (parent->u.state.objects, item);
}

static void
add_property (StatesParserData *parser_data,
              StackItem        *parent,
              StackItem        *item)
{
  g_assert (parser_data != NULL);
  g_assert (parent != NULL);
  g_assert (parent->type == STACK_ITEM_OBJECT);
  g_assert (item != NULL);
  g_assert (item->type == STACK_ITEM_PROPERTY);

  parent->u.object.properties = g_slist_prepend (parent->u.object.properties, item);
}

static gboolean
check_parent (GMarkupParseContext  *context,
              const gchar          *element_name,
              GError              **error)
{
  const GSList *stack;
  const gchar *parent_name;
  const gchar *our_name;

  stack = g_markup_parse_context_get_element_stack (context);
  our_name = stack->data;
  parent_name = stack->next ? stack->next->data : "";

  if (g_strcmp0 (parent_name, element_name) != 0)
    {
      gint line;
      gint col;

      g_markup_parse_context_get_position (context, &line, &col);
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_TAG,
                   "%d:%d: Element <%s> found in <%s>, expected <%s>.",
                   line, col, our_name, parent_name, element_name);
      return FALSE;
    }

  return TRUE;
}

/*
 * flags_from_string:
 *
 * gtkbuilder.c
 *
 * Copyright (C) 1998-2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2006-2007 Async Open Source,
 *                         Johan Dahlin <jdahlin@async.com.br>,
 *                         Henrique Romano <henrique@async.com.br>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
gboolean
flags_from_string (GType         type,
                   const gchar  *string,
                   guint        *flags_value,
                   GError      **error)
{
  GFlagsClass *fclass;
  gchar *endptr, *prevptr;
  guint i, j, value;
  gchar *flagstr;
  GFlagsValue *fv;
  const gchar *flag;
  gunichar ch;
  gboolean eos, ret;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (type), FALSE);
  g_return_val_if_fail (string != 0, FALSE);

  ret = TRUE;

  endptr = NULL;
  errno = 0;
  value = g_ascii_strtoull (string, &endptr, 0);
  if (errno == 0 && endptr != string) /* parsed a number */
    *flags_value = value;
  else
    {
      fclass = g_type_class_ref (type);

      flagstr = g_strdup (string);
      for (value = i = j = 0; ; i++)
        {

          eos = flagstr[i] == '\0';

          if (!eos && flagstr[i] != '|')
            continue;

          flag = &flagstr[j];
          endptr = &flagstr[i];

          if (!eos)
            {
              flagstr[i++] = '\0';
              j = i;
            }

          /* trim spaces */
          for (;;)
            {
              ch = g_utf8_get_char (flag);
              if (!g_unichar_isspace (ch))
                break;
              flag = g_utf8_next_char (flag);
            }

          while (endptr > flag)
            {
              prevptr = g_utf8_prev_char (endptr);
              ch = g_utf8_get_char (prevptr);
              if (!g_unichar_isspace (ch))
                break;
              endptr = prevptr;
            }

          if (endptr > flag)
            {
              *endptr = '\0';
              fv = g_flags_get_value_by_name (fclass, flag);

              if (!fv)
                fv = g_flags_get_value_by_nick (fclass, flag);

              if (fv)
                value |= fv->value;
              else
                {
                  g_set_error (error,
                               GTK_BUILDER_ERROR,
                               GTK_BUILDER_ERROR_INVALID_VALUE,
                               "Unknown flag: `%s'",
                               flag);
                  ret = FALSE;
                  break;
                }
            }

          if (eos)
            {
              *flags_value = value;
              break;
            }
        }

      g_free (flagstr);

      g_type_class_unref (fclass);
    }

  return ret;
}

static void
states_parser_start_element (GMarkupParseContext  *context,
                             const gchar          *element_name,
                             const gchar         **attribute_names,
                             const gchar         **attribute_values,
                             gpointer              user_data,
                             GError              **error)
{
  StatesParserData *parser_data = user_data;
  StackItem *item;

  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (parser_data != NULL);

  if (g_strcmp0 (element_name, "state") == 0)
    {
      const gchar *name;

      if (!check_parent (context, "states", error))
        return;

      if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      item = stack_item_new (STACK_ITEM_STATE);
      item->u.state.name = g_strdup (name);
      g_queue_push_head (parser_data->stack, item);
    }
  else if (g_strcmp0 (element_name, "states") == 0)
    {
      if (!check_parent (context, "object", error))
        return;
    }
  else if (g_strcmp0 (element_name, "object") == 0)
    {
      const gchar *id;

      if (!check_parent (context, "state", error))
        return;

      if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_STRING, "id", &id,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      item = stack_item_new (STACK_ITEM_OBJECT);
      item->u.object.id = g_strdup (id);
      g_queue_push_head (parser_data->stack, item);
    }
  else if (g_strcmp0 (element_name, "property") == 0)
    {
      const gchar *name = NULL;
      const gchar *bind_source = NULL;
      const gchar *bind_property = NULL;
      const gchar *bind_flags_str = NULL;
      GBindingFlags bind_flags = 0;

      if (!check_parent (context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "bind-source", &bind_source,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "bind-property", &bind_property,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "bind-flags", &bind_flags_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      if ((bind_flags_str != NULL) && !flags_from_string (G_TYPE_BINDING_FLAGS, bind_flags_str, &bind_flags, error))
        return;

      item = stack_item_new (STACK_ITEM_PROPERTY);
      item->u.property.name = g_strdup (name);
      item->u.property.bind_source = g_strdup (bind_source);
      item->u.property.bind_property = g_strdup (bind_property);
      item->u.property.bind_flags = bind_flags;
      g_queue_push_head (parser_data->stack, item);
    }
  else if (g_strcmp0 (element_name, "style") == 0)
    {
      if (!check_parent (context, "object", error))
        return;
    }
  else if (g_strcmp0 (element_name, "class") == 0)
    {
      const gchar *name = NULL;

      if (!check_parent (context, "style", error))
        return;

      if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      item = g_queue_peek_head (parser_data->stack);
      g_assert (item->type == STACK_ITEM_OBJECT);

      item->u.object.classes = g_slist_prepend (item->u.object.classes, g_strdup (name));
    }
  else
    {
      const GSList *stack;
      const gchar *parent_name;
      const gchar *our_name;
      gint line;
      gint col;

      stack = g_markup_parse_context_get_element_stack (context);
      our_name = stack->data;
      parent_name = stack->next ? stack->next->data : "";

      g_markup_parse_context_get_position (context, &line, &col);
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_TAG,
                   "%d:%d: Unknown element <%s> found in <%s>.",
                   line, col, our_name, parent_name);
    }

  return;
}

static void
states_parser_end_element (GMarkupParseContext  *context,
                           const gchar          *element_name,
                           gpointer              user_data,
                           GError              **error)
{
  StatesParserData *parser_data = user_data;
  StackItem *item;

  g_assert (context != NULL);
  g_assert (element_name != NULL);
  g_assert (parser_data != NULL);

  if (g_strcmp0 (element_name, "state") == 0)
    {
      item = g_queue_pop_head (parser_data->stack);
      g_assert (item->type == STACK_ITEM_STATE);
      add_state (parser_data, item, error);
      stack_item_free (item);
    }
  else if (g_strcmp0 (element_name, "object") == 0)
    {
      StackItem *parent;

      item = g_queue_pop_head (parser_data->stack);
      g_assert (item->type == STACK_ITEM_OBJECT);

      parent = g_queue_peek_head (parser_data->stack);
      g_assert (parent->type == STACK_ITEM_STATE);

      add_object (parser_data, parent, item);
    }
  else if (g_strcmp0 (element_name, "property") == 0)
    {
      StackItem *parent;

      item = g_queue_pop_head (parser_data->stack);
      g_assert (item->type == STACK_ITEM_PROPERTY);

      parent = g_queue_peek_head (parser_data->stack);
      g_assert (parent->type == STACK_ITEM_OBJECT);

      add_property (parser_data, parent, item);
    }
}

static void
states_parser_text (GMarkupParseContext  *context,
                    const gchar          *text,
                    gsize                 text_len,
                    gpointer              user_data,
                    GError              **error)
{
  StatesParserData *parser_data = user_data;
  StackItem *item;

  g_assert (parser_data != NULL);

  item = g_queue_peek_head (parser_data->stack);
  if ((item != NULL) && (item->type == STACK_ITEM_PROPERTY))
    item->u.property.text = g_strndup (text, text_len);
}

static GMarkupParser StatesParser = {
  states_parser_start_element,
  states_parser_end_element,
  states_parser_text,
};

static gboolean
egg_state_machine_buildable_custom_tag_start (GtkBuildable  *buildable,
                                              GtkBuilder    *builder,
                                              GObject       *child,
                                              const gchar   *tagname,
                                              GMarkupParser *parser,
                                              gpointer      *data)
{
  EggStateMachine *self = (EggStateMachine *)buildable;

  g_assert (EGG_IS_STATE_MACHINE (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (tagname != NULL);
  g_assert (parser != NULL);
  g_assert (data != NULL);

  if (g_strcmp0 (tagname, "states") == 0)
    {
      StatesParserData *parser_data;

      parser_data = g_slice_new0 (StatesParserData);
      parser_data->self = g_object_ref (buildable);
      parser_data->builder = g_object_ref (builder);
      parser_data->stack = g_queue_new ();

      egg_state_machine_freeze (self);

      *parser = StatesParser;
      *data = parser_data;

      return TRUE;
    }

  return FALSE;
}

static void
egg_state_machine_buildable_custom_finished (GtkBuildable *buildable,
                                             GtkBuilder   *builder,
                                             GObject      *child,
                                             const gchar  *tagname,
                                             gpointer      user_data)
{
  EggStateMachine *self = (EggStateMachine *)buildable;

  g_assert (EGG_IS_STATE_MACHINE (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (tagname != NULL);

  if (g_strcmp0 (tagname, "states") == 0)
    {
      StatesParserData *parser_data = user_data;
      gchar *state;

      g_object_unref (parser_data->self);
      g_object_unref (parser_data->builder);
      g_queue_free_full (parser_data->stack, (GDestroyNotify)stack_item_free);
      g_slice_free (StatesParserData, parser_data);

      egg_state_machine_thaw (self);

      /* XXX: reapply current state */
      state = g_strdup (egg_state_machine_get_state (self));
      egg_state_machine_set_state (self, NULL);
      egg_state_machine_set_state (self, state);
      g_free (state);
    }
}

void
egg_state_machine_buildable_iface_init (GtkBuildableIface *iface)
{
  g_assert (iface != NULL);

  egg_state_machine_parent_buildable = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = egg_state_machine_buildable_custom_tag_start;
  iface->custom_finished = egg_state_machine_buildable_custom_finished;
}
