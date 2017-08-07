/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2015 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <canberra-gtk.h>
#include <colord.h>
#include <lcms2.h>
#include <stdlib.h>

#include "gcm-utils.h"
#include "gcm-debug.h"
#include "gcm-calibrate.h"
#include "gcm-calibrate-argyll.h"

typedef enum {
	GCM_CALIBRATE_PAGE_INTRO,
	GCM_CALIBRATE_PAGE_DISPLAY_KIND,
	GCM_CALIBRATE_PAGE_DISPLAY_TEMPERATURE,
	GCM_CALIBRATE_PAGE_DISPLAY_CONFIG,
	GCM_CALIBRATE_PAGE_INSTALL_ARGYLLCMS,
	GCM_CALIBRATE_PAGE_INSTALL_TARGETS,
	GCM_CALIBRATE_PAGE_PRECISION,
	GCM_CALIBRATE_PAGE_PRINT_KIND,
	GCM_CALIBRATE_PAGE_TARGET_KIND,
	GCM_CALIBRATE_PAGE_SENSOR,
	GCM_CALIBRATE_PAGE_ACTION,
	GCM_CALIBRATE_PAGE_FAILURE,
	GCM_CALIBRATE_PAGE_TITLE,
	GCM_CALIBRATE_PAGE_LAST
} GcmCalibratePage;

typedef struct {
	GtkApplication	*application;
	CdClient	*client;
	GcmCalibrate	*calibrate;
	CdDevice	*device;
	CdDeviceKind	 device_kind;
	GCancellable	*cancellable;
	gchar		*device_id;
	guint		 xid;
	GtkWindow	*main_window;
	GPtrArray	*pages;
	gboolean	 internal_lcd;
	GtkWidget	*reference_preview;
	GtkWidget	*action_title;
	GtkWidget	*action_message;
	GtkWidget	*action_image;
	GtkWidget	*action_progress;
	gboolean	 has_pending_interaction;
	gboolean	 started_calibration;
	GcmCalibratePage current_page;
	gint		 inhibit_cookie;
} GcmCalibratePriv;

static void
gcm_window_set_parent_xid (GtkWindow *window, guint32 xid)
{
	GdkDisplay *display;
	GdkWindow *parent_window;
	GdkWindow *our_window;

	display = gdk_display_get_default ();
	parent_window = gdk_x11_window_foreign_new_for_display (display, xid);
	if (parent_window == NULL) {
		g_warning ("failed to get parent window");
		return;
	}

	gtk_widget_realize (GTK_WIDGET (window));
	our_window = gtk_widget_get_window (GTK_WIDGET (window));
	if (our_window == NULL) {
		g_warning ("failed to get our window");
		return;
	}

	/* set this above our parent */
	gtk_window_set_modal (window, TRUE);
	gdk_window_set_transient_for (our_window, parent_window);
	gtk_window_set_title (window, "");
}

static void
gcm_calib_activate_cb (GApplication *application, GcmCalibratePriv *priv)
{
	gtk_window_present (priv->main_window);
}

static void
gcm_calib_confirm_quit_cb (GtkDialog *dialog,
			   gint response_id,
			   GcmCalibratePriv *priv)
{
	if (response_id != GTK_RESPONSE_CLOSE) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		return;
	}
	gcm_calibrate_interaction (priv->calibrate, GTK_RESPONSE_CANCEL);
	g_application_release (G_APPLICATION (priv->application));
}

static void
gcm_calib_confirm_quit (GcmCalibratePriv *priv)
{
	GtkWidget *dialog;

	/* do not ask for confirmation on the initial page */
	if (priv->current_page == GCM_CALIBRATE_PAGE_INTRO)
		g_application_release (G_APPLICATION (priv->application));

	dialog = gtk_message_dialog_new (GTK_WINDOW (priv->main_window),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 "%s", _("Calibration is not complete"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s",
						  _("Are you sure you want to cancel the calibration?"));
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       /* TRANSLATORS: button text */
			       _("Continue calibration"),
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       /* TRANSLATORS: button text */
			       _("Cancel and close"),
			       GTK_RESPONSE_CLOSE);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gcm_calib_confirm_quit_cb),
			  priv);
	gtk_widget_show (dialog);
}

static gboolean
gcm_calib_delete_event_cb (GtkWidget *widget, GdkEvent *event, GcmCalibratePriv *priv)
{
	gcm_calib_confirm_quit (priv);
	return FALSE;
}

static void
gcm_calib_assistant_cancel_cb (GtkAssistant *assistant, GcmCalibratePriv *priv)
{
	gcm_calib_confirm_quit (priv);
}

static void
gcm_calib_assistant_close_cb (GtkAssistant *assistant, GcmCalibratePriv *priv)
{
	g_application_release (G_APPLICATION (priv->application));
}

static void
gcm_calib_play_sound (GcmCalibratePriv *priv)
{
	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "complete",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 /* TRANSLATORS: this is the sound description */
			 CA_PROP_EVENT_DESCRIPTION, _("Profiling completed"),
			 NULL);
}


static GtkWidget *
gcm_calib_get_vbox_for_page (GcmCalibratePriv *priv,
			     GcmCalibratePage page)
{
	guint i;
	GtkWidget *tmp;
	GcmCalibratePage page_tmp;

	for (i = 0; i < priv->pages->len; i++) {
		tmp = g_ptr_array_index (priv->pages, i);
		page_tmp = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (tmp),
								"GcmCalibrateMain::Index"));
		if (page_tmp == page)
			return tmp;
	}
	return NULL;
}

static wchar_t *
utf8_to_wchar_t (const char *src)
{
	gsize len;
	gsize converted;
	wchar_t *buf = NULL;

	len = mbstowcs (NULL, src, 0);
	if (len == (gsize) -1) {
		g_warning ("Invalid UTF-8 in string %s", src);
		goto out;
	}
	len += 1;
	buf = g_malloc (sizeof (wchar_t) * len);
	converted = mbstowcs (buf, src, len - 1);
	g_assert (converted != (gsize) -1);
	buf[converted] = '\0';
out:
	return buf;
}

static cmsBool
_cmsDictAddEntryAscii (cmsHANDLE dict,
		       const gchar *key,
		       const gchar *value)
{
	g_autofree wchar_t *mb_key = NULL;
	g_autofree wchar_t *mb_value = NULL;

	mb_key = utf8_to_wchar_t (key);
	if (mb_key == NULL)
		return FALSE;
	mb_value = utf8_to_wchar_t (value);
	if (mb_value == NULL)
		return FALSE;
	return cmsDictAddEntry (dict, mb_key, mb_value, NULL, NULL);
}

static gboolean
gcm_calib_set_extra_metadata (GcmCalibratePriv *priv,
			      const gchar *filename,
			      GError **error)
{
	cmsHANDLE dict = NULL;
	cmsHPROFILE lcms_profile;
	gboolean ret = TRUE;
	gsize len;
	guint percentage;
	CdSensor *sensor;
	g_autofree gchar *data = NULL;
	g_autofree gchar *screen_brightness_str = NULL;

	/* parse */
	ret = g_file_get_contents (filename, &data, &len, error);
	if (!ret)
		goto out;
	lcms_profile = cmsOpenProfileFromMem (data, len);
	if (lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0,
				     "failed to open profile");
		ret = FALSE;
		goto out;
	}

	/* just create a new dict */
	dict = cmsDictAlloc (NULL);
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_CMF_PRODUCT,
			       PACKAGE_NAME);
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_CMF_BINARY,
			       "gcm-calibrate");
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_CMF_VERSION,
			       PACKAGE_VERSION);
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_DATA_SOURCE,
			       CD_PROFILE_METADATA_DATA_SOURCE_CALIB);
	sensor = gcm_calibrate_get_sensor (priv->calibrate);
	if (sensor != NULL) {
		_cmsDictAddEntryAscii (dict,
				       CD_PROFILE_METADATA_MEASUREMENT_DEVICE,
				       cd_sensor_kind_to_string (cd_sensor_get_kind (sensor)));
	}
	_cmsDictAddEntryAscii (dict,
			       CD_PROFILE_METADATA_MAPPING_DEVICE_ID,
			       cd_device_get_id (priv->device));

	/* add the calibration brightness if an internal panel */
	percentage = gcm_calibrate_get_screen_brightness (priv->calibrate);
	if (percentage > 0) {
		screen_brightness_str = g_strdup_printf ("%u", percentage);
		_cmsDictAddEntryAscii (dict,
				       CD_PROFILE_METADATA_SCREEN_BRIGHTNESS,
				       screen_brightness_str);
	}

	/* just write dict */
	ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "cannot write metadata");
		goto out;
	}

	/* write profile id */
	ret = cmsMD5computeID (lcms_profile);
	if (!ret) {
		g_set_error_literal (error, 1, 0,
				     "failed to write profile id");
		goto out;
	}

	/* save, TODO: get error */
	cmsSaveProfileToFile (lcms_profile, filename);
	ret = TRUE;
out:
	if (dict != NULL)
		cmsDictFree (dict);
	return ret;
}

static void
gcm_calib_set_sensor_options_cb (GObject *object,
				 GAsyncResult *res,
				 gpointer user_data)
{
	CdSensor *sensor = CD_SENSOR (object);
	gboolean ret;
	g_autoptr(GError) error = NULL;

	/* get return value */
	ret = cd_sensor_set_options_finish (sensor, res, &error);
	if (!ret) {
		g_warning ("Failed to set sensor option: %s",
			   error->message);
	}
}

static void
gcm_calib_set_sensor_options (GcmCalibratePriv *priv,
			      const gchar *filename)
{
	CdSensor *sensor;
	gboolean ret;
	gsize len;
	g_autofree gchar *data = NULL;
	g_autofree gchar *sha1 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GHashTable) hash = NULL;

	/* get ChSensor */
	sensor = gcm_calibrate_get_sensor (priv->calibrate);
	if (sensor == NULL)
		return;

	/* set the remote profile hash */
	hash = g_hash_table_new_full (g_str_hash,
				      g_str_equal,
				      g_free,
				      (GDestroyNotify) g_variant_unref);
	ret = g_file_get_contents (filename, &data, &len, &error);
	if (!ret) {
		g_warning ("Failed to get SHA1 hash: %s",
			   error->message);
		return;
	}
	sha1 = g_compute_checksum_for_data (G_CHECKSUM_SHA1,
					    (const guchar *) data,
					    len);
	g_hash_table_insert (hash,
			     g_strdup ("remote-profile-hash"),
			     g_variant_ref_sink (g_variant_new_string (sha1)));
	cd_sensor_set_options (sensor, hash, NULL,
			       gcm_calib_set_sensor_options_cb,
			       priv);
}

static gboolean
gcm_calib_start_idle_cb (gpointer user_data)
{
	g_autoptr(CdProfile) profile = NULL;
	const gchar *filename;
	gboolean ret;
	GcmCalibratePriv *priv = (GcmCalibratePriv *) user_data;
	gint inhibit_cookie;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);
	GtkWidget *vbox;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;

	/* inhibit */
	inhibit_cookie = gtk_application_inhibit (priv->application,
						  priv->main_window,
						  GTK_APPLICATION_INHIBIT_LOGOUT |
						  GTK_APPLICATION_INHIBIT_SWITCH |
						  GTK_APPLICATION_INHIBIT_SUSPEND |
						  GTK_APPLICATION_INHIBIT_IDLE,
						  "Calibration in progress");

	/* actually do the action */
	priv->started_calibration = TRUE;
	ret = gcm_calibrate_device (priv->calibrate,
				    priv->device,
				    priv->main_window,
				    &error);
	if (!ret) {
		gcm_calibrate_set_title (priv->calibrate,
					 _("Failed to calibrate"),
					 GCM_CALIBRATE_UI_ERROR);
		gcm_calibrate_set_message (priv->calibrate,
					   error->message,
					   GCM_CALIBRATE_UI_ERROR);
		gcm_calibrate_set_image (priv->calibrate, NULL);

		g_warning ("failed to calibrate: %s",
			   error->message);

		/* mark this box as the end */
		vbox = gcm_calib_get_vbox_for_page (priv, GCM_CALIBRATE_PAGE_ACTION);
		gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_SUMMARY);
		gtk_assistant_set_page_complete (assistant, vbox, TRUE);
		goto out;
	}

	/* get profile */
	filename = gcm_calibrate_get_filename_result (priv->calibrate);
	if (filename == NULL) {
		g_warning ("failed to get filename from calibration");
		goto out;
	}

	/* set some private properties */
	ret = gcm_calib_set_extra_metadata (priv, filename, &error);
	if (!ret) {
		g_warning ("failed to set extra metadata: %s",
			   error->message);
		goto out;
	}

	/* inform the sensor about the last successful profile */
	gcm_calib_set_sensor_options (priv, filename);

	/* copy the ICC file to the proper location */
	file = g_file_new_for_path (filename);
	profile = cd_client_import_profile_sync (priv->client,
						 file,
						 priv->cancellable,
						 &error);
	if (profile == NULL) {
		g_warning ("failed to find calibration profile: %s",
			   error->message);
		goto out;
	}
	ret = cd_device_add_profile_sync (priv->device,
					  CD_DEVICE_RELATION_HARD,
					  profile,
					  priv->cancellable,
					  &error);
	if (!ret) {
		g_warning ("failed to add %s to %s: %s",
			   cd_profile_get_object_path (profile),
			   cd_device_get_object_path (priv->device),
			   error->message);
		goto out;
	}

	/* remove temporary file */
	g_unlink (filename);

	/* allow forward */
	vbox = gcm_calib_get_vbox_for_page (priv,
					    GCM_CALIBRATE_PAGE_ACTION);
	gtk_assistant_set_page_complete (assistant,
					 vbox, TRUE);

	/* set to summary page */
	gtk_assistant_set_current_page (assistant,
					gtk_assistant_get_n_pages (assistant) - 1);
out:
	if (inhibit_cookie != 0) {
		gtk_application_uninhibit (priv->application,
					   priv->inhibit_cookie);
	}
	return FALSE;
}

static gint
gcm_calib_assistant_page_forward_cb (gint current_page, gpointer user_data)
{
	GtkWidget *vbox;
	GcmCalibratePriv *priv = (GcmCalibratePriv *) user_data;

	/* shouldn't happen... */
	if (priv->current_page != GCM_CALIBRATE_PAGE_ACTION)
		return current_page + 1;

	if (!priv->has_pending_interaction)
		return current_page;

	/* continue calibration */
	gcm_calibrate_interaction (priv->calibrate, GTK_RESPONSE_OK);
	priv->has_pending_interaction = FALSE;

	/* no longer allow forward */
	vbox = gcm_calib_get_vbox_for_page (priv,
					    GCM_CALIBRATE_PAGE_ACTION);

	gtk_assistant_set_page_complete (GTK_ASSISTANT (priv->main_window),
					 vbox, FALSE);
	return current_page;
}

static gboolean
gcm_calib_assistant_prepare_cb (GtkAssistant *assistant,
				GtkWidget *page_widget,
				GcmCalibratePriv *priv)
{
	priv->current_page = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (page_widget),
								   "GcmCalibrateMain::Index"));
	switch (priv->current_page) {
	case GCM_CALIBRATE_PAGE_LAST:
		gcm_calib_play_sound (priv);
		break;
	case GCM_CALIBRATE_PAGE_ACTION:
		g_debug ("lights! camera! action!");
		if (!priv->started_calibration)
			g_idle_add (gcm_calib_start_idle_cb, priv);
		break;
	default:
		break;
	}

	/* ensure we cancel argyllcms if the user clicks back */
	if (priv->current_page != GCM_CALIBRATE_PAGE_ACTION &&
	    priv->started_calibration) {
		gcm_calibrate_interaction (priv->calibrate,
					   GTK_RESPONSE_CANCEL);
		priv->started_calibration = FALSE;
	}

	/* forward on the action page just unsticks the calibration */
	if (priv->current_page == GCM_CALIBRATE_PAGE_ACTION) {
		gtk_assistant_set_forward_page_func (assistant,
						     gcm_calib_assistant_page_forward_cb,
						     priv,
						     NULL);
	} else {
		gtk_assistant_set_forward_page_func (assistant,
						     NULL, NULL, NULL);
	}

	/* use the default on each page */
	switch (priv->current_page) {
	case GCM_CALIBRATE_PAGE_INSTALL_ARGYLLCMS:
	case GCM_CALIBRATE_PAGE_SENSOR:
	case GCM_CALIBRATE_PAGE_ACTION:
		break;
	default:
		gtk_assistant_set_page_complete (assistant, page_widget, TRUE);
		break;
	}
	return FALSE;
}

static GtkWidget *
gcm_calib_add_page_title (GcmCalibratePriv *priv, const gchar *text)
{
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *vbox;
	g_autofree gchar *markup = NULL;

	markup = g_strdup_printf ("<span size=\"large\" font_weight=\"bold\">%s</span>", text);
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), markup);

	/* make left aligned */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

	/* header */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	return vbox;
}

static gboolean
gcm_calib_label_activate_link_cb (GtkLabel *label,
				  gchar *uri,
				  GcmCalibratePriv *priv)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	const gchar *argv[] = { BINDIR "/gnome-control-center color",
				"color",
				NULL };
	ret = g_spawn_async (NULL,
			     (gchar **) argv,
			     NULL,
			     0,
			     NULL, NULL,
			     NULL,
			     &error);
	if (!ret) {
		g_warning ("failed to launch the control center: %s",
			   error->message);
	}
	return ret;
}

static GtkWidget *
gcm_calib_add_page_para (GtkWidget *vbox, const gchar *text)
{
	GtkWidget *label;
	GtkWidget *hbox;

	label = gtk_label_new (NULL);
	g_signal_connect (label, "activate-link",
			  G_CALLBACK (gcm_calib_label_activate_link_cb),
			  NULL);
	gtk_label_set_markup (GTK_LABEL (label), text);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (label), 40);
	gtk_label_set_xalign (GTK_LABEL (label), 0.f);
	gtk_label_set_yalign (GTK_LABEL (label), 0.f);

	/* make left aligned */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	return label;
}

static void
gcm_calib_add_page_bullet (GtkWidget *vbox, const gchar *text)
{
	g_autofree gchar *markup = NULL;
	markup = g_strdup_printf ("• %s", text);
	gcm_calib_add_page_para (vbox, markup);
}

static void
gcm_calib_setup_page_intro (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is intro page text */
	switch (priv->device_kind) {
	case CD_DEVICE_KIND_CAMERA:
	case CD_DEVICE_KIND_WEBCAM:
		/* TRANSLATORS: this is the page title */
		vbox = gcm_calib_add_page_title (priv, _("Calibrate your camera"));
		break;
	case CD_DEVICE_KIND_DISPLAY:
		/* TRANSLATORS: this is the page title */
		vbox = gcm_calib_add_page_title (priv, _("Calibrate your display"));
		break;
	case CD_DEVICE_KIND_PRINTER:
		/* TRANSLATORS: this is the page title */
		vbox = gcm_calib_add_page_title (priv, _("Calibrate your printer"));
		break;
	default:
		/* TRANSLATORS: this is the page title */
		vbox = gcm_calib_add_page_title (priv, _("Calibrate your device"));
		break;
	}

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	switch (priv->device_kind) {
	case CD_DEVICE_KIND_DISPLAY:
		/* TRANSLATORS: this is the final intro page text */
		gcm_calib_add_page_para (content, _("Any existing screen correction will be temporarily turned off and the brightness set to maximum."));
		break;
	default:
		break;
	}

	/* TRANSLATORS: this is the final intro page text */
	gcm_calib_add_page_para (content, _("You can cancel this process at any stage by pressing the cancel button."));

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_INTRO);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Introduction"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_INTRO));

	/* show page */
	gtk_widget_show_all (vbox);
}

static gboolean
gcm_calibrate_is_livecd (void)
{
#ifdef __linux__
	g_autofree gchar *data = NULL;
	g_autoptr(GError) error = NULL;

	/* get the kernel commandline */
	if (!g_file_get_contents ("/proc/cmdline", &data, NULL, &error)) {
		g_warning ("failed to get kernel command line: %s",
			   error->message);
		return FALSE;
	}
	return (g_strstr_len (data, -1, "liveimg") != NULL ||
		g_strstr_len (data, -1, "casper") != NULL);
#else
	return FALSE;
#endif
}

static void
gcm_calib_show_profile_button_clicked_cb (GtkButton *button,
					  GcmCalibratePriv *priv)
{
	const gchar *argv[] = { BINDIR "/nautilus", "", NULL };
	gboolean ret;
	g_autofree gchar *path = NULL;
	g_autoptr(GError) error = NULL;

	/* just hardcode nautilus to open the folder */
	path = g_build_filename (g_get_user_data_dir (), "icc", NULL);
	argv[1] = path;
	ret = g_spawn_async (NULL,
			     (gchar **) argv,
			     NULL,
			     0,
			     NULL, NULL,
			     NULL,
			     &error);
	if (!ret) {
		g_warning ("failed to show profile: %s", error->message);
		return;
	}
}

static GtkWidget *
gcm_calib_get_show_profile_button (GcmCalibratePriv *priv)
{
	GtkStyleContext *context;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *vbox;

	/* add button to show profile */
	button = gtk_button_new ();
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	image = gtk_image_new_from_icon_name ("folder-publicshare-symbolic",
					      GTK_ICON_SIZE_DIALOG);

	/* make image have a gray foreground */
	context = gtk_widget_get_style_context (image);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_IMAGE);

	gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);
	label = gtk_label_new (_("Show File"));
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (button), vbox);
	gtk_widget_set_tooltip_text (button, _("Click here to show the profile"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (gcm_calib_show_profile_button_clicked_cb),
			  priv);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 15);
	gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
	gtk_widget_show_all (button);
	return button;
}

static void
gcm_calib_setup_page_summary (GcmCalibratePriv *priv)
{
	gboolean ret;
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *image;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("All done!"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	switch (priv->device_kind) {
	case CD_DEVICE_KIND_CAMERA:
	case CD_DEVICE_KIND_WEBCAM:
		/* TRANSLATORS: this is the final summary */
		gcm_calib_add_page_para (content, _("The camera has been calibrated successfully."));
		break;
	case CD_DEVICE_KIND_DISPLAY:
		/* TRANSLATORS: this is the final summary */
		gcm_calib_add_page_para (content, _("The display has been calibrated successfully."));
		break;
	case CD_DEVICE_KIND_PRINTER:
		/* TRANSLATORS: this is the final summary */
		gcm_calib_add_page_para (content, _("The printer has been calibrated successfully."));
		break;
	default:
		/* TRANSLATORS: this is the final summary */
		gcm_calib_add_page_para (content, _("The device has been calibrated successfully."));
		break;
	}

	/* only display the backlink if not launched from the control center itself */
	if (priv->xid == 0) {
		/* TRANSLATORS: this is the final summary */
		gcm_calib_add_page_para (content, _("To view details about the new profile or to undo the calibration visit the <a href=\"control-center://color\">control center</a>."));
	}

	/* show the user the profile to copy off the live system */
	ret = gcm_calibrate_is_livecd ();
	if (ret) {
		/* show button to copy profile */
		image = gcm_calib_get_show_profile_button (priv);
		gtk_box_pack_start (GTK_BOX (content), image, FALSE, FALSE, 30);
		gcm_calib_add_page_para (content, _("You can use the profile with <a href=\"import-linux\">Linux</a>, <a href=\"import-osx\">Apple OS X</a> and <a href=\"import-windows\">Microsoft Windows</a> systems."));
	} else {
		/* add image for success */
		image = gtk_image_new ();
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "face-smile", GTK_ICON_SIZE_DIALOG);
		gtk_box_pack_start (GTK_BOX (content), image, FALSE, FALSE, 0);
	}

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_SUMMARY);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Summary"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_LAST));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_setup_page_action (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GList *list;
	GList *list2;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Performing calibration"));

	/* grab title */
	list = gtk_container_get_children (GTK_CONTAINER (vbox));
	list2 = gtk_container_get_children (GTK_CONTAINER (list->data));
	priv->action_title = list2->data;
	g_list_free (list);
	g_list_free (list2);

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	priv->action_message = gcm_calib_add_page_para (content, _("Calibration is about to start"));

	/* add image for success */
	priv->action_image = gtk_image_new ();
	gtk_image_set_from_icon_name (GTK_IMAGE (priv->action_image), "face-frown", GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (content), priv->action_image, FALSE, FALSE, 0);

	/* add progress marker */
	priv->action_progress = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (content), priv->action_progress, FALSE, FALSE, 0);

	/* add content widget */
	gcm_calibrate_set_content_widget (priv->calibrate, vbox);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Action"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_ACTION));

	/* show page */
	gtk_widget_show_all (vbox);
	gtk_widget_hide (priv->action_image);
}

static void
gcm_calib_setup_page_display_configure_wait (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: dialog message, preface */
	vbox = gcm_calib_add_page_title (priv, _("Calibration checklist"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, _("Before calibrating the display, it is recommended to configure your display with the following settings to get optimal results."));

	/* TRANSLATORS: dialog message, preface */
if(0)	gcm_calib_add_page_para (content, _("You may want to consult the owner’s manual for your display on how to achieve these settings."));

	/* TRANSLATORS: dialog message, bullet item */
if(0)	gcm_calib_add_page_bullet (content, _("Reset your display to the factory defaults."));

	/* TRANSLATORS: dialog message, bullet item */
if(0)	gcm_calib_add_page_bullet (content, _("Disable dynamic contrast if your display has this feature."));

	/* TRANSLATORS: dialog message, bullet item */
if(0)	gcm_calib_add_page_bullet (content, _("Configure your display with custom color settings and ensure the RGB channels are set to the same values."));

	/* TRANSLATORS: dialog message, addition to bullet item */
if(0)	gcm_calib_add_page_para (content, _("If custom color is not available then use a 6500K color temperature."));

	/* TRANSLATORS: dialog message, bullet item */
if(0)	gcm_calib_add_page_bullet (content, _("Adjust the display brightness to a comfortable level for prolonged viewing."));

	gcm_calib_add_page_para (content, "");

	/* TRANSLATORS: dialog message, suffix */
	gcm_calib_add_page_para (content, _("For best results, the display should have been powered for at least 15 minutes before starting the calibration."));

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Check Settings"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_DISPLAY_CONFIG));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_button_clicked_install_argyllcms_cb (GtkButton *button, GcmCalibratePriv *priv)
{
	gboolean ret;
	GtkWidget *vbox;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	ret = gcm_utils_install_package (GCM_PREFS_PACKAGE_NAME_ARGYLLCMS,
					 priv->main_window);
	/* we can continue now */
	if (TRUE || ret) {
		vbox = gcm_calib_get_vbox_for_page (priv,
						    GCM_CALIBRATE_PAGE_INSTALL_ARGYLLCMS);
		gtk_assistant_set_page_complete (assistant, vbox, TRUE);
		gtk_assistant_next_page (assistant);

		/* we ddn't need to re-install now */
		gtk_widget_hide (vbox);
	}
}

static void
gcm_calib_setup_page_install_argyllcms (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *button;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);
	g_autoptr(GString) string = NULL;

	string = g_string_new ("");

	g_string_append_printf (string, "%s\n",
				/* TRANSLATORS: dialog message saying the argyllcms is not installed */
				_("Calibration and profiling software is not installed."));
	g_string_append_printf (string, "%s",
				/* TRANSLATORS: dialog message saying the color targets are not installed */
				_("These tools are required to build color profiles for devices."));

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("More software is required!"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, string->str);

	button = gtk_button_new_with_label (_("Install required software"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (gcm_calib_button_clicked_install_argyllcms_cb),
			  priv);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Install Tools"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_INSTALL_ARGYLLCMS));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_button_clicked_install_targets_cb (GtkButton *button, GcmCalibratePriv *priv)
{
	gboolean ret;
	GtkWidget *vbox;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	ret = gcm_utils_install_package (GCM_PREFS_PACKAGE_NAME_SHARED_COLOR_TARGETS,
					 priv->main_window);
	/* we can continue now */
	if (ret) {
		vbox = gcm_calib_get_vbox_for_page (priv,
						    GCM_CALIBRATE_PAGE_INSTALL_TARGETS);
		gtk_assistant_next_page (assistant);

		/* we ddn't need to re-install now */
		gtk_widget_hide (vbox);
	}
}

static void
gcm_calib_setup_page_install_targets (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *button;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);
	g_autoptr(GString) string = NULL;

	string = g_string_new ("");

	/* TRANSLATORS: dialog message saying the color targets are not installed */
	g_string_append_printf (string, "%s\n", _("Common color target files are not installed on this computer."));
	/* TRANSLATORS: dialog message saying the color targets are not installed */
	g_string_append_printf (string, "%s\n\n", _("Color target files are needed to convert the image to a color profile."));
	/* TRANSLATORS: dialog message, asking if it's okay to install them */
	g_string_append_printf (string, "%s\n\n", _("Do you want them to be installed?"));
	/* TRANSLATORS: dialog message, if the user has the target file on a CDROM then there's no need for this package */
	g_string_append_printf (string, "%s", _("If you already have the correct file, you can skip this step."));

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Optional data files available"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, string->str);

	button = gtk_button_new_with_label (_("Install Now"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (gcm_calib_button_clicked_install_targets_cb),
			  priv);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Install Targets"));
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_INSTALL_TARGETS));

	/* show page */
	gtk_widget_show_all (vbox);
}


static const gchar *
gcm_calib_reference_kind_to_localised_string (GcmCalibrateReferenceKind kind)
{
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DIGITAL_TARGET_3) {
		/* TRANSLATORS: this is probably a brand name */
		return _("CMP Digital Target 3");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DT_003) {
		/* TRANSLATORS: this is probably a brand name */
		return _("CMP DT 003");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER) {
		/* TRANSLATORS: this is probably a brand name */
		return _("ColorChecker");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_DC) {
		/* TRANSLATORS: this is probably a brand name */
		return _("ColorChecker DC");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_SG) {
		/* TRANSLATORS: this is probably a brand name */
		return _("ColorChecker SG");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_HUTCHCOLOR) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Hutchcolor");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_I1_RGB_SCAN_1_4) {
		/* TRANSLATORS: this is probably a brand name */
		return _("i1 RGB Scan 1.4");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_IT8) {
		/* TRANSLATORS: this is probably a brand name */
		return _("IT8.7/2");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_LASER_SOFT_DC_PRO) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Laser Soft DC Pro");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201) {
		/* TRANSLATORS: this is probably a brand name */
		return _("QPcard 201");
	}
	return NULL;
}

static const gchar *
gcm_calib_reference_kind_to_image_filename (GcmCalibrateReferenceKind kind)
{
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DIGITAL_TARGET_3)
		return "CMP-DigitalTarget3.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DT_003)
		return "CMP_DT_003.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER)
		return "ColorChecker24.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_DC)
		return "ColorCheckerDC.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_SG)
		return "ColorCheckerSG.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_HUTCHCOLOR)
		return NULL;
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_I1_RGB_SCAN_1_4)
		return "i1_RGB_Scan_14.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_IT8)
		return "IT872.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_LASER_SOFT_DC_PRO)
		return "LaserSoftDCPro.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201)
		return "QPcard_201.png";
	return NULL;
}

static void
gcm_calib_reference_kind_combobox_cb (GtkComboBox *combo_box,
				      GcmCalibratePriv *priv)
{
	const gchar *filename;
	GcmCalibrateReferenceKind reference_kind;
	g_autofree gchar *path = NULL;

	/* not sorted so we can just use the index */
	reference_kind = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));
	g_object_set (priv->calibrate,
		      "reference-kind", reference_kind,
		      NULL);
	filename = gcm_calib_reference_kind_to_image_filename (reference_kind);

	/* fallback */
	if (filename == NULL)
		filename = "unknown.png";

	path = g_build_filename (PKGDATADIR, "targets", filename, NULL);
	gtk_image_set_from_file (GTK_IMAGE (priv->reference_preview), path);
}

static void
gcm_calib_setup_page_target_kind (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *combo;
	guint i;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);
	g_autoptr(GString) string = NULL;

	string = g_string_new ("");

	/* TRANSLATORS: dialog message, preface. A calibration target looks like
	 * this: http://www.colorreference.de/targets/target.jpg */
	g_string_append_printf (string, "%s\n", _("Before profiling the device, you have to manually capture an image of a calibration target and save it as a TIFF image file."));

	/* scanner specific options */
	if (priv->device_kind == CD_DEVICE_KIND_SCANNER) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Ensure that the contrast and brightness are not changed and color correction profiles have not been applied."));

		/* TRANSLATORS: dialog message, suffix */
		g_string_append_printf (string, "%s\n", _("The device sensor should have been cleaned prior to scanning and the output file resolution should be at least 200dpi."));
	}

	/* camera specific options */
	if (priv->device_kind == CD_DEVICE_KIND_CAMERA) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Ensure that the white-balance has not been modified by the camera and that the lens is clean."));
	}

	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append_printf (string, "\n%s", _("Please select the calibration target type."));

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("What target type do you have?"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, string->str);

	/* pack in a preview image */
	priv->reference_preview = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (vbox), priv->reference_preview, FALSE, FALSE, 0);

	combo = gtk_combo_box_text_new ();
	for (i = 0; i < GCM_CALIBRATE_REFERENCE_KIND_UNKNOWN; i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
						gcm_calib_reference_kind_to_localised_string (i));
	}
	g_signal_connect (combo, "changed",
			  G_CALLBACK (gcm_calib_reference_kind_combobox_cb),
			  priv);

	/* use IT8 by default */
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), GCM_CALIBRATE_REFERENCE_KIND_IT8);

	gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, FALSE, 0);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Select Target"));
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_TARGET_KIND));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_display_kind_toggled_cb (GtkToggleButton *togglebutton,
				   GcmCalibratePriv *priv)
{
	GcmCalibrateDisplayKind	 display_kind;

	if (!gtk_toggle_button_get_active (togglebutton))
		return;
	display_kind = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (togglebutton),
							    "GcmCalib::display-kind"));
	g_object_set (priv->calibrate,
		      "display-kind", display_kind,
		      NULL);
}

static void
gcm_calib_setup_page_display_kind (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *widget;
	GSList *list;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Choose your display type"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, _("Select the monitor type that is attached to your computer."));

	widget = gtk_radio_button_new_with_label (NULL, _("LCD (CCFL backlight)"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_DEVICE_KIND_LCD_CCFL));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("LCD (White LED backlight)"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_DEVICE_KIND_LCD_LED_WHITE));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("LCD (RGB LED backlight)"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_DEVICE_KIND_LCD_LED_RGB));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("LCD (Wide Gamut RGB LED backlight)"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_DEVICE_KIND_LCD_LED_RGB_WIDE));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("LCD (Wide Gamut CCFL backlight)"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_DEVICE_KIND_LCD_CCFL_WIDE));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("CRT"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_DEVICE_KIND_CRT));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("Plasma"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_DEVICE_KIND_CRT));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("Projector"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_DEVICE_KIND_PROJECTOR));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Choose Display Type"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_DISPLAY_KIND));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_display_temp_toggled_cb (GtkToggleButton *togglebutton,
				   GcmCalibratePriv *priv)
{
	guint display_temp;
	if (!gtk_toggle_button_get_active (togglebutton))
		return;
	display_temp = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (togglebutton),
							    "GcmCalib::display-temp"));
	g_object_set (priv->calibrate,
		      "target-whitepoint", display_temp,
		      NULL);
}

static void
gcm_calib_setup_page_display_temp (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *widget;
	GSList *list;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Choose your display target white point"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, _("Most displays should be calibrated to a CIE D65 illuminant for general usage."));

	widget = gtk_radio_button_new_with_label (NULL, _("CIE D50 (Printing and publishing)"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-temp",
			   GUINT_TO_POINTER (5000));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_temp_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("CIE D55"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-temp",
			   GUINT_TO_POINTER (5500));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_temp_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("CIE D65 (Photography and graphics)"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-temp",
			   GUINT_TO_POINTER (6500));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_temp_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("CIE D75"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-temp",
			   GUINT_TO_POINTER (7500));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_temp_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, _("Native (Already set manually)"));
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::display-temp",
			   GUINT_TO_POINTER (0));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_display_temp_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Choose Display Whitepoint"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_DISPLAY_TEMPERATURE));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_print_kind_toggled_cb (GtkToggleButton *togglebutton,
				   GcmCalibratePriv *priv)
{
	GcmCalibratePrintKind print_kind;
	if (!gtk_toggle_button_get_active (togglebutton))
		return;
	print_kind = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (togglebutton),
							  "GcmCalib::print-kind"));
	g_object_set (priv->calibrate,
		      "print-kind", print_kind,
		      NULL);
}

static void
gcm_calib_setup_page_print_kind (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *widget;
	GSList *list;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Choose profiling mode"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, _("Please indicate if you want to profile a local printer, generate some test patches, or profile using existing test patches."));

	widget = gtk_radio_button_new_with_label (NULL, "Local printer");
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::print-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PRINT_KIND_LOCAL));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_print_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, "Generate patches for remote printer");
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::print-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PRINT_KIND_GENERATE));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_print_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, "Generate profile from existing test patches");
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::print-kind",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PRINT_KIND_ANALYZE));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_print_kind_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	/* sync the default */
	g_object_set (priv->calibrate,
		      "print-kind", GCM_CALIBRATE_PRINT_KIND_LOCAL,
		      NULL);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Calibration Mode"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_PRINT_KIND));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_precision_toggled_cb (GtkToggleButton *togglebutton,
				GcmCalibratePriv *priv)
{
	GcmCalibratePrecision precision;
	if (!gtk_toggle_button_get_active (togglebutton))
		return;
	precision = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (togglebutton),
							 "GcmCalib::precision"));
	g_object_set (priv->calibrate,
		      "precision", precision,
		      NULL);
}

static void
gcm_calib_setup_page_precision (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *widget;
	GSList *list;
	GString *labels[3];
	guint i;
	guint values_printer[] = { 6, 4, 2}; /* sheets */
	guint values_display[] = { 60, 40, 20}; /* minutes */
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Choose calibration quality"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, _("Higher quality calibration requires many color samples and more time."));

#if 0
	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append (string, _("A higher precision profile provides higher accuracy in color matching but requires more time for reading the color patches."));

	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append_printf (string, "\n%s", _("For a typical workflow, a normal precision profile is sufficient."));

	/* printer specific options */
	if (priv->device_kind == CD_DEVICE_KIND_PRINTER) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "\n%s", _("The high precision profile also requires more paper and printer ink."));
	}
#endif

	/* TRANSLATORS: radio options for calibration precision */
	labels[0] = g_string_new (_("Accurate"));
	labels[1] = g_string_new (_("Normal"));
	labels[2] = g_string_new (_("Quick"));
	switch (priv->device_kind) {
	case CD_DEVICE_KIND_PRINTER:
		for (i = 0; i < 3; i++) {
			g_string_append (labels[i], " ");
			g_string_append_printf (labels[i], ngettext (
						/* TRANSLATORS: radio options for calibration precision */
						"(about %u sheet of paper)",
						"(about %u sheets of paper)",
						values_printer[i]),
						values_printer[i]);
		}
		break;
	case CD_DEVICE_KIND_DISPLAY:
		for (i = 0; i < 3; i++) {
			g_string_append (labels[i], " ");
			g_string_append_printf (labels[i], ngettext (
						/* TRANSLATORS: radio options for calibration precision */
						"(about %u minute)",
						"(about %u minutes)",
						values_display[i]),
						values_display[i]);
		}
		break;
	default:
		break;
	}

	widget = gtk_radio_button_new_with_label (NULL, labels[0]->str);
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::precision",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PRECISION_LONG));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_precision_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, labels[1]->str);
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::precision",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PRECISION_NORMAL));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_precision_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);

	list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	widget = gtk_radio_button_new_with_label (list, labels[2]->str);
	g_object_set_data (G_OBJECT (widget),
			   "GcmCalib::precision",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PRECISION_SHORT));
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (gcm_calib_precision_toggled_cb), priv);
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Calibration Quality"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_PRECISION));

	for (i = 0; i < 3; i++)
		g_string_free (labels[i], TRUE);

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_text_changed_cb (GtkEntry *entry,
			   GcmCalibratePriv *priv)
{
	g_object_set (priv->calibrate,
		      "description", gtk_entry_get_text (entry),
		      NULL);
}

static void
gcm_calib_setup_page_profile_title (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkWidget *widget;
	g_autofree gchar *tmp = NULL;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Profile title"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, _("Choose a title to identify the profile on your system."));

	widget = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (content), widget, FALSE, FALSE, 0);
	gtk_entry_set_max_length (GTK_ENTRY (widget), 128);

	/* set the current title */
	g_object_get (priv->calibrate,
		      "description", &tmp,
		      NULL);
	gtk_entry_set_text (GTK_ENTRY (widget), tmp);

	/* watch for changes */
	g_signal_connect (GTK_EDITABLE (widget), "changed",
			  G_CALLBACK (gcm_calib_text_changed_cb), priv);

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Profile Title"));
	gtk_assistant_set_page_complete (assistant, vbox, TRUE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_TITLE));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_setup_page_sensor (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Insert sensor hardware"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, _("You need to insert sensor hardware to continue."));

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_CONTENT);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Sensor Check"));
	gtk_assistant_set_page_complete (assistant, vbox, FALSE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_SENSOR));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_setup_page_failure (GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	GtkWidget *content;
	GtkAssistant *assistant = GTK_ASSISTANT (priv->main_window);

	/* TRANSLATORS: this is the page title */
	vbox = gcm_calib_add_page_title (priv, _("Failed to calibrate"));

	/* main contents */
	content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);

	/* TRANSLATORS: this is intro page text */
	gcm_calib_add_page_para (content, _("The device could not be found. Ensure it is plugged in and turned on."));

	/* add to assistant */
	gtk_assistant_append_page (assistant, vbox);
	gtk_assistant_set_page_type (assistant, vbox, GTK_ASSISTANT_PAGE_SUMMARY);
	/* TRANSLATORS: this is the calibration wizard page title */
	gtk_assistant_set_page_title (assistant, vbox, _("Summary"));
	gtk_assistant_set_page_complete (assistant, vbox, TRUE);
	g_ptr_array_add (priv->pages, vbox);
	g_object_set_data (G_OBJECT (vbox),
			   "GcmCalibrateMain::Index",
			   GUINT_TO_POINTER (GCM_CALIBRATE_PAGE_FAILURE));

	/* show page */
	gtk_widget_show_all (vbox);
}

static void
gcm_calib_got_sensor (GcmCalibratePriv *priv, CdSensor *sensor)
{
	gboolean is_lowend = FALSE;
	GtkWidget *vbox;
	g_autoptr(GError) error = NULL;

	/* connect to sensor */
	if (!cd_sensor_connect_sync (sensor, NULL, &error)) {
		g_warning ("failed to connect to sensor: %s",
			   error->message);
		return;
	}
	gcm_calibrate_set_sensor (priv->calibrate, sensor);

	/* hide the prompt for the user to insert a sensor */
	vbox = gcm_calib_get_vbox_for_page (priv,
					    GCM_CALIBRATE_PAGE_SENSOR);
	gtk_widget_hide (vbox);

	/* if the device is a simple colorimeter, hide the temperature
	 * chooser. Only expensive accurate spectrophotometers are
	 * accurate enough to do a good job without a color cast */
	if (cd_sensor_get_kind (sensor) == CD_SENSOR_KIND_COLORHUG) {
		is_lowend = TRUE;
	}
	if (priv->device_kind == CD_DEVICE_KIND_DISPLAY) {
		vbox = gcm_calib_get_vbox_for_page (priv,
						    GCM_CALIBRATE_PAGE_DISPLAY_TEMPERATURE);
		gtk_widget_set_visible (vbox, !is_lowend);
	}
}

static void
gcm_calib_get_sensors_cb (GObject *object,
			  GAsyncResult *res,
			  gpointer user_data)
{
	CdClient *client = CD_CLIENT (object);
	CdSensor *sensor_tmp;
	GcmCalibratePriv *priv = (GcmCalibratePriv *) user_data;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) sensors = NULL;

	/* get the result */
	sensors = cd_client_get_sensors_finish (client, res, &error);
	if (sensors == NULL) {
		g_warning ("failed to get sensors: %s",
			   error->message);
		return;
	}

	/* we've got a sensor */
	if (sensors->len != 0) {
		sensor_tmp = g_ptr_array_index (sensors, 0);
		gcm_calib_got_sensor (priv, sensor_tmp);
	}
}

static void
gcm_calib_add_pages (GcmCalibratePriv *priv)
{
	gboolean ret;
	const gchar *xrandr_name;

	/* device not found */
	if (priv->device_kind == CD_DEVICE_KIND_UNKNOWN) {
		gcm_calib_setup_page_failure (priv);
		gtk_widget_show_all (GTK_WIDGET (priv->main_window));
		return;
	}

	gcm_calib_setup_page_intro (priv);

	if (priv->device_kind == CD_DEVICE_KIND_DISPLAY ||
	    priv->device_kind == CD_DEVICE_KIND_PRINTER)
		gcm_calib_setup_page_sensor (priv);

	/* find whether argyllcms is installed using a tool which should exist */
	ret = gcm_calibrate_get_enabled (priv->calibrate);
	if (!ret)
		gcm_calib_setup_page_install_argyllcms (priv);

	xrandr_name = cd_device_get_metadata_item (priv->device,
						   CD_DEVICE_METADATA_XRANDR_NAME);
	if (xrandr_name != NULL)
		priv->internal_lcd = gcm_utils_output_is_lcd_internal (xrandr_name);
	if (!priv->internal_lcd && priv->device_kind == CD_DEVICE_KIND_DISPLAY)
		gcm_calib_setup_page_display_configure_wait (priv);

	gcm_calib_setup_page_precision (priv);

	if (priv->device_kind == CD_DEVICE_KIND_DISPLAY) {
		gcm_calib_setup_page_display_kind (priv);
		gcm_calib_setup_page_display_temp (priv);
	} else if (priv->device_kind == CD_DEVICE_KIND_PRINTER) {
		gcm_calib_setup_page_print_kind (priv);
	} else {
		gcm_calib_setup_page_target_kind (priv);
		ret = g_file_test ("/usr/share/shared-color-targets", G_FILE_TEST_IS_DIR);
		if (!ret)
			gcm_calib_setup_page_install_targets (priv);
	}

	gcm_calib_setup_page_profile_title (priv);
	gcm_calib_setup_page_action (priv);

	gcm_calib_setup_page_summary (priv);

	/* see if we can hide the sensor check */
	cd_client_get_sensors (priv->client,
			       NULL,
			       gcm_calib_get_sensors_cb,
			       priv);
}

static void
gcm_calib_sensor_added_cb (CdClient *client, CdSensor *sensor, GcmCalibratePriv *priv)
{
	g_debug ("sensor inserted");
	gcm_calib_got_sensor (priv, sensor);
	gtk_assistant_next_page (GTK_ASSISTANT (priv->main_window));
}

static void
gcm_calib_startup_cb (GApplication *application, GcmCalibratePriv *priv)
{
	const gint window_width  = 640;
	const gint window_height = 440;
	const gchar *description;
	const gchar *manufacturer;
	const gchar *model;
	const gchar *native_device;
	const gchar *serial;
	gboolean ret;
	g_autofree gchar *copyright = NULL;
	g_autoptr(GDateTime) dt = NULL;
	g_autoptr(GError) error = NULL;

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   PKGDATADIR G_DIR_SEPARATOR_S "icons");

	/* connect to colord */
	priv->client = cd_client_new ();
	g_signal_connect (priv->client, "sensor-added",
			  G_CALLBACK (gcm_calib_sensor_added_cb), priv);
	ret = cd_client_connect_sync (priv->client,
				      NULL,
				      &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s", error->message);
		goto out;
	}

	/* show main UI */
	priv->main_window = GTK_WINDOW (gtk_assistant_new ());
	gtk_window_set_default_size (priv->main_window, window_width, window_height);
	gtk_window_set_resizable (priv->main_window, TRUE);
	gtk_window_set_title (priv->main_window, "");
	gtk_container_set_border_width (GTK_CONTAINER (priv->main_window), 12);
	g_signal_connect (priv->main_window, "delete_event",
			  G_CALLBACK (gcm_calib_delete_event_cb), priv);
	g_signal_connect (priv->main_window, "close",
			  G_CALLBACK (gcm_calib_assistant_close_cb), priv);
	g_signal_connect (priv->main_window, "cancel",
			  G_CALLBACK (gcm_calib_assistant_cancel_cb), priv);
	g_signal_connect (priv->main_window, "prepare",
			  G_CALLBACK (gcm_calib_assistant_prepare_cb), priv);
	gtk_application_add_window (priv->application,
				    priv->main_window);

	/* set the parent window if it is specified */
	if (priv->xid != 0) {
		g_debug ("Setting xid %u", priv->xid);
		gcm_window_set_parent_xid (GTK_WINDOW (priv->main_window), priv->xid);
	}

	/* select a specific profile only */
	priv->device = cd_client_find_device_sync (priv->client,
						    priv->device_id,
						    NULL,
						    &error);
	if (priv->device == NULL) {
		g_warning ("failed to get device %s: %s",
			   priv->device_id,
			   error->message);
		goto out;
	}

	/* connect to the device */
	ret = cd_device_connect_sync (priv->device,
				      NULL,
				      &error);
	if (!ret) {
		g_warning ("failed to connect to device: %s",
			   error->message);
		goto out;
	}

	/* get the device properties */
	priv->device_kind = cd_device_get_kind (priv->device);

	/* for display calibration move the window to lower right area to
         * limit argyll from obscuring the window (too much) */
	if (priv->device_kind == CD_DEVICE_KIND_DISPLAY) {
		gtk_window_set_gravity (priv->main_window, GDK_GRAVITY_SOUTH_EAST);
		gtk_window_move (priv->main_window, gdk_screen_width()  - window_width,
						     gdk_screen_height() - window_height);
	}

	/* set, with fallbacks */
	serial = cd_device_get_serial (priv->device);
	if (serial == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		serial = _("Unknown serial");
	}
	model = cd_device_get_model (priv->device);
	if (model == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		model = _("Unknown model");
	}
	description = cd_device_get_model (priv->device);
	if (description == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		description = _("Unknown description");
	}
	manufacturer = cd_device_get_vendor (priv->device);
	if (manufacturer == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		manufacturer = _("Unknown manufacturer");
	}

	dt = g_date_time_new_now_local ();
	/* TRANSLATORS: this is the copyright string, where it might be
	 * "Copyright (c) 2009 Edward Scissorhands"
	 * BIG RED FLASHING NOTE: YOU NEED TO USE ASCII ONLY */
	copyright = g_strdup_printf ("%s %04i %s", _("Copyright (c)"),
				     g_date_time_get_year (dt),
				     g_get_real_name ());

	/* set the proper values */
	g_object_set (priv->calibrate,
		      "device-kind", priv->device_kind,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      "serial", serial,
		      "copyright", copyright,
		      NULL);

	/* display specific properties */
	if (priv->device_kind == CD_DEVICE_KIND_DISPLAY) {
		native_device = cd_device_get_metadata_item (priv->device,
							     CD_DEVICE_METADATA_XRANDR_NAME);
		if (native_device == NULL) {
			g_warning ("failed to get output");
			goto out;
		}
		g_object_set (priv->calibrate,
			      "output-name", native_device,
			      NULL);
	}
out:
	/* add different pages depending on the device kind */
	gcm_calib_add_pages (priv);
	gtk_assistant_set_current_page (GTK_ASSISTANT (priv->main_window), 0);
}

static void
gcm_calib_title_changed_cb (GcmCalibrate *calibrate,
			    const gchar *title,
			    GcmCalibratePriv *priv)
{
	g_autofree gchar *markup = NULL;
	markup = g_strdup_printf ("<span size=\"large\" font_weight=\"bold\">%s</span>", title);
	gtk_label_set_markup (GTK_LABEL (priv->action_title), markup);
}

static void
gcm_calib_message_changed_cb (GcmCalibrate *calibrate,
			      const gchar *title,
			      GcmCalibratePriv *priv)
{
	gtk_label_set_label (GTK_LABEL (priv->action_message), title);
}

static void
gcm_calib_progress_changed_cb (GcmCalibrate *calibrate,
			       guint percentage,
			       GcmCalibratePriv *priv)
{
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->action_progress),
				       percentage / 100.0f);
}

static void
gcm_calib_image_changed_cb (GcmCalibrate *calibrate,
			    const gchar *filename,
			    GcmCalibratePriv *priv)
{
	g_autoptr(GError) error = NULL;

	if (filename != NULL) {
		g_autoptr(GdkPixbuf) pixbuf = NULL;
		pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 200, 400, &error);
		if (pixbuf == NULL) {
			g_warning ("failed to load image: %s", error->message);
			gtk_widget_hide (priv->action_image);
		} else {
			gtk_image_set_from_pixbuf (GTK_IMAGE (priv->action_image), pixbuf);
			gtk_widget_show (priv->action_image);
		}
	} else {
		gtk_widget_hide (priv->action_image);
	}
}

static void
gcm_calib_interaction_required_cb (GcmCalibrate *calibrate,
				   const gchar *button_text,
				   GcmCalibratePriv *priv)
{
	GtkWidget *vbox;
	vbox = gcm_calib_get_vbox_for_page (priv,
					    GCM_CALIBRATE_PAGE_ACTION);
	gtk_assistant_set_page_complete (GTK_ASSISTANT (priv->main_window),
					 vbox, TRUE);
	priv->has_pending_interaction = TRUE;
}

int
main (int argc, char **argv)
{
	gchar *device_id = NULL;
	GcmCalibratePriv *priv;
	GOptionContext *context;
	guint xid = 0;
	int status = 0;

	const GOptionEntry options[] = {
		{ "parent-window", 'p', 0, G_OPTION_ARG_INT, &xid,
		  /* TRANSLATORS: we can make this modal (stay on top of) another window */
		  _("Set the parent window to make this modal"), NULL },
		{ "device", 'd', 0, G_OPTION_ARG_STRING, &device_id,
		  /* TRANSLATORS: show just the one profile, rather than all */
		  _("Set the specific device to calibrate"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager calibration tool");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gcm_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	priv = g_new0 (GcmCalibratePriv, 1);
	priv->pages = g_ptr_array_new ();
	priv->xid = xid;
	priv->device_id = device_id;
	priv->calibrate = gcm_calibrate_argyll_new ();
	g_object_set (priv->calibrate,
		      "precision", GCM_CALIBRATE_PRECISION_LONG,
		      NULL);
	priv->device_kind = CD_DEVICE_KIND_UNKNOWN;
	g_signal_connect (priv->calibrate, "title-changed",
			  G_CALLBACK (gcm_calib_title_changed_cb), priv);
	g_signal_connect (priv->calibrate, "message-changed",
			  G_CALLBACK (gcm_calib_message_changed_cb), priv);
	g_signal_connect (priv->calibrate, "image-changed",
			  G_CALLBACK (gcm_calib_image_changed_cb), priv);
	g_signal_connect (priv->calibrate, "progress-changed",
			  G_CALLBACK (gcm_calib_progress_changed_cb), priv);
	g_signal_connect (priv->calibrate, "interaction-required",
			  G_CALLBACK (gcm_calib_interaction_required_cb), priv);

	/* nothing specified */
	if (priv->device_id == NULL) {
		g_print ("%s\n", _("No device was specified!"));
		goto out;
	}

	/* ensure single instance */
	priv->application = gtk_application_new ("org.gnome.ColorManager.Calibration", 0);
	g_signal_connect (priv->application, "startup",
			  G_CALLBACK (gcm_calib_startup_cb), priv);
	g_signal_connect (priv->application, "activate",
			  G_CALLBACK (gcm_calib_activate_cb), priv);

	/* wait */
	status = g_application_run (G_APPLICATION (priv->application), argc, argv);

	g_ptr_array_unref (priv->pages);
	g_object_unref (priv->application);
	g_object_unref (priv->calibrate);
	if (priv->client != NULL)
		g_object_unref (priv->client);
	if (priv->device_id != NULL)
		g_free (priv->device_id);
	if (priv->device != NULL)
		g_object_unref (priv->device);
	g_free (priv);
out:
	return status;
}
