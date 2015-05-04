/* test-egg-state-machine.h
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

static GParamSpec *gParamSpecs [LAST_PROP];

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
  TestObject *obj = (TestObject *)object;

  switch (prop_id) {
  case PROP_STRING:
    g_value_set_string (value, obj->str);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  TestObject *obj = (TestObject *)object;

  switch (prop_id) {
  case PROP_STRING:
    g_free (obj->str);
    obj->str = g_value_dup_string (value);
    g_object_notify_by_pspec (object, pspec);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
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

  g_signal_new ("frobnicate", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  obj_class->finalize = finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  gParamSpecs [PROP_STRING] =
    g_param_spec_string ("string",
                         "string",
                         "string",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (obj_class, LAST_PROP, gParamSpecs);
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
  GValue va = { 0 };
  GValue vb = { 0 };

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (obja), propname);
  g_assert (pspec != NULL);

  g_value_init (&va, pspec->value_type);
  g_value_init (&vb, pspec->value_type);

  g_object_get_property (obja, propname, &va);
  g_object_get_property (objb, propname, &vb);

#define ADD_NUMBER_CHECK(NAME, name) \
  case G_TYPE_##NAME: \
    g_assert_cmpint(g_value_get_##name(&va), ==, g_value_get_##name(&vb)); \
    break

  switch (pspec->value_type)
    {
    case G_TYPE_STRING:
      g_assert_cmpstr (g_value_get_string (&va), ==, g_value_get_string (&vb));
      break;

    ADD_NUMBER_CHECK (INT, int);
    ADD_NUMBER_CHECK (UINT, uint);
    ADD_NUMBER_CHECK (FLOAT, float);
    ADD_NUMBER_CHECK (DOUBLE, double);
    ADD_NUMBER_CHECK (BOOLEAN, boolean);

    default:
      g_assert_not_reached ();
    }

  g_value_unset (&va);
  g_value_unset (&vb);
}

static EggStateTransition
transition_cb (EggStateMachine *machine,
               const gchar     *old_state,
               const gchar     *new_state,
               gpointer         user_data)
{
  /* allow any state to state3, except state2 */

  if ((g_strcmp0 (old_state, "state2") == 0) && (g_strcmp0 (new_state, "state3") == 0))
    return EGG_STATE_TRANSITION_INVALID;

  return EGG_STATE_TRANSITION_IGNORED;
}

static void
test_state_machine_basic (void)
{
  EggStateMachine *machine;
  EggStateTransition ret;
  TestObject *dummy;
  TestObject *obj1;
  TestObject *obj2;
  GError *error = NULL;

  machine = egg_state_machine_new ();
  g_object_add_weak_pointer (G_OBJECT (machine), (gpointer *)&machine);

  dummy = g_object_new (TEST_TYPE_OBJECT, NULL);
  obj1 = g_object_new (TEST_TYPE_OBJECT, NULL);
  obj2 = g_object_new (TEST_TYPE_OBJECT, NULL);

#if 0
  g_print ("obj1=%p  obj2=%p  dummy=%p\n", obj1, obj2, dummy);
#endif

  egg_state_machine_connect_object (machine, "state1", obj1, "frobnicate", G_CALLBACK (obj1_frobnicate), dummy, G_CONNECT_SWAPPED);
  egg_state_machine_connect_object (machine, "state2", obj2, "frobnicate", G_CALLBACK (obj2_frobnicate), dummy, G_CONNECT_SWAPPED);

  egg_state_machine_bind (machine, "state1", obj1, "string", dummy, "string", 0);
  egg_state_machine_bind (machine, "state2", obj2, "string", dummy, "string", 0);

  g_signal_connect (machine, "transition", G_CALLBACK (transition_cb), NULL);

  ret = egg_state_machine_transition (machine, "state1", &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, EGG_STATE_TRANSITION_SUCCESS);
  g_assert_cmpint (dummy->obj1_count, ==, 0);
  g_assert_cmpint (dummy->obj2_count, ==, 0);

  g_signal_emit_by_name (obj1, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_assert_cmpint (dummy->obj2_count, ==, 0);

  g_signal_emit_by_name (obj2, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_assert_cmpint (dummy->obj2_count, ==, 0);

  ret = egg_state_machine_transition (machine, "state2", &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, EGG_STATE_TRANSITION_SUCCESS);

  g_signal_emit_by_name (obj1, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_assert_cmpint (dummy->obj2_count, ==, 0);

  g_signal_emit_by_name (obj2, "frobnicate");
  g_assert_cmpint (dummy->obj1_count, ==, 1);
  g_assert_cmpint (dummy->obj2_count, ==, 1);

  g_object_set (obj2, "string", "obj2", NULL);
  g_object_set (obj1, "string", "obj1", NULL);
  assert_prop_equal (obj2, dummy, "string");

  /* state2 -> state3 should fail */
  ret = egg_state_machine_transition (machine, "state3", &error);
  g_assert (ret == EGG_STATE_TRANSITION_INVALID);
  g_assert (error != NULL);
  g_clear_error (&error);

  ret = egg_state_machine_transition (machine, "state1", &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, EGG_STATE_TRANSITION_SUCCESS);

  assert_prop_equal (obj1, dummy, "string");
  g_object_set (obj1, "string", "obj1-1", NULL);
  assert_prop_equal (obj1, dummy, "string");
  g_object_set (obj2, "string", "obj2-1", NULL);
  assert_prop_equal (obj1, dummy, "string");

  /* state1 -> state3 should succeed */
  ret = egg_state_machine_transition (machine, "state3", &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, EGG_STATE_TRANSITION_SUCCESS);

  g_object_unref (machine);
  g_assert (machine == NULL);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Egg/StateMachine/basic", test_state_machine_basic);
  return g_test_run ();
}
