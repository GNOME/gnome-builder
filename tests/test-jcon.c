#include "jcon.h"

static void
test_basic (void)
{
  g_autoptr(JsonNode) node = NULL;
  const gchar *foo1 = NULL;
  gint baz_baz_baz = 0;
  gboolean r;

  node = JCON_NEW (
    "foo", "foo1",
    "bar", "foo2",
    "baz", "{",
      "baz", "[", "{", "baz", JCON_INT (123), "}", "]",
    "}"
  );

  g_assert (node != NULL);

  r = JCON_EXTRACT (node,
    "foo", JCONE_STRING (foo1),
    "baz", "{",
      "baz", "[", "{", "baz", JCONE_INT (baz_baz_baz), "}", "]",
    "}"
  );

  g_assert_cmpstr (foo1, ==, "foo1");
  g_assert_cmpint (baz_baz_baz, ==, 123);
  g_assert_cmpint (r, ==, 1);
}

static void
test_deep_array (void)
{
  g_autoptr(JsonNode) node = NULL;
  const gchar *abc = NULL;
  const gchar *xyz = NULL;
  gboolean r;

  node = JCON_NEW ("foo", "[","[","[","[","[","[","[","[","[","[", "abc", "]", "]","]","]","]","]","]","]","]","]");
  g_assert (node != NULL);

  r = JCON_EXTRACT (node, "foo", "[","[","[","[","[","[","[","[","[","[", JCONE_STRING (abc), "]", "]","]","]","]","]","]","]","]","]");
  g_assert_cmpstr (abc, ==, "abc");
  g_assert_cmpint (r, ==, 1);

  g_clear_pointer (&node, json_node_unref);

  node = JCON_NEW ("foo", "[","[","[","[","[","[","[","[","[","{", "foo", "xyz", "}", "]","]","]","]","]","]","]","]","]");
  g_assert (node != NULL);

  r = JCON_EXTRACT (node, "foo", "[","[","[","[","[","[","[","[","[","{", "foo", JCONE_STRING (xyz), "}", "]","]","]","]","]","]","]","]","]");
  g_assert_cmpstr (xyz, ==, "xyz");
  g_assert_cmpint (r, ==, 1);
}

static void
test_extract_array (void)
{
  g_autoptr(JsonNode) node = NULL;
  JsonArray *ar123 = NULL;
  gboolean r;

  node = JCON_NEW ("foo", "[", JCON_INT (1), JCON_INT (2), JCON_INT (3), "]");
  g_assert (node != NULL);

  r = JCON_EXTRACT (node, "foo", JCONE_ARRAY (ar123));
  g_assert (ar123 != NULL);
  g_assert_cmpint (r, ==, 1);
  g_assert_cmpint (3, ==, json_array_get_length (ar123));
  g_assert (JSON_NODE_HOLDS_VALUE (json_array_get_element (ar123, 0)));
  g_assert (JSON_NODE_HOLDS_VALUE (json_array_get_element (ar123, 1)));
  g_assert (JSON_NODE_HOLDS_VALUE (json_array_get_element (ar123, 2)));
  g_assert_cmpint (json_node_get_int (json_array_get_element (ar123, 0)), ==, 1);
  g_assert_cmpint (json_node_get_int (json_array_get_element (ar123, 1)), ==, 2);
  g_assert_cmpint (json_node_get_int (json_array_get_element (ar123, 2)), ==, 3);

}

static void
test_extract_object (void)
{
  g_autoptr(JsonNode) node = NULL;
  JsonObject *object = NULL;
  gboolean r;

  node = JCON_NEW ("foo", "{", "bar", "[", JCON_INT (1), "two", JCON_INT (3), "]", "}");
  g_assert (node != NULL);

  r = JCON_EXTRACT (node, "foo", JCONE_OBJECT (object));
  g_assert (object != NULL);
  g_assert (json_object_has_member (object, "bar"));
  g_assert (JSON_NODE_HOLDS_ARRAY (json_object_get_member (object, "bar")));
  g_assert (JSON_NODE_HOLDS_VALUE (json_array_get_element (json_object_get_array_member (object, "bar"), 1)));
  g_assert_cmpstr ("two", ==, json_node_get_string (json_array_get_element (json_object_get_array_member (object, "bar"), 1)));
  g_assert (r == TRUE);

}

static void
test_extract_node (void)
{
  g_autoptr(JsonNode) node = NULL;
  JsonNode *ar = NULL;
  gboolean r;

  node = JCON_NEW ("foo", "{", "bar", "[", JCON_INT (1), "two", JCON_INT (3), "]", "}");
  g_assert (node != NULL);

  r = JCON_EXTRACT (node, "foo", "{", "bar", JCONE_NODE (ar), "}");
  g_assert (ar != NULL);
  g_assert_cmpint (r, ==, TRUE);
}

static void
test_paren (void)
{
  g_autoptr(JsonNode) node = NULL;
  const gchar *paren = "{";
  const gchar *str = NULL;
  gboolean r;

  node = JCON_NEW ("foo", "{", "bar", "[", JCON_STRING (paren), "]", "}");
  g_assert (node != NULL);

  r = JCON_EXTRACT (node, "foo", "{", "bar", "[", JCONE_STRING (str), "]", "}");
  g_assert_cmpstr (str, ==, "{");
  g_assert_cmpint (r, ==, TRUE);
}

static void
test_array_toplevel (void)
{
  g_autoptr(JsonNode) node = NULL;
  JsonNode *ar = NULL;
  const gchar *a = NULL;
  const gchar *b = NULL;
  gboolean r;

  node = JCON_NEW ("foo", "[", "a", "b", "c", "d", "e", "]");
  g_assert (node != NULL);

  r = JCON_EXTRACT (node, "foo", JCONE_NODE (ar));
  g_assert (ar);
  g_assert_cmpint (r, ==, TRUE);

  r = JCON_EXTRACT (ar, JCONE_STRING (a), JCONE_STRING (b));
  g_assert_cmpstr (a, ==, "a");
  g_assert_cmpint (r, ==, TRUE);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Jcon/basic", test_basic);
  g_test_add_func ("/Jcon/deep_array", test_deep_array);
  g_test_add_func ("/Jcon/extract_array", test_extract_array);
  g_test_add_func ("/Jcon/extract_object", test_extract_object);
  g_test_add_func ("/Jcon/extract_node", test_extract_node);
  g_test_add_func ("/Jcon/paren", test_paren);
  g_test_add_func ("/Jcon/array_toplevel", test_array_toplevel);
  return g_test_run ();
}
