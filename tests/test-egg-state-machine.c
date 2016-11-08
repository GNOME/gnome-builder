/* test-egg-state-machine.c
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

#include <glib-object.h>
#include <gio/gio.h>

#include "egg-state-machine.h"

struct _TestObject
{
  GObject parent_instance;

  gint obj1_count;
  gint obj2_count;

  gchar *str;
};

#define TEST_TYPE_OBJECT (test_object_get_type())
G_DECLARE_FINAL_TYPE (TestObject, test_object, TEST, OBJECT, GObject)
G_DEFINE_TYPE (TestObject, test_object, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_STRING,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
  TestObject *obj = (TestObject *)object;

  switch (prop_id)
    {
    case PROP_STRING:
      g_value_set_string (value, obj->str);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  TestObject *obj = (TestObject *)object;

  switch (prop_id)
    {
    case PROP_STRING:
      g_free (obj->str);
      obj->str = g_value_dup_string (value);
      g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
finalize (GObject *object)
{
  TestObject *self = (TestObject *)object;

  g_free (self->str);

  G_OBJECT_CLASS (test_object_parent_class)->finalize (object);
}

static void
test_object_class_init (TestObjectClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->finalize = finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  properties [PROP_STRING] =
    g_param_spec_string ("string",
                         "string",
                         "string",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (obj_class, LAST_PROP, properties);

  g_signal_new ("frobnicate",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL, NULL,
                G_TYPE_NONE,
                0);
}

static void
test_object_init (TestObject *self)
{
}

static void
obj1_frobnicate (TestObject *dummy,
                 TestObject *source)
{
  g_assert (TEST_IS_OBJECT (dummy));
  g_assert (TEST_IS_OBJECT (source));

  dummy->obj1_count++;
}

static void
obj2_frobnicate (TestObject *dummy,
                 TestObject *source)
{
  g_assert (TEST_IS_OBJECT (dummy));
  g_assert (TEST_IS_OBJECT (source));

  dummy->obj2_count++;
}

static void
assert_prop_equal (gpointer     obja,
                   gpointer     objb,
                   const gchar *propname)
{
  GParamSpec *pspec;
  GValue va = G_VALUE_INIT;
  GValue vb = G_VALUE_INIT;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (obja), propname);
  g_assert (pspec != NULL);

  g_value_init (&va, pspec->value_type);
  g_value_init (&vb, pspec->value_type);

  g_object_get_property (obja, propname, &va);
  g_object_get_property (objb, propname, &vb);

#define ADD_CHECK(NAME, name, cmp_type) \
  case G_TYPE_##NAME: \
    g_assert_cmp##cmp_type (g_value_get_##name (&va), ==, g_value_get_##name (&vb)); \
    break

  switch (pspec->value_type)
    {
    ADD_CHECK (INT, int, int);
    ADD_CHECK (BOOLEAN, boolean, int);

    ADD_CHECK (UINT, uint, uint);

    ADD_CHECK (FLOAT, float, float);
    ADD_CHECK (DOUBLE, double, float);

    ADD_CHECK (STRING, string, str);

    default:
      g_assert_not_reached ();
    }

  g_value_unset (&va);
  g_value_unset (&vb);
}

#if 0
static gboolean
has_style_class (GtkWidget   *widget,
                 const gchar *class_name)
{
  GtkStyleContext *style_context;

  style_context = gtk_widget_get_style_context (widget);
  return gtk_style_context_has_class (style_context, class_name);
}
#endif

static void
test_state_machine_basic (void)
{
  EggStateMachine *machine;
  GSimpleAction *action;
  TestObject *dummy;
  TestObject *obj1;
  TestObject *obj2;

  machine = egg_state_machine_new ();
  g_object_add_weak_pointer (G_OBJECT (machine), (gpointer *)&machine);

  action = g_simple_action_new ("my-action", NULL);
  dummy = g_object_new (TEST_TYPE_OBJECT, NULL);
  obj1 = g_object_new (TEST_TYPE_OBJECT, NULL);
  obj2 = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_simple_action_set_enabled (action, FALSE);

#if 0
  g_print ("obj1=%p  obj2=%p  dummy=%p\n", obj1, obj2, dummy);
#endif

  egg_state_machine_connect_object (machine, "state1", obj1, "frobnicate",
                                    G_CALLBACK (obj1_frobnicate), dummy, G_CONNECT_SWAPPED);
  egg_state_machine_connect_object (machine, "state2", obj2, "frobnicate",
                                    G_CALLBACK (obj2_frobnicate), dummy, G_CONNECT_SWAPPED);

  egg_state_machine_add_binding (machine, "state1", obj1, "string", dummy, "string", 0);
  egg_state_machine_add_binding (machine, "state2", obj2, "string", dummy, "string", 0);

  egg_state_machine_add_property (machine, "state1", action, "enabled", TRUE);
  egg_state_machine_add_property (machine, "state2", action, "enabled", FALSE);
  egg_state_machine_add_property (machine, "state3", action, "enabled", FALSE);

  g_assert_false (g_action_get_enabled (G_ACTION (action)));

  egg_state_machine_set_state (machine, "state1");
  g_assert_cmpstr (egg_state_machine_get_state (machine), ==, "state1");
  g_assert_cmpint (dummy->obj1_count, ==, 0);
  g_assert_cmpint (dummy->obj2_count, ==, 0);

  g_assert_true (g_action_get_enabled (G_ACTION (action)));

  g_signal_emit_by_name (obj1, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_assert_cmpint (dummy->obj2_count, ==, 0);

  g_signal_emit_by_name (obj2, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_assert_cmpint (dummy->obj2_count, ==, 0);

  egg_state_machine_set_state (machine, "state2");
  g_assert_cmpstr (egg_state_machine_get_state (machine), ==, "state2");

  g_assert_false (g_action_get_enabled (G_ACTION (action)));

  g_signal_emit_by_name (obj1, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_assert_cmpint (dummy->obj2_count, ==, 0);

  g_signal_emit_by_name (obj2, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_assert_cmpint (dummy->obj2_count, ==, 1);

  g_object_set (obj2, "string", "obj2", NULL);
  g_object_set (obj1, "string", "obj1", NULL);
  assert_prop_equal (obj2, dummy, "string");

  egg_state_machine_set_state (machine, "state3");
  egg_state_machine_set_state (machine, "state1");

  assert_prop_equal (obj1, dummy, "string");
  g_object_set (obj1, "string", "obj1-1", NULL);
  assert_prop_equal (obj1, dummy, "string");
  g_object_set (obj2, "string", "obj2-1", NULL);
  assert_prop_equal (obj1, dummy, "string");

  egg_state_machine_set_state (machine, "state3");

  g_object_unref (machine);
  g_assert (machine == NULL);

  g_clear_object (&action);
}

#define assert_final_ref(o) \
  G_STMT_START \
    { \
      GObject **object_ptr = (GObject **)o; \
\
      g_object_add_weak_pointer (*object_ptr, (gpointer *)object_ptr); \
      g_object_unref (*object_ptr); \
      g_assert_null (*object_ptr); \
    } \
  G_STMT_END


/* This test exposed multiple bugs in GObject:
 *   https://bugzilla.gnome.org/show_bug.cgi?id=749659
 *   https://bugzilla.gnome.org/show_bug.cgi?id=749660
 */
#if 0
static void
test_state_machine_weak_ref_source (void)
{
  EggStateMachine *machine;
  GSimpleAction *action;
  TestObject *dummy;
  TestObject *obj;
  GtkWidget *widget;

  machine = egg_state_machine_new ();

  action = g_simple_action_new ("my-action", NULL);
  dummy = g_object_new (TEST_TYPE_OBJECT, NULL);
  obj = g_object_new (TEST_TYPE_OBJECT, NULL);
  widget = g_object_ref_sink (gtk_event_box_new ());

  g_simple_action_set_enabled (action, FALSE);

  egg_state_machine_connect_object (machine, "state", obj, "frobnicate",
                                    G_CALLBACK (obj1_frobnicate), dummy, G_CONNECT_SWAPPED);
  egg_state_machine_add_binding (machine, "state", obj, "string", dummy, "string", 0);
  egg_state_machine_add_property (machine, "state", action, "enabled", TRUE);
  egg_state_machine_add_style (machine, "state", widget, "testing");

  egg_state_machine_set_state (machine, "state");
  g_assert_cmpstr (egg_state_machine_get_state (machine), ==, "state");

  /* Check that everything is working */
  g_signal_emit_by_name (obj, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_object_set (obj, "string", "hello world", NULL);
  assert_prop_equal (obj, dummy, "string");
  g_assert_true (g_action_get_enabled (G_ACTION (action)));
  g_assert (has_style_class (widget, "testing"));

  /* Destroy the source objects while still in the state */
  assert_final_ref (&widget);
  assert_final_ref (&obj);
  assert_final_ref (&dummy);
  assert_final_ref (&action);

  /* Go back and forth between the states as this would cause
   * a warning if the source objects did not have a weakref on them
   */
  egg_state_machine_set_state (machine, NULL);
  g_assert_cmpstr (egg_state_machine_get_state (machine), ==, NULL);
  egg_state_machine_set_state (machine, "state");
  g_assert_cmpstr (egg_state_machine_get_state (machine), ==, "state");
  egg_state_machine_set_state (machine, "empty");
  g_assert_cmpstr (egg_state_machine_get_state (machine), ==, "empty");
  egg_state_machine_set_state (machine, "state");
  g_assert_cmpstr (egg_state_machine_get_state (machine), ==, "state");
  egg_state_machine_set_state (machine, NULL);
  g_assert_cmpstr (egg_state_machine_get_state (machine), ==, NULL);

  assert_final_ref (&machine);
}
#endif

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gtk_init (&argc, &argv);
  g_test_add_func ("/Egg/StateMachine/basic", test_state_machine_basic);
  /*g_test_add_func ("/Egg/StateMachine/weak-ref-source", test_state_machine_weak_ref_source);*/
  return g_test_run ();
}
