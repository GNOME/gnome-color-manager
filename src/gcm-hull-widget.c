/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include <gtk/gtk.h>

#ifdef HAVE_CLUTTER
 #include <mash/mash.h>
#endif

#include "gcm-hull.h"
#include "gcm-hull-widget.h"

G_DEFINE_TYPE (GcmHullWidget, gcm_hull_widget, GTK_CLUTTER_TYPE_EMBED);
#define GCM_HULL_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_HULL_WIDGET, GcmHullWidgetPrivate))

struct GcmHullWidgetPrivate
{
	ClutterActor		*stage;
	gboolean		 stage_color;
	gdouble			 existing_rotation;
	gdouble			 old_x;
	gdouble			 old_y;
	GPtrArray		*models;
	guint			 button;
};

static void	gcm_hull_widget_finalize (GObject *object);

enum
{
	PROP_0,
	PROP_STAGE_COLOR,
	PROP_LAST
};

/**
 * gcm_hull_widget_clear:
 **/
void
gcm_hull_widget_clear (GcmHullWidget *hull_widget)
{
	ClutterActor *model;
	guint i;

	for (i=0; i<hull_widget->priv->models->len; i++) {
		model = g_ptr_array_index (hull_widget->priv->models, i);
		clutter_actor_remove_child (hull_widget->priv->stage, model);
	}
	g_ptr_array_set_size (hull_widget->priv->models, 0);
}

/**
 * gcm_hull_widget_set_actor_position:
 **/
static void
gcm_hull_widget_set_actor_position (GcmHullWidget *hull_widget,
				    ClutterActor *actor)
{
	ClutterActor *stage = hull_widget->priv->stage;
	clutter_actor_set_size (actor,
				clutter_actor_get_width (stage) * 0.7f,
				clutter_actor_get_height (stage) * 0.7f);
	clutter_actor_set_position (actor,
				    clutter_actor_get_width (stage) * 0.15f,
				    clutter_actor_get_height (stage) * 0.15f);
}

/**
 * gcm_hull_widget_add:
 **/
gboolean
gcm_hull_widget_add (GcmHullWidget *hull_widget,
		     GcmProfile *profile)
{
	ClutterActor *model = NULL;
	gboolean ret = FALSE;
	gchar *ply_data = NULL;
	GcmHull *hull = NULL;
	GError *error = NULL;

	/* generate hull */
	hull = gcm_profile_generate_gamut_hull (profile, 12);
	if (hull == NULL)
		goto out;

	/* save as PLY file */
	ply_data = gcm_hull_export_to_ply (hull);
	ret = g_file_set_contents ("/tmp/gamut.ply", ply_data, -1, &error);
	if (!ret) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}

	/* load model: TODO: use mash_model_new_from_data() */
	model = mash_model_new_from_file (MASH_DATA_NONE, "/tmp/gamut.ply",
					  &error);
	if (model == NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set some good defaults */
	gcm_hull_widget_set_actor_position (hull_widget, model);

	/* add the actor to the stage */
	clutter_actor_add_child (hull_widget->priv->stage, model);
	g_ptr_array_add (hull_widget->priv->models,
			 g_object_ref (model));

	/* enable depth testing */
	g_signal_connect_swapped (model, "paint",
				  G_CALLBACK (cogl_set_depth_test_enabled),
				  GINT_TO_POINTER (TRUE));
	g_signal_connect_data (model, "paint",
			       G_CALLBACK (cogl_set_depth_test_enabled),
			       GINT_TO_POINTER (FALSE), NULL,
			       G_CONNECT_AFTER | G_CONNECT_SWAPPED);

	/* set the old rotation */
	clutter_actor_set_pivot_point (model, 0.5, 0.5);
	clutter_actor_set_rotation_angle (model,
					  CLUTTER_Y_AXIS,
					  hull_widget->priv->existing_rotation);

	/* success */
	clutter_actor_set_opacity (model, 240);
	clutter_actor_show (model);
	ret = TRUE;
out:
	g_free (ply_data);
	if (hull != NULL)
		g_object_unref (hull);
	return ret;
}

/**
 * gcm_hull_widget_button_press_cb:
 **/
static gboolean
gcm_hull_widget_button_press_cb (GtkWidget *widget,
				 GdkEvent *event,
				 GcmHullWidget *hull_widget)
{
	GdkEventButton *button = (GdkEventButton *) event;
	hull_widget->priv->button = button->button;
	hull_widget->priv->old_x = 0;
	hull_widget->priv->old_y = 0;
	return TRUE;
}

/**
 * gcm_hull_widget_button_release_cb:
 **/
static gboolean
gcm_hull_widget_button_release_cb (GtkWidget *widget,
				   GdkEvent  *event,
				   GcmHullWidget *hull_widget)
{
	hull_widget->priv->button = 0;
	return TRUE;
}

/**
 * gcm_hull_widget_motion_notify_cb:
 **/
static gboolean
gcm_hull_widget_motion_notify_cb (GtkWidget *widget,
				  GdkEvent  *event,
				  GcmHullWidget *hull_widget)
{
	ClutterActor *model;
	GdkEventMotion *motion = (GdkEventMotion *) event;
	gdouble angle_x;
	guint i;

	if (hull_widget->priv->button == 0)
		goto out;
	if (hull_widget->priv->old_x < 0.001 ||
	    hull_widget->priv->old_y < 0.001) {
		goto out;
	}

	if (hull_widget->priv->models->len == 0)
		return FALSE;

	/* work out the offset */
	angle_x = motion->x - hull_widget->priv->old_x;

	/* get old rotation of primary model */
	model = g_ptr_array_index (hull_widget->priv->models, 0);
	hull_widget->priv->existing_rotation =
		clutter_actor_get_rotation_angle (model,
						  CLUTTER_Y_AXIS);

	/* rotate all the models on the stage */
	for (i=0; i<hull_widget->priv->models->len; i++) {
		model = g_ptr_array_index (hull_widget->priv->models, i);
		clutter_actor_set_rotation_angle (model,
						  CLUTTER_Y_AXIS,
						  hull_widget->priv->existing_rotation + angle_x);
	}
out:
	hull_widget->priv->old_x = motion->x;
	hull_widget->priv->old_y = motion->y;
	return TRUE;
}

/**
 * gcm_hull_widget_stage_allocation_changed_cb:
 **/
static void
gcm_hull_widget_stage_allocation_changed_cb (ClutterActor *actor,
					     ClutterActorBox *box,
					     ClutterAllocationFlags flags,
					     GcmHullWidget *hull_widget)
{
	ClutterActor *model;
	guint i;

	for (i=0; i<hull_widget->priv->models->len; i++) {
		model = g_ptr_array_index (hull_widget->priv->models, i);
		gcm_hull_widget_set_actor_position (hull_widget, model);
	}
}

/**
 * gcm_hull_widget_get_property:
 **/
static void
gcm_hull_widget_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmHullWidget *hull_widget = GCM_HULL_WIDGET (object);
	switch (prop_id) {
	case PROP_STAGE_COLOR:
		g_value_set_double (value, hull_widget->priv->stage_color);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_hull_widget_set_property:
 **/
static void
gcm_hull_widget_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmHullWidget *hull_widget = GCM_HULL_WIDGET (object);

	switch (prop_id) {
	case PROP_STAGE_COLOR:
		hull_widget->priv->stage_color = g_value_get_double (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	/* refresh widget */
	gtk_widget_hide (GTK_WIDGET (hull_widget));
	gtk_widget_show (GTK_WIDGET (hull_widget));
}

/**
 * gcm_hull_widget_class_init:
 **/
static void
gcm_hull_widget_class_init (GcmHullWidgetClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->get_property = gcm_hull_widget_get_property;
	object_class->set_property = gcm_hull_widget_set_property;
	object_class->finalize = gcm_hull_widget_finalize;

	g_type_class_add_private (class, sizeof (GcmHullWidgetPrivate));

	/* properties */
	g_object_class_install_property (object_class,
					 PROP_STAGE_COLOR,
					 g_param_spec_double ("stage-color", NULL, NULL,
							      0.0, 1.0, 0.8,
							      G_PARAM_READWRITE));
}

/**
 * gcm_hull_widget_init:
 **/
static void
gcm_hull_widget_init (GcmHullWidget *hull_widget)
{
	ClutterColor color;

	hull_widget->priv = GCM_HULL_WIDGET_GET_PRIVATE (hull_widget);
	hull_widget->priv->stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (hull_widget));

	/* list of models */
	hull_widget->priv->models = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* grey stage background */
	color.red = 120;
	color.green = 120;
	color.blue = 120;
	color.alpha = 0;
	clutter_actor_set_background_color (CLUTTER_ACTOR (hull_widget->priv->stage), &color);
	g_signal_connect (hull_widget->priv->stage, "allocation-changed",
			  G_CALLBACK (gcm_hull_widget_stage_allocation_changed_cb),
			  hull_widget);

	hull_widget->priv->existing_rotation = 270.0f;

	/* allow user to rotate */
	gtk_widget_add_events (GTK_WIDGET (hull_widget),
			       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			       GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK);
	g_signal_connect (hull_widget, "button-press-event",
			  G_CALLBACK (gcm_hull_widget_button_press_cb),
			  hull_widget);
	g_signal_connect (hull_widget, "button-release-event",
			  G_CALLBACK (gcm_hull_widget_button_release_cb),
			  hull_widget);
	g_signal_connect (hull_widget, "motion-notify-event",
			  G_CALLBACK (gcm_hull_widget_motion_notify_cb),
			  hull_widget);

}

/**
 * gcm_hull_widget_finalize:
 **/
static void
gcm_hull_widget_finalize (GObject *object)
{
	GcmHullWidget *hull_widget = (GcmHullWidget*) object;
	g_ptr_array_unref (hull_widget->priv->models);
	G_OBJECT_CLASS (gcm_hull_widget_parent_class)->finalize (object);
}

/**
 * gcm_hull_widget_new:
 * Return value: A new GcmHullWidget object.
 **/
GtkWidget *
gcm_hull_widget_new (void)
{
	return g_object_new (GCM_TYPE_HULL_WIDGET, NULL);
}

