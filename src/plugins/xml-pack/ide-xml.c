/* ide-xml.c
 *
 * Copyright 2015 Dimitris Zenios <dimitris.zenios@gmail.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ide-xml.h"

typedef gboolean (*iter_func) (GtkTextIter *iter);


static gboolean
find_end_element_char (gunichar ch,
                       gpointer user_data)
{
  return g_unichar_isspace (ch) || ch == '/' || ch == '>';
}

static gboolean
find_char (iter_func          func,
           const GtkTextIter *iter,
           GtkTextIter       *curr,
           gunichar           ch)
{
  GtkTextIter copy = *iter;

  do {
    gunichar curr_ch;

    curr_ch = gtk_text_iter_get_char (&copy);

    if (curr_ch == ch)
      {
        *curr = copy;
        return TRUE;
      }
  } while (func (&copy));

  return FALSE;
}

gboolean
ide_xml_in_element (const GtkTextIter *iter)
{
  GtkTextIter copy = *iter;

  g_return_val_if_fail (iter != NULL, FALSE);

  do {
    gunichar ch;

    ch = gtk_text_iter_get_char (&copy);

    if (ch == '/')
      {
        gtk_text_iter_backward_char (&copy);
        ch = gtk_text_iter_get_char (&copy);
        if (ch == '<')
          return TRUE;
      }

    /*
     * If the iter char points to end of the element '>'
     * we are still inside the element.This is the reason
     * we check for equality of the copy and iter
     */
    if (ch == '>' && !gtk_text_iter_equal (&copy, iter))
      return FALSE;
    else if (ch == '<')
      return TRUE;
  } while (gtk_text_iter_backward_char (&copy));

  return FALSE;
}

gboolean
ide_xml_get_current_element (const GtkTextIter *iter,
                             GtkTextIter       *start,
                             GtkTextIter       *end)
{
  g_return_val_if_fail (ide_xml_in_element (iter), FALSE);
  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (end != NULL, FALSE);


  if(find_char (gtk_text_iter_backward_char, iter, start, '<') &&
     find_char (gtk_text_iter_forward_char,iter, end, '>') &&
     gtk_text_iter_compare (start, end) < 0)
    return TRUE;

  return FALSE;
}

gboolean
ide_xml_find_next_element (const GtkTextIter *iter,
                           GtkTextIter       *start,
                           GtkTextIter       *end)
{

  g_return_val_if_fail (iter != NULL,  FALSE);
  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (end != NULL,   FALSE);

  if(find_char (gtk_text_iter_forward_char, iter, start, '<') &&
     find_char (gtk_text_iter_forward_char,start, end, '>') &&
     gtk_text_iter_compare (start, end) < 0)
    return TRUE;

  return FALSE;
}

gboolean
ide_xml_find_previous_element (const GtkTextIter *iter,
                               GtkTextIter       *start,
                               GtkTextIter       *end)
{
  g_return_val_if_fail (iter != NULL,  FALSE);
  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (end != NULL,   FALSE);

  if(find_char (gtk_text_iter_backward_char, iter, end, '>') &&
     find_char (gtk_text_iter_backward_char, end, start, '<') &&
     gtk_text_iter_compare (start, end) < 0)
    return TRUE;

  return FALSE;
}

IdeXmlElementTagType
ide_xml_get_element_tag_type (const GtkTextIter *start,
                              const GtkTextIter *end)
{

  GtkTextIter curr_start = *start;
  GtkTextIter curr_end = *end;
  gunichar start_ch;
  gunichar end_ch;


  g_return_val_if_fail (ide_xml_in_element (start) &&
                        gtk_text_iter_get_char (start) == '<', IDE_XML_ELEMENT_TAG_UNKNOWN);
  g_return_val_if_fail (ide_xml_in_element (start) &&
                        gtk_text_iter_get_char (end) == '>', IDE_XML_ELEMENT_TAG_UNKNOWN);
  g_return_val_if_fail (gtk_text_iter_compare (start, end) < 0, IDE_XML_ELEMENT_TAG_UNKNOWN);

  /*Move pass the < and > char*/
  g_return_val_if_fail (gtk_text_iter_forward_char (&curr_start), IDE_XML_ELEMENT_TAG_UNKNOWN);
  g_return_val_if_fail (gtk_text_iter_backward_char (&curr_end), IDE_XML_ELEMENT_TAG_UNKNOWN);

  start_ch = gtk_text_iter_get_char (&curr_start);
  end_ch = gtk_text_iter_get_char (&curr_end);

  if (end_ch == '/' ||
      (end_ch == '?' && start_ch == '?') ||
      (end_ch == '-' && start_ch == '!'))
    return IDE_XML_ELEMENT_TAG_START_END;

  if (start_ch == '/')
    return IDE_XML_ELEMENT_TAG_END;

  return IDE_XML_ELEMENT_TAG_START;
}

gchar
*ide_xml_get_element_name (const GtkTextIter *start,
                           const GtkTextIter *end)
{
  GtkTextIter curr;
  GtkTextIter begin = *start;

  g_return_val_if_fail (ide_xml_in_element (start) &&
                        gtk_text_iter_get_char (start) == '<', NULL);
  g_return_val_if_fail (ide_xml_in_element (start) &&
                        gtk_text_iter_get_char (end) == '>', NULL);
  g_return_val_if_fail (gtk_text_iter_compare (start, end) < 0, FALSE);

  /*
   * We need to move pass by the start '<' and closing '/' char of the element
   */
  while (gtk_text_iter_get_char (&begin) == '<' || gtk_text_iter_get_char (&begin) == '/')
    gtk_text_iter_forward_char (&begin);

  /* Comments and elements starting with ? do not have a name */
  if (gtk_text_iter_get_char (&begin) == '!' || gtk_text_iter_get_char (&begin) == '?')
    return NULL;

  curr = begin;
  /*
   * Find the end of the element name by iterating over it until we find
   * a '/' or '>' or ' ' char
   */
  if (gtk_text_iter_forward_find_char (&curr, find_end_element_char, NULL, end) &&
      gtk_text_iter_compare (&begin,&curr) < 0)
    return gtk_text_iter_get_slice (&begin, &curr);

  return NULL;
}

gboolean
ide_xml_find_closing_element (const GtkTextIter *start,
                              const GtkTextIter *end,
                              GtkTextIter       *found_element_start,
                              GtkTextIter       *found_element_end)
{
  IdeXmlElementTagType tag_type;
  GQueue *element_queue;
  guint element_queue_length = 0;
  gchar *element_name = NULL;

  g_return_val_if_fail (found_element_start != NULL, FALSE);
  g_return_val_if_fail (found_element_end != NULL, FALSE);

  tag_type = ide_xml_get_element_tag_type (start, end);
  if (tag_type != IDE_XML_ELEMENT_TAG_START)
    return FALSE;

  element_name = ide_xml_get_element_name (start, end);
  if (element_name == NULL)
    return FALSE;

  element_queue = g_queue_new();
  g_queue_push_head(element_queue, element_name);

  while (g_queue_get_length (element_queue) > 0 &&
         ide_xml_find_next_element (end, found_element_start, found_element_end))
    {
      tag_type = ide_xml_get_element_tag_type (found_element_start, found_element_end);
      if (tag_type == IDE_XML_ELEMENT_TAG_START)
        {
          element_name = ide_xml_get_element_name (found_element_start, found_element_end);
          if (element_name != NULL)
            g_queue_push_head(element_queue, element_name);
        }
      else if (tag_type == IDE_XML_ELEMENT_TAG_END)
        {
          element_name = ide_xml_get_element_name (found_element_start, found_element_end);
          if (element_name != NULL)
            {
              if(g_strcmp0 (g_queue_peek_head (element_queue), element_name) == 0)
                {
                  g_free (g_queue_pop_head (element_queue));
                  g_free (element_name);
                }
              /*Unbalanced element.Stop parsing*/
              else
                {
                  g_free (element_name);
                  goto completed;
                }
            }
        }
      end = found_element_end;
    }

completed:
  element_queue_length = g_queue_get_length (element_queue);
  g_queue_free_full (element_queue, g_free);

  return element_queue_length > 0 ? FALSE : TRUE;
}

gboolean
ide_xml_find_opening_element (const GtkTextIter *start,
                              const GtkTextIter *end,
                              GtkTextIter       *found_element_start,
                              GtkTextIter       *found_element_end)
{
  IdeXmlElementTagType tag_type;
  GQueue *element_queue;
  guint element_queue_length = 0;
  gchar *element_name = NULL;

  g_return_val_if_fail (found_element_start != NULL, FALSE);
  g_return_val_if_fail (found_element_end != NULL, FALSE);

  tag_type = ide_xml_get_element_tag_type (start, end);
  if (tag_type != IDE_XML_ELEMENT_TAG_END)
    return FALSE;

  element_name = ide_xml_get_element_name (start, end);
  if (element_name == NULL)
    return FALSE;

  element_queue = g_queue_new();
  g_queue_push_head(element_queue, element_name);

  while (g_queue_get_length (element_queue) > 0 &&
         ide_xml_find_previous_element (start, found_element_start, found_element_end))
    {
      tag_type = ide_xml_get_element_tag_type (found_element_start, found_element_end);
      if (tag_type == IDE_XML_ELEMENT_TAG_END)
        {
          element_name = ide_xml_get_element_name (found_element_start, found_element_end);
          if (element_name != NULL)
            g_queue_push_head(element_queue, element_name);
        }
      else if (tag_type == IDE_XML_ELEMENT_TAG_START)
        {
          element_name = ide_xml_get_element_name (found_element_start, found_element_end);
          if (element_name != NULL)
            {
              if(g_strcmp0 (g_queue_peek_head(element_queue), element_name) == 0)
                {
                  g_free (g_queue_pop_head (element_queue));
                  g_free (element_name);
                }
              /*Unbalanced element.Stop parsing*/
              else
                {
                  g_free (element_name);
                  goto completed;
                }
            }
        }
      start = found_element_start;
    }

completed:
  element_queue_length = g_queue_get_length (element_queue);
  g_queue_free_full (element_queue, g_free);

  return element_queue_length > 0 ? FALSE : TRUE;
}


