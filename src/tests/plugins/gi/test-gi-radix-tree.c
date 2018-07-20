/* test-radix-tree.c
 *
 * Copyright (C) 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <stdio.h>

#include "../../../plugins/gi/ide-gi-macros.h"

#include "../../../plugins/gi/radix-tree/ide-gi-radix-tree-builder.h"
#include "../../../plugins/gi/radix-tree/ide-gi-flat-radix-tree.h"

typedef struct
{
  const gchar *word;
  guint64      payload1;
  guint64      payload2;
} Item;

static const Item items[] = {
  { "tAb",      0x00000001 , 0x10000000 },
  { "tAble",    0x00000002 , 0x20000000 },
  { "tablette", 0x00000003 , 0x30000000 },
  { "tableau",  0x00000004 , 0x40000000 },
  { "tablier",  0x00000005 , 0x50000000 },
  { "voiTure",  0x00000006 , 0x60000000 },
  { "voIle",    0x00000007 , 0x70000000 },
  { "VoiSin",   0x00000008 , 0x80000000 },
  { "mai",      0x00000009 , 0x90000000 },
  { "ma",       0x00000011 , 0x11000000 },
  { "Mai",      0x00000012 , 0x12000000 },
  { "MaiS",     0x00000013 , 0x13000000 },
};

static gboolean
is_in_array (const gchar  *word,
             gchar       **ar)
{
  while (*ar != NULL)
    {
      if (g_str_equal (word, *ar))
        return TRUE;

      ar++;
    }

  return FALSE;
}

static gboolean
compare_builder_result (GArray  *ar,
                        gchar  **result,
                        guint    nb_elem)
{
  for (guint i = 0; i < ar->len; i++)
    {
      IdeGiRadixTreeCompleteItem *item = &g_array_index (ar, IdeGiRadixTreeCompleteItem, i);

      if (!is_in_array (item->word, result))
        return FALSE;
    }

  if (ar->len != nb_elem)
    return FALSE;

  return TRUE;
}

static void
test_tree_builder (void)
{
  g_autoptr(IdeGiRadixTreeBuilder) tree = ide_gi_radix_tree_builder_new ();

  g_assert (ide_gi_radix_tree_builder_is_empty (tree));

  for (guint i = 0; i < G_N_ELEMENTS (items); i++)
    ide_gi_radix_tree_builder_add (tree, items[i].word, 2, (gpointer)&items[i].payload1);

  for (guint i = 0; i < G_N_ELEMENTS (items); i++)
    {
      IdeGiRadixTreeNode *node;

      node = ide_gi_radix_tree_builder_lookup (tree, items[i].word);
      g_assert (node != NULL &&
                node->payloads != NULL &&
                node->payloads[0] == items[i].payload1 &&
                node->payloads[1] == items[i].payload2);
    }

  g_assert (!ide_gi_radix_tree_builder_is_empty (tree));
  ide_gi_radix_tree_builder_remove (tree, "tAble");
  g_assert (NULL == ide_gi_radix_tree_builder_lookup (tree, "tAble"));

  /* Test completion */
  {
    static const gchar *result[] = {"tablette", "tableau", "tablier", NULL};
    g_autoptr(GArray) ar = ide_gi_radix_tree_builder_complete (tree, "tab");
    g_assert (compare_builder_result (ar, (gchar **)result, 3));
  }

  /* Test payload */
    {
      IdeGiRadixTreeNode *node;
      static guint64 payloads[] = {1,2,3,4,5,6,7};
      ide_gi_radix_tree_builder_add (tree, "payload", 3, &payloads[2]);
      node = ide_gi_radix_tree_builder_lookup (tree, "payload");
      g_assert (node->nb_payloads == 3);
      g_assert (node->payloads[0] == 3);
      g_assert (node->payloads[1] == 4);
      g_assert (node->payloads[2] == 5);

      ide_gi_radix_tree_builder_node_prepend_payload (node, 1, &payloads[0]);
      ide_gi_radix_tree_builder_node_insert_payload (node, 1, 1, &payloads[1]);
      ide_gi_radix_tree_builder_node_append_payload (node, 2, &payloads[5]);
      g_assert (node->nb_payloads == 7);

      g_assert (node->payloads[0] == 1);
      g_assert (node->payloads[1] == 2);
      g_assert (node->payloads[2] == 3);
      g_assert (node->payloads[3] == 4);
      g_assert (node->payloads[4] == 5);
      g_assert (node->payloads[5] == 6);
      g_assert (node->payloads[6] == 7);

      ide_gi_radix_tree_builder_node_remove_payload (node, 0);
      ide_gi_radix_tree_builder_node_remove_payload (node, 2);
      g_assert (node->payloads[0] == 2);
      g_assert (node->payloads[2] == 5);

      g_assert (node->nb_payloads == 5);
    }
}

static gboolean
compare_flat_result (GArray  *ar,
                     gchar  **result,
                     guint    nb_elem)
{
  for (guint i = 0; i < ar->len; i++)
    {
      IdeGiFlatRadixTreeCompleteItem *item = &g_array_index (ar, IdeGiFlatRadixTreeCompleteItem, i);

      if (!is_in_array (item->word, result))
        return FALSE;
    }

  if (ar->len != nb_elem)
    return FALSE;

  return TRUE;
}

static void
test_tree_flat (void)
{
  g_autoptr(IdeGiRadixTreeBuilder) tree = NULL;
  g_autoptr(GByteArray) ba = NULL;
  g_autoptr(IdeGiFlatRadixTree) flat = NULL;
  guint64 *payloads;
  guint nb_payloads;

  tree = ide_gi_radix_tree_builder_new ();
  for (guint i = 0; i < G_N_ELEMENTS (items); i++)
    ide_gi_radix_tree_builder_add (tree, items[i].word, 2, (gpointer)&items[i].payload1);

  ba = ide_gi_radix_tree_builder_serialize (tree);
  flat = ide_gi_flat_radix_tree_new ();

NO_CAST_ALIGN_PUSH
  ide_gi_flat_radix_tree_init (flat, (guint64 *)ba->data, ba->len);
NO_CAST_ALIGN_POP

  for (guint i = 0; i < G_N_ELEMENTS (items); i++)
    {
      gboolean ret;

      ret = ide_gi_flat_radix_tree_lookup (flat, items[i].word, &payloads, &nb_payloads);
      g_assert (ret == TRUE);
      g_assert (nb_payloads == 2);
      g_assert (((guint64 *)payloads)[0] == items[i].payload1);
      g_assert (((guint64 *)payloads)[1] == items[i].payload2);
    }

  /* Test completion */
  {
    static const gchar *result[] = {"tAb", "tAble", "tablette", "tableau", "tablier", NULL};
    g_autoptr(GArray) ar = ide_gi_flat_radix_tree_complete (flat, "tab", FALSE, FALSE);
    g_assert (compare_flat_result (ar, (gchar **)result, 5));
  }

  {
    static const gchar *result[] = {"voiTure", "voIle", "VoiSin", NULL};
    g_autoptr(GArray) ar = ide_gi_flat_radix_tree_complete (flat, "voi", FALSE, FALSE);
    g_assert (compare_flat_result (ar, (gchar **)result, 3));
  }

/* Test case-sensitive */
  {
    static const gchar *result[] = {"VoiSin", NULL};
    g_autoptr(GArray) ar = ide_gi_flat_radix_tree_complete (flat, "Voi", FALSE, TRUE);
    g_assert (compare_flat_result (ar, (gchar **)result, 1));
  }

  {
    static const gchar *result[] = {"tAb", "tAble", NULL};
    g_autoptr(GArray) ar = ide_gi_flat_radix_tree_complete (flat, "tAb", FALSE, TRUE);
    g_assert (compare_flat_result (ar, (gchar **)result, 2));
  }

  {
    static const gchar *result[] = {"VoiSin", NULL};
    g_autoptr(GArray) ar = ide_gi_flat_radix_tree_complete (flat, "Voi", FALSE, TRUE);
    g_assert (compare_flat_result (ar, (gchar **)result, 1));
  }

  /* Test get_prefix */
  {
    static const gchar *result[] = {"mai", "ma", "Mai", "MaiS", NULL};
    g_autoptr(GArray) ar = ide_gi_flat_radix_tree_complete (flat, "maison", TRUE, FALSE);
    g_assert (compare_flat_result (ar, (gchar **)result, 4));
  }

  {
    static const gchar *result[] = {"Mai", "MaiS", NULL};
    g_autoptr(GArray) ar = ide_gi_flat_radix_tree_complete (flat, "MaiSon", TRUE, TRUE);
    g_assert (compare_flat_result (ar, (gchar **)result, 2));
  }
}

static void
test_tree_builder_to_flat_to_builder (void)
{
  g_autoptr(IdeGiRadixTreeBuilder) tree = NULL;
  g_autoptr(IdeGiRadixTreeBuilder) new_tree = NULL;
  g_autoptr(GByteArray) ba = NULL;
  g_autoptr(IdeGiFlatRadixTree) flat = NULL;
  IdeGiRadixTreeNode *node;

  tree = ide_gi_radix_tree_builder_new ();
  for (guint i = 0; i < G_N_ELEMENTS (items); i++)
    ide_gi_radix_tree_builder_add (tree, items[i].word, 2, (gpointer)&items[i].payload1);

  ba = ide_gi_radix_tree_builder_serialize (tree);
  flat = ide_gi_flat_radix_tree_new ();

NO_CAST_ALIGN_PUSH
  ide_gi_flat_radix_tree_init (flat, (guint64 *)ba->data, ba->len);
NO_CAST_ALIGN_POP

  new_tree = ide_gi_flat_radix_tree_deserialize (flat);
  for (guint i = 0; i < G_N_ELEMENTS (items); i++)
    {
      node = ide_gi_radix_tree_builder_lookup (new_tree, items[i].word);
      g_assert (node != NULL);
      g_assert (node->nb_payloads == 2);
      g_assert (node->payloads[0] == items[i].payload1);
      g_assert (node->payloads[1] == items[i].payload2);
    }
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gi/radix_tree/builder", test_tree_builder);
  g_test_add_func ("/gi/radix_tree/flat", test_tree_flat);
  g_test_add_func ("/gi/radix_tree/builder_to_flat_to_builder", test_tree_builder_to_flat_to_builder);

  return g_test_run ();
}
