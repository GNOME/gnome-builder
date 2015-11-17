/* ide-back-forward-list.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-back-forward-list"

/*
 * TODO: This was originally written under the assumption you can "have a current item". After
 *       using things, I don't think this is what I want going forward.
 */

#include <glib/gi18n.h>

#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-file.h"
#include "ide-project.h"
#include "ide-source-location.h"

#define MAX_ITEMS_TOTAL 100

struct _IdeBackForwardList
{
  IdeObject           parent_instance;

  GQueue             *backward;
  IdeBackForwardItem *current_item;
  GQueue             *forward;
};


G_DEFINE_TYPE (IdeBackForwardList, ide_back_forward_list, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CAN_GO_BACKWARD,
  PROP_CAN_GO_FORWARD,
  PROP_CURRENT_ITEM,
  LAST_PROP
};

enum {
  NAVIGATE_TO,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint       signals [LAST_SIGNAL];

/**
 * ide_back_forward_list_get_current_item:
 *
 * Retrieves the current #IdeBackForwardItem or %NULL if no items have been
 * added to the #IdeBackForwardList.
 *
 * Returns: (transfer none) (nullable): An #IdeBackForwardItem or %NULL.
 */
IdeBackForwardItem *
ide_back_forward_list_get_current_item (IdeBackForwardList *self)
{
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), NULL);

  return self->current_item;
}

static void
ide_back_forward_list_navigate_to (IdeBackForwardList *self,
                                   IdeBackForwardItem *item)
{
  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));
  g_return_if_fail (IDE_IS_BACK_FORWARD_ITEM (item));

  g_signal_emit (self, signals [NAVIGATE_TO], 0, item);
}

void
ide_back_forward_list_go_backward (IdeBackForwardList *self)
{
  IdeBackForwardItem *current_item;

  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));

  current_item = g_queue_pop_head (self->backward);

  if (current_item)
    {
      if (self->current_item)
        g_queue_push_head (self->forward, self->current_item);

      self->current_item = current_item;
      ide_back_forward_list_navigate_to (self, self->current_item);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_GO_BACKWARD]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_GO_FORWARD]);
    }
  else
    g_warning ("Cannot go backward, no more items in queue.");
}

void
ide_back_forward_list_go_forward (IdeBackForwardList *self)
{
  IdeBackForwardItem *current_item;

  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));

  current_item = g_queue_pop_head (self->forward);

  if (current_item)
    {
      if (self->current_item)
        g_queue_push_head (self->backward, self->current_item);

      self->current_item = current_item;
      ide_back_forward_list_navigate_to (self, self->current_item);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_GO_BACKWARD]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_GO_FORWARD]);
    }
  else
    g_warning ("Cannot go forward, no more items in queue.");
}

gboolean
ide_back_forward_list_get_can_go_backward (IdeBackForwardList *self)
{
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), FALSE);

  return (self->backward->length > 0);
}

gboolean
ide_back_forward_list_get_can_go_forward (IdeBackForwardList *self)
{
  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), FALSE);

  return (self->forward->length > 0);
}

static void
ide_back_forward_list_prune (IdeBackForwardList *self)
{
  g_assert (IDE_IS_BACK_FORWARD_LIST (self));

  while (self->backward->length > MAX_ITEMS_TOTAL)
    {
      IdeBackForwardList *item;

      item = g_queue_pop_tail (self->backward);
      g_clear_object (&item);
    }
}

void
ide_back_forward_list_push (IdeBackForwardList *self,
                            IdeBackForwardItem *item)
{
  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));
  g_return_if_fail (IDE_IS_BACK_FORWARD_ITEM (item));

  /*
   * The following algorithm tries to loosely copy the design of jump lists
   * in Vim. If we are not all the way forward, we push all items back onto
   * the backward stack. We then push a duplicated "current_item" onto the
   * backward stack. After that, we place @item as the new current_item.
   * This allows us to jump back to our previous place easily, but not lose
   * the history from previously forward progress.
   */

  if (!self->current_item)
    {
      self->current_item = g_object_ref (item);

      g_return_if_fail (self->backward->length == 0);
      g_return_if_fail (self->forward->length == 0);

      return;
    }

  g_queue_push_head (self->backward, self->current_item);

  if (self->forward->length)
    {
      while (self->forward->length)
        g_queue_push_head (self->backward, g_queue_pop_head (self->forward));
      g_queue_push_head (self->backward, g_object_ref (self->current_item));
    }

  if (self->backward->head && ide_back_forward_item_chain (self->backward->head->data, item))
    self->current_item = g_queue_pop_head (self->backward);
  else
    self->current_item = g_object_ref (item);

  ide_back_forward_list_prune (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_GO_BACKWARD]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_GO_FORWARD]);

  g_return_if_fail (self->forward->length == 0);
}

/**
 * ide_back_forward_list_branch:
 *
 * Branches @self into a newly created #IdeBackForwardList.
 *
 * This can be used independently and then merged back into a global
 * #IdeBackForwardList. This can be useful in situations where you have
 * multiple sets of editors.
 *
 * Returns: (transfer full): An #IdeBackForwardList
 */
IdeBackForwardList *
ide_back_forward_list_branch (IdeBackForwardList *self)
{
  IdeBackForwardList *ret;
  IdeContext *context;
  GList *iter;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));

  ret = g_object_new (IDE_TYPE_BACK_FORWARD_LIST,
                      "context", context,
                      NULL);

  for (iter = self->backward->head; iter; iter = iter->next)
    {
      IdeBackForwardItem *item = iter->data;
      ide_back_forward_list_push (ret, item);
    }

  if (self->current_item)
    ide_back_forward_list_push (ret, self->current_item);

  for (iter = self->forward->head; iter; iter = iter->next)
    {
      IdeBackForwardItem *item = iter->data;
      ide_back_forward_list_push (ret, item);
    }

  return ret;
}

static GPtrArray *
ide_back_forward_list_to_array (IdeBackForwardList *self)
{
  GPtrArray *ret;
  GList *iter;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), NULL);

  ret = g_ptr_array_new ();

  for (iter = self->backward->tail; iter; iter = iter->prev)
    g_ptr_array_add (ret, iter->data);

  if (self->current_item)
    g_ptr_array_add (ret, self->current_item);

  for (iter = self->forward->head; iter; iter = iter->next)
    g_ptr_array_add (ret, iter->data);

  return ret;
}

void
ide_back_forward_list_merge (IdeBackForwardList *self,
                             IdeBackForwardList *branch)
{
  IdeBackForwardList *first;
  gboolean found = FALSE;
  GPtrArray *ar1;
  GPtrArray *ar2;
  gsize i = 0;
  gsize j;

  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));
  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (branch));

  /*
   * The merge process works by:
   *
   * 1) Convert both BackForwardLists to an array containing all elements.
   * 2) Find the common ancestor between the two lists.
   * 3) If there is no common ancestor, copy all elements to @self.
   * 4) If there was a common ancestor, work our way until the paths diverge.
   * 5) Add all remaining elements to @self.
   */

  ar1 = ide_back_forward_list_to_array (self);
  ar2 = ide_back_forward_list_to_array (branch);

  g_assert (ar1 != NULL);
  g_assert (ar2 != NULL);

  if (ar2->len == 0)
    return;

  first = g_ptr_array_index (ar2, 0);

  for (i = 0; i < ar1->len; i++)
    {
      IdeBackForwardList *current = g_ptr_array_index (ar1, i);

      if (current == first)
        {
          found = TRUE;
          break;
        }
    }

  if (!found)
    {
      for (i = 0; i < ar2->len; i++)
        {
          IdeBackForwardItem *current = g_ptr_array_index (ar2, i);
          ide_back_forward_list_push (self, current);
        }

      goto cleanup;
    }

  for (j = 0; (i < ar1->len) && (j < ar2->len); i++, j++)
    {
      IdeBackForwardList *item1 = g_ptr_array_index (ar1, i);
      IdeBackForwardList *item2 = g_ptr_array_index (ar2, j);

      if (item1 != item2)
        {
          gsize k;

          for (k = j; k < ar2->len; k++)
            {
              IdeBackForwardItem *current = g_ptr_array_index (ar2, k);
              ide_back_forward_list_push (self, current);
            }

          goto cleanup;
        }
    }

cleanup:
  g_ptr_array_unref (ar1);
  g_ptr_array_unref (ar2);
}

static void
ide_back_forward_list_dispose (GObject *object)
{
  IdeBackForwardList *self = (IdeBackForwardList *)object;
  IdeBackForwardItem *item;

  if (self->backward)
    {
      while ((item = g_queue_pop_head (self->backward)))
        g_object_unref (item);
      g_clear_pointer (&self->backward, g_queue_free);
    }

  if (self->forward)
    {
      while ((item = g_queue_pop_head (self->forward)))
        g_object_unref (item);
      g_clear_pointer (&self->forward, g_queue_free);
    }

  G_OBJECT_CLASS (ide_back_forward_list_parent_class)->dispose (object);
}

static void
ide_back_forward_list_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeBackForwardList *self = IDE_BACK_FORWARD_LIST (object);

  switch (prop_id)
    {
    case PROP_CAN_GO_BACKWARD:
      g_value_set_boolean (value, ide_back_forward_list_get_can_go_backward (self));
      break;

    case PROP_CAN_GO_FORWARD:
      g_value_set_boolean (value, ide_back_forward_list_get_can_go_forward (self));
      break;

    case PROP_CURRENT_ITEM:
      g_value_set_object (value, ide_back_forward_list_get_current_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_back_forward_list_class_init (IdeBackForwardListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_back_forward_list_dispose;
  object_class->get_property = ide_back_forward_list_get_property;

  properties [PROP_CAN_GO_BACKWARD] =
    g_param_spec_boolean ("can-go-backward",
                          "Can Go Backward",
                          "If there are more backward navigation items.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CAN_GO_FORWARD] =
    g_param_spec_boolean ("can-go-forward",
                          "Can Go Forward",
                          "If there are more forward navigation items.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CURRENT_ITEM] =
    g_param_spec_object ("current-item",
                         "Current Item",
                         "The current navigation item.",
                         IDE_TYPE_BACK_FORWARD_ITEM,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [NAVIGATE_TO] =
    g_signal_new ("navigate-to",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_BACK_FORWARD_ITEM);
}

static void
ide_back_forward_list_init (IdeBackForwardList *self)
{
  self->backward = g_queue_new ();
  self->forward = g_queue_new ();
}

void
_ide_back_forward_list_foreach (IdeBackForwardList *self,
                                GFunc               callback,
                                gpointer            user_data)
{
  GList *iter;

  g_assert (IDE_IS_BACK_FORWARD_LIST (self));
  g_assert (callback);

  for (iter = self->forward->tail; iter; iter = iter->prev)
    callback (iter->data, user_data);

  if (self->current_item)
    callback (self->current_item, user_data);

  for (iter = self->backward->head; iter; iter = iter->next)
    callback (iter->data, user_data);
}

static void
find_by_file (gpointer data,
              gpointer user_data)
{
  IdeBackForwardItem *item = data;
  IdeUri *uri;
  struct {
    GFile *file;
    IdeBackForwardItem *result;
  } *lookup = user_data;

  g_assert (lookup);
  g_assert (G_IS_FILE (lookup->file));
  g_assert (IDE_IS_BACK_FORWARD_ITEM (item));

  if (lookup->result)
    return;

  uri = ide_back_forward_item_get_uri (item);
  if (uri == NULL)
    return;

  if (ide_uri_is_file (uri, lookup->file))
    lookup->result = item;
}

/**
 * _ide_back_forward_list_find:
 * @self: A #IdeBackForwardList.
 * @file: The target #IdeFile
 *
 * This internal function will attempt to discover the most recent jump point for @file. It starts
 * from the most recent item and works backwards until the target file is found or the list is
 * exhausted.
 *
 * This is useful if you want to place the insert mark on the last used position within the buffer.
 *
 * Returns: (transfer none): An #IdeBackForwardItem or %NULL.
 */
IdeBackForwardItem *
_ide_back_forward_list_find (IdeBackForwardList *self,
                             IdeFile            *file)
{
  struct {
    GFile *file;
    IdeBackForwardItem *result;
  } lookup;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), NULL);
  g_return_val_if_fail (IDE_IS_FILE (file), NULL);

  lookup.file = ide_file_get_file (file);
  lookup.result = NULL;

  _ide_back_forward_list_foreach (self, find_by_file, &lookup);

  return lookup.result;
}
