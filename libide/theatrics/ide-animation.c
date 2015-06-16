/* ide-animation.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <gobject/gvaluecollector.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <string.h>

#include "ide-animation.h"
#include "ide-frame-source.h"

#define FALLBACK_FRAME_RATE 60

typedef gdouble (*AlphaFunc) (gdouble       offset);
typedef void    (*TweenFunc) (const GValue *begin,
                              const GValue *end,
                              GValue       *value,
                              gdouble       offset);

typedef struct
{
  gboolean    is_child;  /* Does GParamSpec belong to parent widget */
  GParamSpec *pspec;     /* GParamSpec of target property */
  GValue      begin;     /* Begin value in animation */
  GValue      end;       /* End value in animation */
} Tween;


struct _IdeAnimation
{
  GInitiallyUnowned  parent_instance;

  gpointer           target;              /* Target object to animate */
  guint64            begin_msec;          /* Time in which animation started */
  guint              duration_msec;       /* Duration of animation */
  guint              mode;                /* Tween mode */
  gulong             tween_handler;       /* GSource or signal handler */
  gulong             after_paint_handler; /* signal handler */
  gdouble            last_offset;         /* Track our last offset */
  GArray            *tweens;              /* Array of tweens to perform */
  GdkFrameClock     *frame_clock;         /* An optional frame-clock for sync. */
};

G_DEFINE_TYPE (IdeAnimation, ide_animation, G_TYPE_INITIALLY_UNOWNED)

enum {
  PROP_0,
  PROP_DURATION,
  PROP_FRAME_CLOCK,
  PROP_MODE,
  PROP_TARGET,
  LAST_PROP
};


enum {
  TICK,
  LAST_SIGNAL
};


/*
 * Helper macros.
 */
#define LAST_FUNDAMENTAL 64
#define TWEEN(type)                                       \
  static void                                             \
  tween_ ## type (const GValue * begin,                   \
                  const GValue * end,                     \
                  GValue * value,                         \
                  gdouble offset)                         \
  {                                                       \
    g ## type x = g_value_get_ ## type (begin);           \
    g ## type y = g_value_get_ ## type (end);             \
    g_value_set_ ## type (value, x + ((y - x) * offset)); \
  }


/*
 * Globals.
 */
static AlphaFunc   gAlphaFuncs[IDE_ANIMATION_LAST];
static gboolean    gDebug;
static GParamSpec *gParamSpecs[LAST_PROP];
static guint       gSignals[LAST_SIGNAL];
static TweenFunc   gTweenFuncs[LAST_FUNDAMENTAL];


/*
 * Tweeners for basic types.
 */
TWEEN (int);
TWEEN (uint);
TWEEN (long);
TWEEN (ulong);
TWEEN (float);
TWEEN (double);


/**
 * ide_animation_alpha_ease_in_cubic:
 * @offset: (in): The position within the animation; 0.0 to 1.0.
 *
 * An alpha function to transform the offset within the animation.
 * @IDE_ANIMATION_CUBIC means the valu ewill be transformed into
 * cubic acceleration (x * x * x).
 */
static gdouble
ide_animation_alpha_ease_in_cubic (gdouble offset)
{
  return offset * offset * offset;
}


static gdouble
ide_animation_alpha_ease_out_cubic (gdouble offset)
{
  gdouble p = offset - 1.0;

  return p * p * p + 1.0;
}


/**
 * ide_animation_alpha_linear:
 * @offset: (in): The position within the animation; 0.0 to 1.0.
 *
 * An alpha function to transform the offset within the animation.
 * @IDE_ANIMATION_LINEAR means no tranformation will be made.
 *
 * Returns: @offset.
 * Side effects: None.
 */
static gdouble
ide_animation_alpha_linear (gdouble offset)
{
  return offset;
}


/**
 * ide_animation_alpha_ease_in_quad:
 * @offset: (in): The position within the animation; 0.0 to 1.0.
 *
 * An alpha function to transform the offset within the animation.
 * @IDE_ANIMATION_EASE_IN_QUAD means that the value will be transformed
 * into a quadratic acceleration.
 *
 * Returns: A tranformation of @offset.
 * Side effects: None.
 */
static gdouble
ide_animation_alpha_ease_in_quad (gdouble offset)
{
  return offset * offset;
}


/**
 * ide_animation_alpha_ease_out_quad:
 * @offset: (in): The position within the animation; 0.0 to 1.0.
 *
 * An alpha function to transform the offset within the animation.
 * @IDE_ANIMATION_EASE_OUT_QUAD means that the value will be transformed
 * into a quadratic deceleration.
 *
 * Returns: A tranformation of @offset.
 * Side effects: None.
 */
static gdouble
ide_animation_alpha_ease_out_quad (gdouble offset)
{
  return -1.0 * offset * (offset - 2.0);
}


/**
 * ide_animation_alpha_ease_in_out_quad:
 * @offset: (in): The position within the animation; 0.0 to 1.0.
 *
 * An alpha function to transform the offset within the animation.
 * @IDE_ANIMATION_EASE_IN_OUT_QUAD means that the value will be transformed
 * into a quadratic acceleration for the first half, and quadratic
 * deceleration the second half.
 *
 * Returns: A tranformation of @offset.
 * Side effects: None.
 */
static gdouble
ide_animation_alpha_ease_in_out_quad (gdouble offset)
{
  offset *= 2.0;
  if (offset < 1.0)
    return 0.5 * offset * offset;
  offset -= 1.0;
  return -0.5 * (offset * (offset - 2.0) - 1.0);
}


/**
 * ide_animation_load_begin_values:
 * @animation: (in): A #IdeAnimation.
 *
 * Load the begin values for all the properties we are about to
 * animate.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ide_animation_load_begin_values (IdeAnimation *animation)
{
  GtkContainer *container;
  Tween *tween;
  gint i;

  g_return_if_fail (IDE_IS_ANIMATION (animation));

  for (i = 0; i < animation->tweens->len; i++)
    {
      tween = &g_array_index (animation->tweens, Tween, i);
      g_value_reset (&tween->begin);
      if (tween->is_child)
        {
          container = GTK_CONTAINER (gtk_widget_get_parent (animation->target));
          gtk_container_child_get_property (container,
                                            animation->target,
                                            tween->pspec->name,
                                            &tween->begin);
        }
      else
        {
          g_object_get_property (animation->target,
                                 tween->pspec->name,
                                 &tween->begin);
        }
    }
}


/**
 * ide_animation_unload_begin_values:
 * @animation: (in): A #IdeAnimation.
 *
 * Unloads the begin values for the animation. This might be particularly
 * useful once we support pointer types.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ide_animation_unload_begin_values (IdeAnimation *animation)
{
  Tween *tween;
  gint i;

  g_return_if_fail (IDE_IS_ANIMATION (animation));

  for (i = 0; i < animation->tweens->len; i++)
    {
      tween = &g_array_index (animation->tweens, Tween, i);
      g_value_reset (&tween->begin);
    }
}


/**
 * ide_animation_get_offset:
 * @animation: A #IdeAnimation.
 * @frame_time: the time to present the frame, or 0 for current timing.
 *
 * Retrieves the position within the animation from 0.0 to 1.0. This
 * value is calculated using the msec of the beginning of the animation
 * and the current time.
 *
 * Returns: The offset of the animation from 0.0 to 1.0.
 */
static gdouble
ide_animation_get_offset (IdeAnimation *animation,
                          gint64        frame_time)
{
  gdouble offset;
  gint64 frame_msec;

  g_return_val_if_fail (IDE_IS_ANIMATION (animation), 0.0);

  if (frame_time == 0)
    {
      if (animation->frame_clock != NULL)
        frame_time = gdk_frame_clock_get_frame_time (animation->frame_clock);
      else
        frame_time = g_get_monotonic_time ();
    }

  frame_msec = frame_time / 1000L;

  offset = (gdouble) (frame_msec - animation->begin_msec) /
           (gdouble) MAX (animation->duration_msec, 1);

  return CLAMP (offset, 0.0, 1.0);
}


/**
 * ide_animation_update_property:
 * @animation: (in): A #IdeAnimation.
 * @target: (in): A #GObject.
 * @tween: (in): a #Tween containing the property.
 * @value: (in): The new value for the property.
 *
 * Updates the value of a property on an object using @value.
 *
 * Returns: None.
 * Side effects: The property of @target is updated.
 */
static void
ide_animation_update_property (IdeAnimation  *animation,
                              gpointer      target,
                              Tween        *tween,
                              const GValue *value)
{
  g_assert (IDE_IS_ANIMATION (animation));
  g_assert (G_IS_OBJECT (target));
  g_assert (tween);
  g_assert (value);

  g_object_set_property (target, tween->pspec->name, value);
}


/**
 * ide_animation_update_child_property:
 * @animation: (in): A #IdeAnimation.
 * @target: (in): A #GObject.
 * @tween: (in): A #Tween containing the property.
 * @value: (in): The new value for the property.
 *
 * Updates the value of the parent widget of the target to @value.
 *
 * Returns: None.
 * Side effects: The property of @target<!-- -->'s parent widget is updated.
 */
static void
ide_animation_update_child_property (IdeAnimation *animation,
                                     gpointer      target,
                                     Tween        *tween,
                                     const GValue *value)
{
  GtkWidget *parent;

  g_assert (IDE_IS_ANIMATION (animation));
  g_assert (G_IS_OBJECT (target));
  g_assert (tween);
  g_assert (value);

  parent = gtk_widget_get_parent (GTK_WIDGET (target));
  gtk_container_child_set_property (GTK_CONTAINER (parent),
                                    target,
                                    tween->pspec->name,
                                    value);
}


/**
 * ide_animation_get_value_at_offset:
 * @animation: (in): A #IdeAnimation.
 * @offset: (in): The offset in the animation from 0.0 to 1.0.
 * @tween: (in): A #Tween containing the property.
 * @value: (out): A #GValue in which to store the property.
 *
 * Retrieves a value for a particular position within the animation.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ide_animation_get_value_at_offset (IdeAnimation *animation,
                                   gdouble       offset,
                                   Tween        *tween,
                                   GValue       *value)
{
  g_return_if_fail (IDE_IS_ANIMATION (animation));
  g_return_if_fail (tween != NULL);
  g_return_if_fail (value != NULL);
  g_return_if_fail (value->g_type == tween->pspec->value_type);

  if (value->g_type < LAST_FUNDAMENTAL)
    {
      /*
       * If you hit the following assertion, you need to add a function
       * to create the new value at the given offset.
       */
      g_assert (gTweenFuncs[value->g_type]);
      gTweenFuncs[value->g_type](&tween->begin, &tween->end, value, offset);
    }
  else
    {
      /*
       * TODO: Support complex transitions.
       */
      if (offset >= 1.0)
        g_value_copy (&tween->end, value);
    }
}

static void
ide_animation_set_frame_clock (IdeAnimation  *animation,
                               GdkFrameClock *frame_clock)
{
  if (animation->frame_clock != frame_clock)
    {
      g_clear_object (&animation->frame_clock);
      animation->frame_clock = frame_clock ? g_object_ref (frame_clock) : NULL;
    }
}

static void
ide_animation_set_target (IdeAnimation *animation,
                          gpointer      target)
{
  g_assert (!animation->target);

  animation->target = g_object_ref (target);

  if (GTK_IS_WIDGET (animation->target))
    ide_animation_set_frame_clock (animation,
                                  gtk_widget_get_frame_clock (animation->target));
}


/**
 * ide_animation_tick:
 * @animation: (in): A #IdeAnimation.
 *
 * Moves the object properties to the next position in the animation.
 *
 * Returns: %TRUE if the animation has not completed; otherwise %FALSE.
 * Side effects: None.
 */
static gboolean
ide_animation_tick (IdeAnimation *animation,
                    gdouble       offset)
{
  gdouble alpha;
  GValue value = { 0 };
  Tween *tween;
  gint i;

  g_return_val_if_fail (IDE_IS_ANIMATION (animation), FALSE);

  if (offset == animation->last_offset)
    return offset < 1.0;

  alpha = gAlphaFuncs[animation->mode](offset);

  /*
   * Update property values.
   */
  for (i = 0; i < animation->tweens->len; i++)
    {
      tween = &g_array_index (animation->tweens, Tween, i);
      g_value_init (&value, tween->pspec->value_type);
      ide_animation_get_value_at_offset (animation, alpha, tween, &value);
      if (!tween->is_child)
        {
          ide_animation_update_property (animation,
                                        animation->target,
                                        tween,
                                        &value);
        }
      else
        {
          ide_animation_update_child_property (animation,
                                              animation->target,
                                              tween,
                                              &value);
        }
      g_value_unset (&value);
    }

  /*
   * Notify anyone interested in the tick signal.
   */
  g_signal_emit (animation, gSignals[TICK], 0);

  /*
   * Flush any outstanding events to the graphics server (in the case of X).
   */
#if !GTK_CHECK_VERSION (3, 13, 0)
  if (GTK_IS_WIDGET (animation->target))
    {
      GdkWindow *window;

      if ((window = gtk_widget_get_window (GTK_WIDGET (animation->target))))
        gdk_window_flush (window);
    }
#endif

  animation->last_offset = offset;

  return offset < 1.0;
}


/**
 * ide_animation_timeout_cb:
 * @user_data: (in): A #IdeAnimation.
 *
 * Timeout from the main loop to move to the next step of the animation.
 *
 * Returns: %TRUE until the animation has completed; otherwise %FALSE.
 * Side effects: None.
 */
static gboolean
ide_animation_timeout_cb (gpointer user_data)
{
  IdeAnimation *animation = user_data;
  gboolean ret;
  gdouble offset;

  offset = ide_animation_get_offset (animation, 0);

  if (!(ret = ide_animation_tick (animation, offset)))
    ide_animation_stop (animation);

  return ret;
}


static gboolean
ide_animation_widget_tick_cb (GdkFrameClock *frame_clock,
                              IdeAnimation  *animation)
{
  gboolean ret = G_SOURCE_REMOVE;

  g_assert (GDK_IS_FRAME_CLOCK (frame_clock));
  g_assert (IDE_IS_ANIMATION (animation));

  if (animation->tween_handler)
    {
      gdouble offset;

      offset = ide_animation_get_offset (animation, 0);

      if (!(ret = ide_animation_tick (animation, offset)))
        ide_animation_stop (animation);
    }

  return ret;
}


static void
ide_animation_widget_after_paint_cb (GdkFrameClock *frame_clock,
                                     IdeAnimation  *animation)
{
  gint64 base_time;
  gint64 interval;
  gint64 next_frame_time;
  gdouble offset;

  g_assert (GDK_IS_FRAME_CLOCK (frame_clock));
  g_assert (IDE_IS_ANIMATION (animation));

  base_time = gdk_frame_clock_get_frame_time (frame_clock);
  gdk_frame_clock_get_refresh_info (frame_clock, base_time, &interval, &next_frame_time);

  offset = ide_animation_get_offset (animation, next_frame_time);

  ide_animation_tick (animation, offset);
}


/**
 * ide_animation_start:
 * @animation: (in): A #IdeAnimation.
 *
 * Start the animation. When the animation stops, the internal reference will
 * be dropped and the animation may be finalized.
 *
 * Returns: None.
 * Side effects: None.
 */
void
ide_animation_start (IdeAnimation *animation)
{
  g_return_if_fail (IDE_IS_ANIMATION (animation));
  g_return_if_fail (!animation->tween_handler);

  g_object_ref_sink (animation);
  ide_animation_load_begin_values (animation);

  if (animation->frame_clock)
    {
      animation->begin_msec = gdk_frame_clock_get_frame_time (animation->frame_clock) / 1000UL;
      animation->tween_handler =
        g_signal_connect (animation->frame_clock,
                          "update",
                          G_CALLBACK (ide_animation_widget_tick_cb),
                          animation);
      animation->after_paint_handler =
        g_signal_connect (animation->frame_clock,
                          "after-paint",
                          G_CALLBACK (ide_animation_widget_after_paint_cb),
                          animation);
      gdk_frame_clock_begin_updating (animation->frame_clock);
    }
  else
    {
      animation->begin_msec = g_get_monotonic_time () / 1000UL;
      animation->tween_handler = ide_frame_source_add (FALLBACK_FRAME_RATE,
                                                       ide_animation_timeout_cb,
                                                       animation);
    }
}


/**
 * ide_animation_stop:
 * @animation: (in): A #IdeAnimation.
 *
 * Stops a running animation. The internal reference to the animation is
 * dropped and therefore may cause the object to finalize.
 *
 * Returns: None.
 * Side effects: None.
 */
void
ide_animation_stop (IdeAnimation *animation)
{
  g_return_if_fail (IDE_IS_ANIMATION (animation));

  if (animation->tween_handler)
    {
      if (animation->frame_clock)
        {
          gdk_frame_clock_end_updating (animation->frame_clock);
          g_signal_handler_disconnect (animation->frame_clock, animation->tween_handler);
          g_signal_handler_disconnect (animation->frame_clock, animation->after_paint_handler);
          animation->tween_handler = 0;
        }
      else
        {
          g_source_remove (animation->tween_handler);
          animation->tween_handler = 0;
        }
      ide_animation_unload_begin_values (animation);
      g_object_unref (animation);
    }
}


/**
 * ide_animation_add_property:
 * @animation: (in): A #IdeAnimation.
 * @pspec: (in): A #ParamSpec of @target or a #GtkWidget<!-- -->'s parent.
 * @value: (in): The new value for the property at the end of the animation.
 *
 * Adds a new property to the set of properties to be animated during the
 * lifetime of the animation.
 *
 * Returns: None.
 * Side effects: None.
 */
void
ide_animation_add_property (IdeAnimation *animation,
                            GParamSpec   *pspec,
                            const GValue *value)
{
  Tween tween = { 0 };
  GType type;

  g_return_if_fail (IDE_IS_ANIMATION (animation));
  g_return_if_fail (pspec != NULL);
  g_return_if_fail (value != NULL);
  g_return_if_fail (value->g_type);
  g_return_if_fail (animation->target);
  g_return_if_fail (!animation->tween_handler);

  type = G_TYPE_FROM_INSTANCE (animation->target);
  tween.is_child = !g_type_is_a (type, pspec->owner_type);
  if (tween.is_child)
    {
      if (!GTK_IS_WIDGET (animation->target))
        {
          g_critical (_("Cannot locate property %s in class %s"),
                      pspec->name, g_type_name (type));
          return;
        }
    }

  tween.pspec = g_param_spec_ref (pspec);
  g_value_init (&tween.begin, pspec->value_type);
  g_value_init (&tween.end, pspec->value_type);
  g_value_copy (value, &tween.end);
  g_array_append_val (animation->tweens, tween);
}


/**
 * ide_animation_dispose:
 * @object: (in): A #IdeAnimation.
 *
 * Releases any object references the animation contains.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ide_animation_dispose (GObject *object)
{
  IdeAnimation *self = IDE_ANIMATION (object);

  g_clear_object (&self->target);
  g_clear_object (&self->frame_clock);

  G_OBJECT_CLASS (ide_animation_parent_class)->dispose (object);
}


/**
 * ide_animation_finalize:
 * @object: (in): A #IdeAnimation.
 *
 * Finalizes the object and releases any resources allocated.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ide_animation_finalize (GObject *object)
{
  IdeAnimation *self = IDE_ANIMATION (object);
  Tween *tween;
  gint i;

  for (i = 0; i < self->tweens->len; i++)
    {
      tween = &g_array_index (self->tweens, Tween, i);
      g_value_unset (&tween->begin);
      g_value_unset (&tween->end);
      g_param_spec_unref (tween->pspec);
    }

  g_array_unref (self->tweens);

  G_OBJECT_CLASS (ide_animation_parent_class)->finalize (object);
}


/**
 * ide_animation_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
ide_animation_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeAnimation *animation = IDE_ANIMATION (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      animation->duration_msec = g_value_get_uint (value);
      break;

    case PROP_FRAME_CLOCK:
      ide_animation_set_frame_clock (animation, g_value_get_object (value));
      break;

    case PROP_MODE:
      animation->mode = g_value_get_enum (value);
      break;

    case PROP_TARGET:
      ide_animation_set_target (animation, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


/**
 * ide_animation_class_init:
 * @klass: (in): A #IdeAnimationClass.
 *
 * Initializes the GObjectClass.
 *
 * Returns: None.
 * Side effects: Properties, signals, and vtables are initialized.
 */
static void
ide_animation_class_init (IdeAnimationClass *klass)
{
  GObjectClass *object_class;

  gDebug = !!g_getenv ("IDE_ANIMATION_DEBUG");

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = ide_animation_dispose;
  object_class->finalize = ide_animation_finalize;
  object_class->set_property = ide_animation_set_property;

  /**
   * IdeAnimation:duration:
   *
   * The "duration" property is the total number of milliseconds that the
   * animation should run before being completed.
   */
  gParamSpecs[PROP_DURATION] =
    g_param_spec_uint ("duration",
                       _("Duration"),
                       _("The duration of the animation"),
                       0,
                       G_MAXUINT,
                       250,
                       (G_PARAM_WRITABLE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));

  gParamSpecs[PROP_FRAME_CLOCK] =
    g_param_spec_object ("frame-clock",
                         _("Frame Clock"),
                         _("An optional frame-clock to synchronize with."),
                         GDK_TYPE_FRAME_CLOCK,
                         (G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * IdeAnimation:mode:
   *
   * The "mode" property is the Alpha function that should be used to
   * determine the offset within the animation based on the current
   * offset in the animations duration.
   */
  gParamSpecs[PROP_MODE] =
    g_param_spec_enum ("mode",
                       _("Mode"),
                       _("The animation mode"),
                       IDE_TYPE_ANIMATION_MODE,
                       IDE_ANIMATION_LINEAR,
                       (G_PARAM_WRITABLE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * IdeAnimation:target:
   *
   * The "target" property is the #GObject that should have it's properties
   * animated.
   */
  gParamSpecs[PROP_TARGET] =
    g_param_spec_object ("target",
                         _("Target"),
                         _("The target of the animation"),
                         G_TYPE_OBJECT,
                         (G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  /**
   * IdeAnimation::tick:
   *
   * The "tick" signal is emitted on each frame in the animation.
   */
  gSignals[TICK] = g_signal_new ("tick",
                                 IDE_TYPE_ANIMATION,
                                 G_SIGNAL_RUN_FIRST,
                                 0,
                                 NULL, NULL, NULL,
                                 G_TYPE_NONE,
                                 0);

#define SET_ALPHA(_T, _t) \
  gAlphaFuncs[IDE_ANIMATION_ ## _T] = ide_animation_alpha_ ## _t

  SET_ALPHA (LINEAR, linear);
  SET_ALPHA (EASE_IN_QUAD, ease_in_quad);
  SET_ALPHA (EASE_OUT_QUAD, ease_out_quad);
  SET_ALPHA (EASE_IN_OUT_QUAD, ease_in_out_quad);
  SET_ALPHA (EASE_IN_CUBIC, ease_in_cubic);
  SET_ALPHA (EASE_OUT_CUBIC, ease_out_cubic);

#define SET_TWEEN(_T, _t) \
  G_STMT_START { \
    guint idx = G_TYPE_ ## _T; \
    gTweenFuncs[idx] = tween_ ## _t; \
  } G_STMT_END

  SET_TWEEN (INT, int);
  SET_TWEEN (UINT, uint);
  SET_TWEEN (LONG, long);
  SET_TWEEN (ULONG, ulong);
  SET_TWEEN (FLOAT, float);
  SET_TWEEN (DOUBLE, double);
}


/**
 * ide_animation_init:
 * @animation: (in): A #IdeAnimation.
 *
 * Initializes the #IdeAnimation instance.
 *
 * Returns: None.
 * Side effects: Everything.
 */
static void
ide_animation_init (IdeAnimation *animation)
{
  animation->duration_msec = 250;
  animation->mode = IDE_ANIMATION_EASE_IN_OUT_QUAD;
  animation->tweens = g_array_new (FALSE, FALSE, sizeof (Tween));
  animation->last_offset = -G_MINDOUBLE;
}


/**
 * ide_animation_mode_get_type:
 *
 * Retrieves the GType for #IdeAnimationMode.
 *
 * Returns: A GType.
 * Side effects: GType registered on first call.
 */
GType
ide_animation_mode_get_type (void)
{
  static GType type_id = 0;
  static const GEnumValue values[] = {
    { IDE_ANIMATION_LINEAR, "IDE_ANIMATION_LINEAR", "LINEAR" },
    { IDE_ANIMATION_EASE_IN_QUAD, "IDE_ANIMATION_EASE_IN_QUAD", "EASE_IN_QUAD" },
    { IDE_ANIMATION_EASE_IN_OUT_QUAD, "IDE_ANIMATION_EASE_IN_OUT_QUAD", "EASE_IN_OUT_QUAD" },
    { IDE_ANIMATION_EASE_OUT_QUAD, "IDE_ANIMATION_EASE_OUT_QUAD", "EASE_OUT_QUAD" },
    { IDE_ANIMATION_EASE_IN_CUBIC, "IDE_ANIMATION_EASE_IN_CUBIC", "EASE_IN_CUBIC" },
    { IDE_ANIMATION_EASE_OUT_CUBIC, "IDE_ANIMATION_EASE_OUT_CUBIC", "EASE_OUT_CUBIC" },
    { 0 }
  };

  if (G_UNLIKELY (!type_id))
    type_id = g_enum_register_static ("IdeAnimationMode", values);
  return type_id;
}

/**
 * ide_object_animatev:
 * @object: A #GObject.
 * @mode: The animation mode.
 * @duration_msec: The duration in milliseconds.
 * @frame_rate: The target frame rate.
 * @first_property: The first property to animate.
 * @args: A variadac list of arguments
 *
 * Returns: (transfer none): A #IdeAnimation.
 */
IdeAnimation *
ide_object_animatev (gpointer          object,
                     IdeAnimationMode  mode,
                     guint             duration_msec,
                     GdkFrameClock    *frame_clock,
                     const gchar      *first_property,
                     va_list           args)
{
  IdeAnimation *animation;
  GObjectClass *klass;
  GObjectClass *pklass;
  const gchar *name;
  GParamSpec *pspec;
  GtkWidget *parent;
  GValue value = { 0 };
  gchar *error = NULL;
  GType type;
  GType ptype;
  gboolean enable_animations;

  g_return_val_if_fail (first_property != NULL, NULL);
  g_return_val_if_fail (mode < IDE_ANIMATION_LAST, NULL);

  if ((frame_clock != NULL) && GTK_IS_WIDGET (object))
    frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (object));

  /*
   * If we have a frame clock, then we must be in the gtk thread and we
   * should check GtkSettings for disabled animations. If we are disabled,
   * we will just make the timeout immediate.
   */
  if (frame_clock != NULL)
    {
      g_object_get (gtk_settings_get_default (),
                    "gtk-enable-animations", &enable_animations,
                    NULL);

      if (enable_animations == FALSE)
        duration_msec = 0;
    }

  name = first_property;
  type = G_TYPE_FROM_INSTANCE (object);
  klass = G_OBJECT_GET_CLASS (object);
  animation = g_object_new (IDE_TYPE_ANIMATION,
                            "duration", duration_msec,
                            "frame-clock", frame_clock,
                            "mode", mode,
                            "target", object,
                            NULL);

  do
    {
      /*
       * First check for the property on the object. If that does not exist
       * then check if the object has a parent and look at its child
       * properties (if its a GtkWidget).
       */
      if (!(pspec = g_object_class_find_property (klass, name)))
        {
          if (!g_type_is_a (type, GTK_TYPE_WIDGET))
            {
              g_critical (_("Failed to find property %s in %s"),
                          name, g_type_name (type));
              goto failure;
            }
          if (!(parent = gtk_widget_get_parent (object)))
            {
              g_critical (_("Failed to find property %s in %s"),
                          name, g_type_name (type));
              goto failure;
            }
          pklass = G_OBJECT_GET_CLASS (parent);
          ptype = G_TYPE_FROM_INSTANCE (parent);
          if (!(pspec = gtk_container_class_find_child_property (pklass, name)))
            {
              g_critical (_("Failed to find property %s in %s or parent %s"),
                          name, g_type_name (type), g_type_name (ptype));
              goto failure;
            }
        }

      g_value_init (&value, pspec->value_type);
      G_VALUE_COLLECT (&value, args, 0, &error);
      if (error != NULL)
        {
          g_critical (_("Failed to retrieve va_list value: %s"), error);
          g_free (error);
          goto failure;
        }

      ide_animation_add_property (animation, pspec, &value);
      g_value_unset (&value);
    }
  while ((name = va_arg (args, const gchar *)));

  ide_animation_start (animation);

  return animation;

failure:
  g_object_ref_sink (animation);
  g_object_unref (animation);
  return NULL;
}

/**
 * ide_object_animate:
 * @object: (in): A #GObject.
 * @mode: (in): The animation mode.
 * @duration_msec: (in): The duration in milliseconds.
 * @first_property: (in): The first property to animate.
 *
 * Animates the properties of @object. The can be set in a similar manner to g_object_set(). They
 * will be animated from their current value to the target value over the time period.
 *
 * Return value: (transfer none): A #IdeAnimation.
 * Side effects: None.
 */
IdeAnimation*
ide_object_animate (gpointer        object,
                    IdeAnimationMode mode,
                    guint           duration_msec,
                    GdkFrameClock  *frame_clock,
                    const gchar    *first_property,
                    ...)
{
  IdeAnimation *animation;
  va_list args;

  va_start (args, first_property);
  animation = ide_object_animatev (object, mode, duration_msec, frame_clock, first_property, args);
  va_end (args);
  return animation;
}

/**
 * ide_object_animate_full:
 *
 * Return value: (transfer none): A #IdeAnimation.
 */
IdeAnimation*
ide_object_animate_full (gpointer        object,
                         IdeAnimationMode mode,
                         guint           duration_msec,
                         GdkFrameClock  *frame_clock,
                         GDestroyNotify  notify,
                         gpointer        notify_data,
                         const gchar    *first_property,
                         ...)
{
  IdeAnimation *animation;
  va_list args;

  va_start (args, first_property);
  animation = ide_object_animatev (object,
                                  mode,
                                  duration_msec,
                                  frame_clock,
                                  first_property,
                                  args);
  va_end (args);

  g_object_weak_ref (G_OBJECT (animation), (GWeakNotify) notify, notify_data);

  return animation;
}
