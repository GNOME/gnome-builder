/* ide-macros.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_CORE_INSIDE) && !defined (IDE_CORE_COMPILATION)
# error "Only <libide-core.h> can be included directly."
#endif

#ifndef __GI_SCANNER__

#include <glib.h>

#include "ide-global.h"
#include "ide-object.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define ide_str_empty0(str)       (!(str) || !*(str))
#define ide_str_equal(str1,str2)  (strcmp((char*)str1,(char*)str2)==0)
#define ide_str_equal0(str1,str2) (g_strcmp0((char*)str1,(char*)str2)==0)
#define ide_strv_empty0(strv)     (((strv) == NULL) || ((strv)[0] == NULL))

#define ide_clear_param(pptr, pval) \
  G_STMT_START { if (pptr) { *(pptr) = pval; }; } G_STMT_END

#define IDE_IS_MAIN_THREAD() (g_thread_self() == ide_get_main_thread())

#define IDE_PTR_ARRAY_CLEAR_FREE_FUNC(ar)                       \
  IDE_PTR_ARRAY_SET_FREE_FUNC(ar, NULL)
#define IDE_PTR_ARRAY_SET_FREE_FUNC(ar, func)                   \
  G_STMT_START {                                                \
    if ((ar) != NULL)                                           \
      g_ptr_array_set_free_func ((ar), (GDestroyNotify)(func)); \
  } G_STMT_END
#define IDE_PTR_ARRAY_STEAL_FULL(arptr)        \
  ({ IDE_PTR_ARRAY_CLEAR_FREE_FUNC (*(arptr)); \
     g_steal_pointer ((arptr)); })

static inline gboolean
ide_set_strv (char               ***dest,
              const char * const   *src)
{
  if ((const char * const *)*dest == src)
    return FALSE;

  if (*dest == NULL ||
      src == NULL ||
      !g_strv_equal ((const char * const *)*dest, src))
    {
      char **copy = g_strdupv ((char **)src);
      g_strfreev (*dest);
      *dest = copy;
      return TRUE;
    }

  return FALSE;
}

static inline gpointer
ide_ptr_array_steal_index (GPtrArray      *array,
                           guint           index,
                           GDestroyNotify  free_func)
{
  gpointer ret;

  if (index >= array->len)
    return NULL;

  g_ptr_array_set_free_func (array, NULL);
  ret = g_ptr_array_index (array, index);
  g_ptr_array_remove_index (array, index);
  g_ptr_array_set_free_func (array, free_func);

  return ret;
}

static inline gboolean
ide_error_ignore (const GError *error)
{
  return g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
         g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
}

static inline void
_g_object_unref0 (gpointer instance)
{
  if (instance)
    g_object_unref (instance);
}

static inline void
ide_take_string (char **ptr,
                 char  *str)
{
  if (*ptr == NULL && str == NULL)
    return;
  g_free (*ptr);
  *ptr = str;
}

static inline void
ide_clear_string (char **ptr)
{
  g_clear_pointer (ptr, g_free);
}

static inline GList *
_g_list_insert_before_link (GList *list,
                            GList *sibling,
                            GList *link_)
{
  g_return_val_if_fail (link_ != NULL, list);

  if (!list)
    {
      g_return_val_if_fail (sibling == NULL, list);
      return link_;
    }
  else if (sibling)
    {
      link_->prev = sibling->prev;
      link_->next = sibling;
      sibling->prev = link_;
      if (link_->prev)
        {
          link_->prev->next = link_;
          return list;
        }
      else
        {
          g_return_val_if_fail (sibling == list, link_);
          return link_;
        }
    }
  else
    {
      GList *last;

      last = list;
      while (last->next)
        last = last->next;

      last->next = link_;
      last->next->prev = last;
      last->next->next = NULL;

      return list;
    }
}

static inline void
_g_queue_insert_before_link (GQueue *queue,
                             GList  *sibling,
                             GList  *link_)
{
  g_return_if_fail (queue != NULL);
  g_return_if_fail (link_ != NULL);

  if G_UNLIKELY (sibling == NULL)
    {
      /* We don't use g_list_insert_before_link() with a NULL sibling because it
       * would be a O(n) operation and we would need to update manually the tail
       * pointer.
       */
      g_queue_push_tail_link (queue, link_);
    }
  else
    {
      queue->head = _g_list_insert_before_link (queue->head, sibling, link_);
      queue->length++;
    }
}

static inline void
_g_queue_insert_after_link (GQueue *queue,
                            GList  *sibling,
                            GList  *link_)
{
  g_return_if_fail (queue != NULL);
  g_return_if_fail (link_ != NULL);

  if (sibling == NULL)
    g_queue_push_head_link (queue, link_);
  else
    _g_queue_insert_before_link (queue, sibling->next, link_);
}

static inline GPtrArray *
_g_ptr_array_copy_objects (GPtrArray *ar)
{
  if (ar != NULL)
    {
      GPtrArray *copy = g_ptr_array_new_full (ar->len, g_object_unref);
      for (guint i = 0; i < ar->len; i++)
        g_ptr_array_add (copy, g_object_ref (g_ptr_array_index (ar, i)));
      return g_steal_pointer (&copy);
    }

  return NULL;
}

static void
ide_object_unref_and_destroy (IdeObject *object)
{
  if (object != NULL)
    {
      if (!ide_object_in_destruction (object))
        ide_object_destroy (object);
      g_object_unref (object);
    }
}

typedef GPtrArray IdeObjectArray;

static inline void
ide_clear_and_destroy_object (gpointer pptr)
{
  IdeObject **ptr = pptr;

  if (ptr && *ptr)
    {
      if (!ide_object_in_destruction (*ptr))
        ide_object_destroy (*ptr);
      g_clear_object (ptr);
    }
}

static inline GPtrArray *
ide_object_array_new (void)
{
  return g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);
}

static inline gpointer
ide_object_array_steal_index (IdeObjectArray *array,
                              guint           position)
{
  gpointer ret = g_ptr_array_index (array, position);
  g_ptr_array_index (array, position) = NULL;
  g_ptr_array_remove_index (array, position);
  return ret;
}

static inline gpointer
ide_object_array_index (IdeObjectArray *array,
                        guint           position)
{
  return g_ptr_array_index (array, position);
}

static inline void
ide_object_array_add (IdeObjectArray *ar,
                      gpointer        instance)
{
  g_ptr_array_add (ar, g_object_ref (IDE_OBJECT (instance)));
}

static inline void
ide_object_array_unref (IdeObjectArray *ar)
{
  g_ptr_array_unref (ar);
}

#define IDE_OBJECT_ARRAY_STEAL_FULL(ar) IDE_PTR_ARRAY_STEAL_FULL(ar)

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeObjectArray, g_ptr_array_unref)

#define IDE_STRV_INIT(...) ((const char * const[]) { __VA_ARGS__, NULL})

static inline int
ide_strv_qsort_compare_element (const char * const *a,
                                const char * const *b,
                                gpointer            sort_data)
{
  return strcmp (*a, *b);
}

static inline void
ide_strv_sort (char   **strv,
               gssize   len)
{
  if (len < 0)
    len = g_strv_length (strv);
  g_qsort_with_data (strv, len, sizeof (char*),
                     (GCompareDataFunc)ide_strv_qsort_compare_element,
                     NULL);
}

static inline gboolean
ide_strv_add_to_set (char ***strv,
                     char   *value)
{
  gsize len;

  if (value == NULL)
    return FALSE;

  if (*strv != NULL && g_strv_contains ((const char * const *)*strv, value))
    {
      g_free (value);
      return FALSE;
    }

  len = *strv ? g_strv_length (*strv) : 0;
  *strv = g_renew (char *, *strv, len + 2);
  (*strv)[len++] = value;
  (*strv)[len] = NULL;

  return TRUE;
}

static inline gboolean
ide_strv_remove_from_set (char       **strv,
                          const char  *value)
{
  if (value == NULL)
    return FALSE;

  if (strv == NULL)
    return FALSE;

  for (guint i = 0; strv[i]; i++)
    {
      if (strcmp (value, strv[i]) == 0)
        {
          g_free (strv[i]);

          while (TRUE)
            {
              strv[i] = strv[i+1];
              if (strv[i] == NULL)
                return TRUE;
              i++;
            }
        }
    }

  return FALSE;
}

static inline int
ide_steal_fd (int *fd)
{
  int ret = *fd;
  *fd = -1;
  return ret;
}

static inline gboolean
_g_signal_group_get_target_cb (gpointer data)
{
  return G_SOURCE_REMOVE;
}

static inline gpointer
_g_signal_group_get_target (GSignalGroup *sg)
{
  gpointer ret = g_signal_group_dup_target (sg);
  if (ret != NULL)
    g_idle_add_full (G_PRIORITY_HIGH_IDLE, _g_signal_group_get_target_cb, ret, g_object_unref);
  return ret;
}

#define IDE_DEFINE_BOOLEAN_PROPERTY(name, value, flags, PROP)               \
  properties[PROP_##PROP] =                                                 \
    g_param_spec_boolean(name, NULL, NULL, value,                           \
        (flags|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_STATIC_STRINGS));
#define IDE_DEFINE_BOOLEAN_GETTER(symbol, Symbol, TYPE, name)               \
gboolean symbol##_get_##name (Symbol *self)                                 \
{                                                                           \
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, TYPE), FALSE);    \
  return self->name;                                                        \
}
#define IDE_DEFINE_BOOLEAN_SETTER(symbol, Symbol, TYPE, name, PROP)         \
void symbol##_set_##name (Symbol *self, gboolean name)                      \
{                                                                           \
  g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, TYPE));               \
  name = !!name;                                                            \
  if (name != self->name)                                                   \
    {                                                                       \
      self->name = name;                                                    \
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_##PROP]); \
    }                                                                       \
}
#define IDE_DEFINE_BOOLEAN_GETTER_PRIVATE(symbol, Symbol, TYPE, name)       \
gboolean symbol##_get_##name (Symbol *self)                                 \
{                                                                           \
  Symbol##Private *priv = symbol##_get_instance_private(self);              \
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, TYPE), FALSE);    \
  return priv->name;                                                        \
}
#define IDE_DEFINE_BOOLEAN_SETTER_PRIVATE(symbol, Symbol, TYPE, name, PROP) \
void symbol##_set_##name (Symbol *self, gboolean name)                      \
{                                                                           \
  Symbol##Private *priv = symbol##_get_instance_private(self);              \
  g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, TYPE));               \
  name = !!name;                                                            \
  if (name != priv->name)                                                   \
    {                                                                       \
      priv->name = name;                                                    \
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_##PROP]); \
    }                                                                       \
}
#define IDE_GET_PROPERTY_BOOLEAN(symbol, name, PROP)                        \
  case PROP_##PROP:                                                         \
    g_value_set_boolean (value, symbol##_get_##name (self));                \
    break
#define IDE_SET_PROPERTY_BOOLEAN(symbol, name, PROP)                        \
  case PROP_##PROP:                                                         \
    symbol##_set_##name (self, g_value_get_boolean (value));                \
    break

#define IDE_DEFINE_STRING_PROPERTY(name, value, flags, PROP)                \
  properties[PROP_##PROP] =                                                 \
    g_param_spec_string(name, NULL, NULL, value,                            \
        (flags|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_STATIC_STRINGS));
#define IDE_DEFINE_STRING_GETTER(symbol, Symbol, TYPE, name)                \
const char *symbol##_get_##name (Symbol *self)                              \
{                                                                           \
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, TYPE), NULL);     \
  return self->name;                                                        \
}
#define IDE_DEFINE_STRING_SETTER(symbol, Symbol, TYPE, name, PROP)          \
void symbol##_set_##name (Symbol *self, const char *name)                   \
{                                                                           \
  g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, TYPE));               \
  if (g_set_str (&self->name, name))                                   \
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_##PROP]);   \
}
#define IDE_DEFINE_STRING_GETTER_PRIVATE(symbol, Symbol, TYPE, name)        \
const char *symbol##_get_##name (Symbol *self)                              \
{                                                                           \
  Symbol##Private *priv = symbol##_get_instance_private(self);              \
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, TYPE), NULL);     \
  return priv->name;                                                        \
}
#define IDE_DEFINE_STRING_SETTER_PRIVATE(symbol, Symbol, TYPE, name, PROP)  \
void symbol##_set_##name (Symbol *self, const char *name)                   \
{                                                                           \
  Symbol##Private *priv = symbol##_get_instance_private(self);              \
  g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, TYPE));               \
  if (g_set_str (&priv->name, name))                                   \
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_##PROP]);   \
}
#define IDE_GET_PROPERTY_STRING(symbol, name, PROP)                         \
  case PROP_##PROP:                                                         \
    g_value_set_string (value, symbol##_get_##name (self));                 \
    break
#define IDE_SET_PROPERTY_STRING(symbol, name, PROP)                         \
  case PROP_##PROP:                                                         \
    symbol##_set_##name (self, g_value_get_string (value));                 \
    break

G_END_DECLS

#endif /* __GI_SCANNER__ */
