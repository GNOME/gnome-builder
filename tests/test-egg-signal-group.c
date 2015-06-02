
#define G_LOG_DOMAIN "egg-signal-group"

#include "egg-signal-group.h"

typedef struct _SignalTarget
{
  GObject parent_instance;
} SignalTarget;

G_DECLARE_FINAL_TYPE (SignalTarget, signal_target, TEST, SIGNAL_TARGET, GObject)
G_DEFINE_TYPE (SignalTarget, signal_target, G_TYPE_OBJECT)

enum {
  THE_SIGNAL,
  NEVER_EMITTED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

static void
signal_target_class_init (SignalTargetClass *klass)
{
  gSignals [THE_SIGNAL] =
    g_signal_new ("the-signal",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_OBJECT);

  gSignals [NEVER_EMITTED] =
    g_signal_new ("never-emitted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_OBJECT);
}

static void
signal_target_init (SignalTarget *self)
{
}

static gint global_signal_calls;
static gint global_weak_notify_called;

static void
connect_before_cb (SignalTarget   *target,
                   EggSignalGroup *group,
                   gint           *signal_calls)
{
  g_assert (TEST_IS_SIGNAL_TARGET (target));
  g_assert (EGG_IS_SIGNAL_GROUP (group));
  g_assert (egg_signal_group_get_target (group) == (GObject *)target);
  g_assert (signal_calls != NULL);
  g_assert (signal_calls == &global_signal_calls);

  *signal_calls += 1;
}

static void
connect_after_cb (SignalTarget   *target,
                  EggSignalGroup *group,
                  gint           *signal_calls)
{
  g_assert (TEST_IS_SIGNAL_TARGET (target));
  g_assert (EGG_IS_SIGNAL_GROUP (group));
  g_assert (egg_signal_group_get_target (group) == (GObject *)target);
  g_assert (signal_calls != NULL);
  g_assert (signal_calls == &global_signal_calls);

  g_assert_cmpint (*signal_calls, ==, 3);
  *signal_calls += 1;
}

static void
connect_swapped_cb (gint           *signal_calls,
                    EggSignalGroup *group,
                    SignalTarget   *target)
{
  g_assert (signal_calls != NULL);
  g_assert (signal_calls == &global_signal_calls);
  g_assert (EGG_IS_SIGNAL_GROUP (group));
  g_assert (TEST_IS_SIGNAL_TARGET (target));
  g_assert (egg_signal_group_get_target (group) == (GObject *)target);

  *signal_calls += 1;
}

static void
connect_object_cb (SignalTarget   *target,
                   EggSignalGroup *group,
                   GObject        *object)
{
  gint *signal_calls;

  g_assert (TEST_IS_SIGNAL_TARGET (target));
  g_assert (EGG_IS_SIGNAL_GROUP (group));
  g_assert (egg_signal_group_get_target (group) == (GObject *)target);
  g_assert (G_IS_OBJECT (object));

  signal_calls = g_object_get_data (object, "signal-calls");
  g_assert (signal_calls != NULL);
  g_assert (signal_calls == &global_signal_calls);

  *signal_calls += 1;
}

static void
connect_never_emitted_cb (SignalTarget *target,
                          gboolean     *weak_notify_called)
{
  g_error ("This signal is never emitted!");
}

static void
connect_data_notify_cb (gboolean *weak_notify_called,
                        GClosure *closure)
{
  g_assert (weak_notify_called != NULL);
  g_assert (weak_notify_called == &global_weak_notify_called);
  g_assert (closure != NULL);

  g_assert_false (*weak_notify_called);
  *weak_notify_called = TRUE;
}

static void
connect_data_weak_notify_cb (gboolean       *weak_notify_called,
                             EggSignalGroup *group)
{
  g_assert (weak_notify_called != NULL);
  g_assert (weak_notify_called == &global_weak_notify_called);
  g_assert (EGG_IS_SIGNAL_GROUP (group));

  g_assert_true (*weak_notify_called);
}

static void
connect_all_signals (EggSignalGroup *group)
{
  GObject *object;

  /* Check that these are called in the right order */
  egg_signal_group_connect (group,
                            "the-signal",
                            G_CALLBACK (connect_before_cb),
                            &global_signal_calls);
  egg_signal_group_connect_after (group,
                                  "the-signal",
                                  G_CALLBACK (connect_after_cb),
                                  &global_signal_calls);

  /* Check that this is called with the arguments swapped */
  egg_signal_group_connect_swapped (group,
                                    "the-signal",
                                    G_CALLBACK (connect_swapped_cb),
                                    &global_signal_calls);

  /* Check that this is called with the arguments swapped */
  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_data (object, "signal-calls", &global_signal_calls);
  egg_signal_group_connect_object (group,
                                   "the-signal",
                                   G_CALLBACK (connect_object_cb),
                                   object,
                                   0);
  g_object_weak_ref (G_OBJECT (group),
                     (GWeakNotify)g_object_unref,
                     object);


  /* Check that the notify is called correctly */
  global_weak_notify_called = FALSE;
  egg_signal_group_connect_data (group,
                                 "never-emitted",
                                 G_CALLBACK (connect_never_emitted_cb),
                                 &global_weak_notify_called,
                                 (GClosureNotify)connect_data_notify_cb,
                                 0);
  g_object_weak_ref (G_OBJECT (group),
                     (GWeakNotify)connect_data_weak_notify_cb,
                     &global_weak_notify_called);
}

static void
assert_signals (SignalTarget   *target,
                EggSignalGroup *group,
                gboolean        success)
{
  g_assert (TEST_IS_SIGNAL_TARGET (target));
  g_assert (group == NULL || EGG_IS_SIGNAL_GROUP (group));

  global_signal_calls = 0;
  g_signal_emit (target, gSignals [THE_SIGNAL], 0, group);
  g_assert_cmpint (global_signal_calls, ==, success ? 4 : 0);
}

static void
test_signal_group_invalid (void)
{
  GObject *invalid_target = g_object_new (G_TYPE_OBJECT, NULL);
  SignalTarget *target = g_object_new (signal_target_get_type (), NULL);
  EggSignalGroup *group = egg_signal_group_new (signal_target_get_type ());

  /* Invalid Target Type */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*g_type_is_a*G_TYPE_OBJECT*");
  egg_signal_group_new (G_TYPE_DATE_TIME);
  g_test_assert_expected_messages ();

  /* Invalid Target */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*Failed to set EggSignalGroup of target "
                         "type SignalTarget using target * of type GObject*");
  egg_signal_group_set_target (group, invalid_target);
  g_test_assert_expected_messages ();

  /* Invalid Signal Name */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*g_signal_parse_name*");
  egg_signal_group_connect (group,
                            "does-not-exist",
                            G_CALLBACK (connect_before_cb),
                            NULL);
  g_test_assert_expected_messages ();

  /* Invalid Callback */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*callback != NULL*");
  egg_signal_group_connect (group,
                            "the-signal",
                            G_CALLBACK (NULL),
                            NULL);
  g_test_assert_expected_messages ();

  g_object_unref (group);
  g_object_unref (target);
  g_object_unref (invalid_target);
}

static void
test_signal_group_simple (void)
{
  SignalTarget *target = g_object_new (signal_target_get_type (), NULL);
  EggSignalGroup *group = egg_signal_group_new (signal_target_get_type ());

  /* Set the target before connecting the signals */
  g_assert_null (egg_signal_group_get_target (group));
  egg_signal_group_set_target (group, target);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target);

  connect_all_signals (group);
  assert_signals (target, group, TRUE);

  /* Destroying the SignalGroup should disconnect the signals */
  g_object_unref (group);
  assert_signals (target, NULL, FALSE);

  g_object_unref (target);
}

static void
test_signal_group_changing_target (void)
{
  SignalTarget *target1, *target2;
  EggSignalGroup *group = egg_signal_group_new (signal_target_get_type ());

  connect_all_signals (group);
  g_assert_null (egg_signal_group_get_target (group));

  /* Set the target after connecting the signals */
  target1 = g_object_new (signal_target_get_type (), NULL);
  egg_signal_group_set_target (group, target1);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target1);

  assert_signals (target1, group, TRUE);

  /* Set the same target */
  g_assert (egg_signal_group_get_target (group) == (GObject *)target1);
  egg_signal_group_set_target (group, target1);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target1);

  assert_signals (target1, group, TRUE);

  /* Set a new target when the current target is non-NULL */
  target2 = g_object_new (signal_target_get_type (), NULL);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target1);
  egg_signal_group_set_target (group, target2);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target2);

  assert_signals (target2, group, TRUE);

  g_object_unref (target2);
  g_object_unref (target1);
  g_object_unref (group);
}

static void
assert_blocking (SignalTarget   *target,
                 EggSignalGroup *group,
                 gint            count)
{
  gint i;

  assert_signals (target, group, TRUE);

  /* Assert that multiple blocks are effective */
  for (i = 0; i < count; ++i)
    {
      egg_signal_group_block (group);
      assert_signals (target, group, FALSE);
    }

  /* Assert the signal is not emitted after the first unblock */
  for (i = 0; i < count; ++i)
    {
      assert_signals (target, group, FALSE);
      egg_signal_group_unblock (group);
    }

  assert_signals (target, group, TRUE);
}

static void
test_signal_group_blocking (void)
{
  SignalTarget *target1, *target2;
  EggSignalGroup *group = egg_signal_group_new (signal_target_get_type ());

  connect_all_signals (group);
  g_assert_null (egg_signal_group_get_target (group));

  target1 = g_object_new (signal_target_get_type (), NULL);
  egg_signal_group_set_target (group, target1);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target1);

  assert_blocking (target1, group, 1);
  assert_blocking (target1, group, 3);
  assert_blocking (target1, group, 15);

  /* Assert that blocking transfers across changing the target */
  egg_signal_group_block (group);
  egg_signal_group_block (group);

  /* Set a new target when the current target is non-NULL */
  target2 = g_object_new (signal_target_get_type (), NULL);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target1);
  egg_signal_group_set_target (group, target2);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target2);

  assert_signals (target2, group, FALSE);
  egg_signal_group_unblock (group);
  assert_signals (target2, group, FALSE);
  egg_signal_group_unblock (group);
  assert_signals (target2, group, TRUE);

  g_object_unref (target2);
  g_object_unref (target1);
  g_object_unref (group);
}

static void
test_signal_group_weak_ref_target (void)
{
  SignalTarget *target = g_object_new (signal_target_get_type (), NULL);
  EggSignalGroup *group = egg_signal_group_new (signal_target_get_type ());

  g_assert_null (egg_signal_group_get_target (group));
  egg_signal_group_set_target (group, target);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target);

  g_object_add_weak_pointer (G_OBJECT (target), (gpointer)&target);
  g_object_unref (target);
  g_assert_null (target);
  g_assert_null (egg_signal_group_get_target (group));

  g_object_unref (group);
}

static void
test_signal_group_connect_object (void)
{
  GObject *object = g_object_new (G_TYPE_OBJECT, NULL);
  SignalTarget *target = g_object_new (signal_target_get_type (), NULL);
  EggSignalGroup *group = egg_signal_group_new (signal_target_get_type ());

  /* We already do basic connect_object() tests in connect_signals(),
   * this is only needed to test the specifics of connect_object()
   */
  egg_signal_group_connect_object (group,
                                   "the-signal",
                                   G_CALLBACK (connect_object_cb),
                                   object,
                                   0);

  g_assert_null (egg_signal_group_get_target (group));
  egg_signal_group_set_target (group, target);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target);

  g_object_add_weak_pointer (G_OBJECT (object), (gpointer)&object);
  g_object_unref (object);
  g_assert_null (object);

  /* This would cause a warning if the SignalGroup did not
   * have a weakref on the object as it would try to connect again
   */
  egg_signal_group_set_target (group, NULL);
  g_assert (egg_signal_group_get_target (group) == (GObject *)NULL);
  egg_signal_group_set_target (group, target);
  g_assert (egg_signal_group_get_target (group) == (GObject *)target);

  g_object_unref (group);
  g_object_unref (target);
}

static void
test_signal_group_signal_parsing (void)
{
  g_test_trap_subprocess ("/Egg/SignalGroup/signal-parsing/subprocess", 0,
                          G_TEST_SUBPROCESS_INHERIT_STDERR);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("");
}

static void
test_signal_group_signal_parsing_subprocess (void)
{
  EggSignalGroup *group;

  /* Check that the class has not been created and with it the
   * signals registered. This will cause g_signal_parse_name()
   * to fail unless EggSignalGroup calls g_type_class_ref().
   */
  g_assert_null (g_type_class_peek (signal_target_get_type ()));

  group = egg_signal_group_new (signal_target_get_type ());
  egg_signal_group_connect (group,
                            "the-signal",
                            G_CALLBACK (connect_before_cb),
                            NULL);

  g_object_unref (group);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Egg/SignalGroup/invalid", test_signal_group_invalid);
  g_test_add_func ("/Egg/SignalGroup/simple", test_signal_group_simple);
  g_test_add_func ("/Egg/SignalGroup/changing-target", test_signal_group_changing_target);
  g_test_add_func ("/Egg/SignalGroup/blocking", test_signal_group_blocking);
  g_test_add_func ("/Egg/SignalGroup/weak-ref-target", test_signal_group_weak_ref_target);
  g_test_add_func ("/Egg/SignalGroup/connect-object", test_signal_group_connect_object);
  g_test_add_func ("/Egg/SignalGroup/signal-parsing", test_signal_group_signal_parsing);
  g_test_add_func ("/Egg/SignalGroup/signal-parsing/subprocess", test_signal_group_signal_parsing_subprocess);
  return g_test_run ();
}
