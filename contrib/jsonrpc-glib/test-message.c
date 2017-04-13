#include <json-glib/json-glib.h>

#include "jsonrpc-message.h"

static void
test_basic (void)
{
  g_autoptr(GVariant) node = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) deserialized = NULL;
  const gchar *foo1 = NULL;
  gint64 baz_baz_baz = 0;
  gboolean r;

  node = JSONRPC_MESSAGE_NEW (
    "foo", "foo1",
    "bar", "foo2",
    "baz", "{",
      "baz", "[", "{", "baz", JSONRPC_MESSAGE_PUT_INT64 (123), "}", "]",
    "}"
  );

  g_assert (node != NULL);

  r = JSONRPC_MESSAGE_PARSE (node,
    "foo", JSONRPC_MESSAGE_GET_STRING (&foo1),
    "baz", "{",
      "baz", "[", "{", "baz", JSONRPC_MESSAGE_GET_INT64 (&baz_baz_baz), "}", "]",
    "}"
  );

  g_assert_cmpstr (foo1, ==, "foo1");
  g_assert_cmpint (baz_baz_baz, ==, 123);
  g_assert_cmpint (r, ==, 1);

  /* compare json gvariant encoding to ensure it matches */
#define TESTSTR "{'foo': 'foo1', 'bar': 'foo2', 'baz': {'baz': [{'baz': 123}]}}"
  parser = json_parser_new ();
  r = json_parser_load_from_data (parser, TESTSTR, -1, &error);
  g_assert (r);
  g_assert_no_error (error);
  deserialized = json_gvariant_deserialize (json_parser_get_root (parser), NULL, &error);
  g_assert (deserialized);
  g_assert_no_error (error);
  g_assert (g_variant_equal (deserialized, node));
#undef TESTSTR
}

static void
test_deep_array (void)
{
  g_autoptr(GVariant) node = NULL;
  const gchar *abc = NULL;
  const gchar *xyz = NULL;
  gboolean r;

  node = JSONRPC_MESSAGE_NEW ("foo", "[","[","[","[","[","[","[","[","[","[", "abc", "]", "]","]","]","]","]","]","]","]","]");
  g_assert (node != NULL);

  r = JSONRPC_MESSAGE_PARSE (node, "foo", "[","[","[","[","[","[","[","[","[","[", JSONRPC_MESSAGE_GET_STRING (&abc), "]", "]","]","]","]","]","]","]","]","]");
  g_assert_cmpstr (abc, ==, "abc");
  g_assert_cmpint (r, ==, 1);

  g_clear_pointer (&node, g_variant_unref);

  node = JSONRPC_MESSAGE_NEW ("foo", "[","[","[","[","[","[","[","[","[","{", "foo", "xyz", "}", "]","]","]","]","]","]","]","]","]");
  g_assert (node != NULL);

  r = JSONRPC_MESSAGE_PARSE (node, "foo", "[","[","[","[","[","[","[","[","[","{", "foo", JSONRPC_MESSAGE_GET_STRING (&xyz), "}", "]","]","]","]","]","]","]","]","]");
  g_assert_cmpstr (xyz, ==, "xyz");
  g_assert_cmpint (r, ==, 1);
}

static void
test_extract_array (void)
{
  g_autoptr(GVariant) node = NULL;
  g_autoptr(GVariant) ar123 = NULL;
  gboolean r;
  gint32 a=0, b=0, c=0;

  node = JSONRPC_MESSAGE_NEW ("foo", "[", JSONRPC_MESSAGE_PUT_INT32 (1), JSONRPC_MESSAGE_PUT_INT32 (2), JSONRPC_MESSAGE_PUT_INT32 (3), "]");
  g_assert (node != NULL);

  r = JSONRPC_MESSAGE_PARSE (node, "foo", JSONRPC_MESSAGE_GET_VARIANT (&ar123));
  g_assert (ar123 != NULL);
  g_assert_cmpint (r, ==, 1);
  g_assert_cmpint (3, ==, g_variant_n_children (ar123));

  r = JSONRPC_MESSAGE_PARSE (node, "foo", "[", JSONRPC_MESSAGE_GET_INT32 (&a), JSONRPC_MESSAGE_GET_INT32 (&b), JSONRPC_MESSAGE_GET_INT32 (&c), "]");
  g_assert_cmpint (r, ==, 1);
  g_assert_cmpint (a, ==, 1);
  g_assert_cmpint (b, ==, 2);
  g_assert_cmpint (c, ==, 3);
}

static void
test_extract_object (void)
{
  g_autoptr(GVariant) node = NULL;
  g_autoptr(GVariantDict) dict = NULL;
  gboolean r;

  node = JSONRPC_MESSAGE_NEW ("foo", "{", "bar", "[", JSONRPC_MESSAGE_PUT_INT32 (1), "two", JSONRPC_MESSAGE_PUT_INT32 (3), "]", "}");
  g_assert (node != NULL);

  r = JSONRPC_MESSAGE_PARSE (node, "foo", JSONRPC_MESSAGE_GET_DICT (&dict));
  g_assert (dict != NULL);
  g_assert (g_variant_dict_contains (dict, "bar"));
  g_assert_cmpint (r, ==, TRUE);
}

static void
test_extract_node (void)
{
  g_autoptr(GVariant) node = NULL;
  g_autoptr(GVariant) ar = NULL;
  gboolean r;

  node = JSONRPC_MESSAGE_NEW ("foo", "{", "bar", "[", JSONRPC_MESSAGE_PUT_INT32 (1), "two", JSONRPC_MESSAGE_PUT_INT32 (3), "]", "}");
  g_assert (node != NULL);

  r = JSONRPC_MESSAGE_PARSE (node, "foo", "{", "bar", JSONRPC_MESSAGE_GET_VARIANT (&ar), "}");
  g_assert (ar != NULL);
  g_assert_cmpint (r, ==, TRUE);
}

static void
test_paren (void)
{
  g_autoptr(GVariant) node = NULL;
  const gchar *paren = "{";
  const gchar *str = NULL;
  gboolean r;

  node = JSONRPC_MESSAGE_NEW ("foo", "{", "bar", "[", JSONRPC_MESSAGE_PUT_STRING (paren), "]", "}");
  g_assert (node != NULL);

  r = JSONRPC_MESSAGE_PARSE (node, "foo", "{", "bar", "[", JSONRPC_MESSAGE_GET_STRING (&str), "]", "}");
  g_assert_cmpstr (str, ==, "{");
  g_assert_cmpint (r, ==, TRUE);
}

static void
test_array_toplevel (void)
{
  g_autoptr(GVariant) node = NULL;
  g_autoptr(GVariantIter) iter = NULL;
  const gchar *a = NULL;
  const gchar *b = NULL;
  gboolean r;

  node = JSONRPC_MESSAGE_NEW ("foo", "[", "a", "b", "c", "d", "e", "]");
  g_assert (node != NULL);

  r = JSONRPC_MESSAGE_PARSE (node, "foo", JSONRPC_MESSAGE_GET_ITER (&iter));
  g_assert_cmpint (r, ==, TRUE);
  g_assert (iter != NULL);

  r = JSONRPC_MESSAGE_PARSE_ARRAY (iter, JSONRPC_MESSAGE_GET_STRING (&a), JSONRPC_MESSAGE_GET_STRING (&b));
  g_assert_cmpint (r, ==, TRUE);
  g_assert_cmpstr (a, ==, "a");
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
