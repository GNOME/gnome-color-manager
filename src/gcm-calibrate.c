#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <colord.h>

#include "gcm-calibrate.h"
#include "gcm-utils.h"
#include "gcm-brightness.h"
#include "gcm-exif.h"

static void     gcm_calibrate_finalize	(GObject     *object);

#define GCM_CALIBRATE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CALIBRATE, GcmCalibratePrivate))

/**
 * GcmCalibratePrivate:
 *
 * Private #GcmCalibrate data
 **/
struct _GcmCalibratePrivate
{
	GcmCalibrateDisplayKind		 display_kind;
	GcmCalibratePrintKind		 print_kind;
	GcmCalibratePrecision		 precision;
	GcmCalibrateReferenceKind	 reference_kind;
	CdSensor			*sensor;
	CdDeviceKind			 device_kind;
	CdColorXYZ			*xyz;
	gchar				*output_name;
	gchar				*filename_source;
	gchar				*filename_reference;
	gchar				*filename_result;
	gchar				*basename;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*description;
	gchar				*serial;
	gchar				*device;
	gchar				*working_path;
	guint				 target_whitepoint;
	GtkWidget			*content_widget;
	GPtrArray			*old_message;
	GPtrArray			*old_title;
};

enum {
	PROP_0,
	PROP_BASENAME,
	PROP_MODEL,
	PROP_DESCRIPTION,
	PROP_SERIAL,
	PROP_DEVICE,
	PROP_MANUFACTURER,
	PROP_REFERENCE_KIND,
	PROP_DISPLAY_KIND,
	PROP_PRINT_KIND,
	PROP_DEVICE_KIND,
	PROP_SENSOR_KIND,
	PROP_OUTPUT_NAME,
	PROP_FILENAME_SOURCE,
	PROP_FILENAME_REFERENCE,
	PROP_FILENAME_RESULT,
	PROP_WORKING_PATH,
	PROP_PRECISION,
	PROP_XYZ,
	PROP_TARGET_WHITEPOINT,
	PROP_LAST
};

enum {
	SIGNAL_TITLE_CHANGED,
	SIGNAL_MESSAGE_CHANGED,
	SIGNAL_IMAGE_CHANGED,
	SIGNAL_INTERACTION_REQUIRED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmCalibrate, gcm_calibrate, G_TYPE_OBJECT)

void
gcm_calibrate_set_content_widget (GcmCalibrate *calibrate, GtkWidget *widget)
{
	calibrate->priv->content_widget = widget;
}

GtkWidget *
gcm_calibrate_get_content_widget (GcmCalibrate *calibrate)
{
	return calibrate->priv->content_widget;
}

/**
 * gcm_calibrate_mkdir_with_parents:
 **/
static gboolean
gcm_calibrate_mkdir_with_parents (const gchar *filename, GError **error)
{
	gboolean ret;
	GFile *file = NULL;

	/* ensure desination exists */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		file = g_file_new_for_path (filename);
		ret = g_file_make_directory_with_parents (file, NULL, error);
		if (!ret)
			goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * gcm_calibrate_get_device_fallback:
 **/
gchar *
gcm_calibrate_get_profile_copyright (GcmCalibrate *calibrate)
{
	gchar *text;
	GDate *date = NULL;

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));

	/* TRANSLATORS: this is the copyright string, where it might be "Copyright (c) 2009 Edward Scissorhands" - YOU NEED TO USE ASCII ONLY */
	text = g_strdup_printf ("%s %04i %s", _("Copyright (c)"), date->year, g_get_real_name ());

	g_date_free (date);
	return text;
}

/**
 * gcm_calibrate_get_device_fallback:
 **/
gchar *
gcm_calibrate_get_profile_description (GcmCalibrate *calibrate)
{
	gchar *text;
	const gchar *description;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* we've got something set */
	if (priv->description != NULL) {
		description = priv->description;
	} else {
		/* TRANSLATORS: this is saved in the profile */
		description = _("Unknown description");
	}

	/* get description */
	text = g_strdup_printf ("%s, %s", priv->device, description);
	return text;
}

/**
 * gcm_calibrate_get_device_fallback:
 **/
gchar *
gcm_calibrate_get_profile_model (GcmCalibrate *calibrate)
{
	GcmCalibratePrivate *priv = calibrate->priv;

	/* we've got something set */
	if (priv->model != NULL)
		return g_strdup (priv->model);

	/* TRANSLATORS: this is saved in the profile */
	return g_strdup (_("Unknown model"));
}

/**
 * gcm_calibrate_get_device_fallback:
 **/
gchar *
gcm_calibrate_get_profile_manufacturer (GcmCalibrate *calibrate)
{
	GcmCalibratePrivate *priv = calibrate->priv;

	/* we've got something set */
	if (priv->manufacturer != NULL)
		return g_strdup (priv->manufacturer);

	/* TRANSLATORS: this is saved in the profile */
	return g_strdup (_("Unknown manufacturer"));
}

/**
 * gcm_calibrate_get_time:
 **/
static gchar *
gcm_calibrate_get_time (void)
{
	gchar *text;
	time_t c_time;

	/* get the time now */
	time (&c_time);
	text = g_new0 (gchar, 255);

	/* format text */
	strftime (text, 254, "%H-%M-%S", localtime (&c_time));
	return text;
}

/**
 * gcm_calibrate_get_filename_result:
 **/
const gchar *
gcm_calibrate_get_filename_result (GcmCalibrate *calibrate)
{
	return calibrate->priv->filename_result;
}

/**
 * gcm_calibrate_get_working_path:
 **/
const gchar *
gcm_calibrate_get_working_path (GcmCalibrate *calibrate)
{
	return calibrate->priv->working_path;
}

/**
 * gcm_calibrate_get_basename:
 **/
const gchar *
gcm_calibrate_get_basename (GcmCalibrate *calibrate)
{
	return calibrate->priv->basename;
}

/**
 * gcm_calibrate_set_basename:
 **/
static void
gcm_calibrate_set_basename (GcmCalibrate *calibrate)
{
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *timespec = NULL;
	GDate *date = NULL;
	GString *basename;

	/* get device properties */
	g_object_get (calibrate,
		      "serial", &serial,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      NULL);

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));
	timespec = gcm_calibrate_get_time ();

	/* form basename */
	basename = g_string_new ("GCM");
	if (manufacturer != NULL)
		g_string_append_printf (basename, " - %s", manufacturer);
	if (model != NULL)
		g_string_append_printf (basename, " - %s", model);
	if (serial != NULL)
		g_string_append_printf (basename, " - %s", serial);
	g_string_append_printf (basename, " (%04i-%02i-%02i)", date->year, date->month, date->day);

	/* Use time as we can calibrate more than once per day */
	g_string_append_printf (basename, " [%s]", timespec);

	/* save this */
	g_object_set (calibrate, "basename", basename->str, NULL);

	g_date_free (date);
	g_free (serial);
	g_free (manufacturer);
	g_free (model);
	g_free (timespec);
	g_string_free (basename, TRUE);
}

/**
 * gcm_calibrate_set_from_device:
 **/
gboolean
gcm_calibrate_set_from_device (GcmCalibrate *calibrate,
			       CdDevice *device,
			       CdSensor *sensor,
			       GError **error)
{
	gboolean ret = TRUE;
	const gchar *native_device = NULL;
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	const gchar *description = NULL;
	const gchar *serial = NULL;
	CdDeviceKind kind;

	/* get the device */
	kind = cd_device_get_kind (device);
	serial = cd_device_get_serial (device);
	model = cd_device_get_model (device);
	description = cd_device_get_model (device);
	manufacturer = cd_device_get_vendor (device);

	/* do not refcount */
	calibrate->priv->sensor = sensor;

	/* set the proper values */
	g_object_set (calibrate,
		      "device-kind", kind,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      "serial", serial,
		      NULL);

	/* get a filename based on calibration attributes we've just set */
	gcm_calibrate_set_basename (calibrate);

	/* display specific properties */
	if (kind == CD_DEVICE_KIND_DISPLAY) {
		native_device = cd_device_get_metadata_item (device, CD_DEVICE_METADATA_XRANDR_NAME);
		if (native_device == NULL) {
			g_set_error (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "failed to get output");
			ret = FALSE;
			goto out;
		}
		g_object_set (calibrate,
			      "output-name", native_device,
			      NULL);
	}

out:
	return ret;
}

/**
 * gcm_calibrate_set_from_exif:
 **/
gboolean
gcm_calibrate_set_from_exif (GcmCalibrate *calibrate, const gchar *filename, GError **error)
{
	const gchar *manufacturer;
	const gchar *model;
	const gchar *serial;
	gchar *description = NULL;
	gboolean ret;
	GcmExif *exif;
	GFile *file;

	/* parse file */
	exif = gcm_exif_new ();
	file = g_file_new_for_path (filename);
	ret = gcm_exif_parse (exif, file, error);
	if (!ret)
		goto out;

	/* get data */
	manufacturer = gcm_exif_get_manufacturer (exif);
	model = gcm_exif_get_model (exif);
	serial = gcm_exif_get_serial (exif);

	/* do the best we can */
	description = g_strdup_printf ("%s - %s", manufacturer, model);

	/* only set what we've got, don't nuke perfectly good device data */
	if (model != NULL)
		g_object_set (calibrate, "model", model, NULL);
	if (description != NULL)
		g_object_set (calibrate, "description", description, NULL);
	if (manufacturer != NULL)
		g_object_set (calibrate, "manufacturer", manufacturer, NULL);
	if (serial != NULL)
		g_object_set (calibrate, "serial", serial, NULL);

out:
	g_object_unref (file);
	g_object_unref (exif);
	g_free (description);
	return ret;
}

#if 0
	/* can this device support projectors? */
	if (priv->display_kind == GCM_CALIBRATE_DEVICE_KIND_PROJECTOR &&
	    !cd_sensor_has_cap (priv->sensor, CD_SENSOR_CAP_PROJECTOR)) {
		/* TRANSLATORS: title, the hardware calibration device does not support projectors */
		title = _("Could not calibrate and profile using this color measuring instrument");

		/* TRANSLATORS: dialog message */
		message = _("This color measuring instrument is not designed to support calibration and profiling projectors.");
	}
#endif

void
gcm_calibrate_set_sensor (GcmCalibrate *calibrate, CdSensor *sensor)
{
	/* do not refcount */
	calibrate->priv->sensor = sensor;
}

/**
 * gcm_calibrate_set_working_path:
 **/
static gboolean
gcm_calibrate_set_working_path (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = FALSE;
	gchar *timespec = NULL;
	gchar *folder = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* remove old value */
	g_free (priv->working_path);

	/* use the basename and the timespec */
	timespec = gcm_calibrate_get_time ();
	folder = g_strjoin (" - ", priv->basename, timespec, NULL);
	priv->working_path = g_build_filename (g_get_user_config_dir (), "gnome-color-manager", "calibration", folder, NULL);
	ret = gcm_calibrate_mkdir_with_parents (priv->working_path, error);
	g_free (timespec);
	g_free (folder);
	return ret;
}

/**
 * gcm_calibrate_spotread:
 **/
gboolean
gcm_calibrate_spotread (GcmCalibrate *calibrate, CdDevice *device, GtkWindow *window, GError **error)
{
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	GcmCalibratePrivate *priv = calibrate->priv;
	gboolean ret;

	/* coldplug source */
	if (klass->calibrate_spotread == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_spotread (calibrate, device, priv->sensor, window, error);
out:
	return ret;
}

/**
 * gcm_calibrate_interaction:
 **/
void
gcm_calibrate_interaction (GcmCalibrate *calibrate, GtkResponseType response)
{
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);

	/* coldplug source */
	if (klass->interaction == NULL)
		return;

	/* proxy */
	klass->interaction (calibrate, response);
}

/**
 * gcm_calibrate_display:
 **/
gboolean
gcm_calibrate_display (GcmCalibrate *calibrate, CdDevice *device, GtkWindow *window, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	GcmCalibratePrivate *priv = calibrate->priv;

	/* set basename */
	gcm_calibrate_set_basename (calibrate);
#if 0
	const gchar *hardware_device;
	/* get device, harder */
	hardware_device = cd_sensor_get_model (priv->sensor);
	if (hardware_device == NULL) {
		/* TRANSLATORS: this is the formattted custom profile description.
		 * "Custom" refers to the fact that it's user generated */
		hardware_device = _("Custom");
	}
#endif

	/* coldplug source */
	if (klass->calibrate_display == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_display (calibrate, device, priv->sensor, window, error);
out:
	return ret;
}

/**
 * gcm_calibrate_device_get_reference_image:
 **/
static gchar *
gcm_calibrate_device_get_reference_image (const gchar *directory, GtkWindow *window)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* TRANSLATORS: dialog for file->open dialog. A calibration target image is the
	 * aquired image of the calibration target, e.g. an image file that looks
	 * a bit like this: http://www.colorreference.de/targets/target.jpg */
	dialog = gtk_file_chooser_dialog_new (_("Select calibration target image"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), directory);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "image/tiff");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("Supported images files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * gcm_calibrate_device_get_reference_data:
 **/
static gchar *
gcm_calibrate_device_get_reference_data (const gchar *directory, GtkWindow *window)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select CIE reference values file"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), directory);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "application/x-it87");

	/* we can remove this when we depend on a new shared-mime-info */
	gtk_file_filter_add_pattern (filter, "*.txt");
	gtk_file_filter_add_pattern (filter, "*.TXT");
	gtk_file_filter_add_pattern (filter, "*.it8");
	gtk_file_filter_add_pattern (filter, "*.IT8");

	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("CIE values"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * gcm_calibrate_get_device_for_it8_file:
 **/
static gchar *
gcm_calibrate_get_device_for_it8_file (const gchar *filename)
{
	gchar *contents = NULL;
	gchar **lines = NULL;
	gchar *device = NULL;
	gboolean ret;
	GError *error = NULL;
	guint i;

	/* get contents */
	ret = g_file_get_contents (filename, &contents, NULL, &error);
	if (!ret) {
		g_warning ("failed to get contents: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* split */
	lines = g_strsplit (contents, "\n", 15);
	for (i=0; lines[i] != NULL; i++) {
		if (!g_str_has_prefix (lines[i], "ORIGINATOR"))
			continue;

		/* copy, without the header or double quotes */
		device = g_strdup (lines[i]+12);
		g_strdelimit (device, "\"", '\0');
		break;
	}
out:
	g_free (contents);
	g_strfreev (lines);
	return device;
}

/**
 * gcm_calibrate_file_chooser_get_working_path:
 **/
static gchar *
gcm_calibrate_file_chooser_get_working_path (GcmCalibrate *calibrate, GtkWindow *window)
{
	GtkWidget *dialog;
	gchar *current_folder;
	gchar *working_path = NULL;

	/* start in the correct place */
	current_folder = g_build_filename (g_get_user_config_dir (), "gnome-color-manager", "calibration", NULL);

	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select ICC Profile File"), window,
					       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       _("Open"), GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), current_folder);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		working_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	g_free (current_folder);
	return working_path;
}

/**
 * gcm_calibrate_printer:
 **/
gboolean
gcm_calibrate_printer (GcmCalibrate *calibrate, CdDevice *device, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	gchar *ptr;
	GtkWindow *window_tmp = NULL;
	gchar *precision = NULL;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	GcmCalibratePrivate *priv = calibrate->priv;

	/* copy */
	g_object_get (NULL, "print-kind", &priv->print_kind, NULL);

	if (priv->print_kind != GCM_CALIBRATE_PRINT_KIND_ANALYZE) {
		/* set the per-profile filename */
		ret = gcm_calibrate_set_working_path (calibrate, error);
		if (!ret)
			goto out;
	} else {

		/* remove previously set value (if any) */
		g_free (priv->working_path);
		priv->working_path = NULL;

		/* get from the user */
		priv->working_path = gcm_calibrate_file_chooser_get_working_path (calibrate, window_tmp);
		if (priv->working_path == NULL) {
			g_set_error_literal (error,
					     GCM_CALIBRATE_ERROR,
					     GCM_CALIBRATE_ERROR_USER_ABORT,
					     "user did not choose folder");
			ret = FALSE;
			goto out;
		}

		/* reprogram the basename */
		g_free (priv->basename);
		priv->basename = g_path_get_basename (priv->working_path);

		/* remove the timespec */
		ptr = g_strrstr (priv->basename, " - ");
		if (ptr != NULL)
			ptr[0] = '\0';
	}

	/* coldplug source */
	if (klass->calibrate_printer == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_printer (calibrate, device, priv->sensor, window, error);
out:
	g_free (precision);
	return ret;
}

/**
 * gcm_calibrate_device:
 **/
gboolean
gcm_calibrate_device (GcmCalibrate *calibrate, CdDevice *device, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	gboolean has_shared_targets = TRUE;
	gchar *reference_image = NULL;
	gchar *reference_data = NULL;
	gchar *device_str = NULL;
	const gchar *directory;
	GString *string;
	GtkWindow *window_tmp = NULL;
	gchar *precision = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);

	string = g_string_new ("");

	/* get scanned image */
	directory = g_get_home_dir ();
	reference_image = gcm_calibrate_device_get_reference_image (directory, window_tmp);
	if (reference_image == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "could not get calibration target image");
		ret = FALSE;
		goto out;
	}

	/* use the exif data if there is any present */
	ret = gcm_calibrate_set_from_exif (calibrate, reference_image, NULL);
	if (!ret)
		g_debug ("no EXIF data, so using device attributes");

	/* get reference data */
	directory = has_shared_targets ? "/usr/share/color/targets" : "/media";
	reference_data = gcm_calibrate_device_get_reference_data (directory, window_tmp);
	if (reference_data == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "could not get reference target");
		ret = FALSE;
		goto out;
	}

	/* use the ORIGINATOR in the it8 file */
	device_str = gcm_calibrate_get_device_for_it8_file (reference_data);
	if (device_str == NULL)
		device_str = g_strdup ("IT8.7");

	/* set the calibration parameters */
	g_object_set (calibrate,
		      "filename-source", reference_image,
		      "filename-reference", reference_data,
		      "device", device,
		      NULL);

	/* coldplug source */
	if (klass->calibrate_device == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_device (calibrate, device, priv->sensor, window, error);
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	g_free (precision);
	g_free (device_str);
	g_free (reference_image);
	g_free (reference_data);
	return ret;
}

void
gcm_calibrate_set_title (GcmCalibrate *calibrate, const gchar *title)
{
	g_signal_emit (calibrate, signals[SIGNAL_TITLE_CHANGED], 0, title);
	g_ptr_array_add (calibrate->priv->old_title, g_strdup (title));
}

void
gcm_calibrate_set_message (GcmCalibrate *calibrate, const gchar *message)
{
	g_signal_emit (calibrate, signals[SIGNAL_MESSAGE_CHANGED], 0, message);
	g_ptr_array_add (calibrate->priv->old_message, g_strdup (message));
}

void
gcm_calibrate_set_image (GcmCalibrate *calibrate, const gchar *filename)
{
	g_signal_emit (calibrate, signals[SIGNAL_IMAGE_CHANGED], 0, filename);
}

void
gcm_calibrate_interaction_required (GcmCalibrate *calibrate, const gchar *button_text)
{
	g_signal_emit (calibrate, signals[SIGNAL_INTERACTION_REQUIRED], 0, button_text);
}

void
gcm_calibrate_pop (GcmCalibrate *calibrate)
{
	const gchar *tmp;
	tmp = g_ptr_array_index (calibrate->priv->old_title, calibrate->priv->old_title->len - 2);
	g_signal_emit (calibrate, signals[SIGNAL_TITLE_CHANGED], 0, tmp);
	tmp = g_ptr_array_index (calibrate->priv->old_message, calibrate->priv->old_message->len - 2);
	g_signal_emit (calibrate, signals[SIGNAL_MESSAGE_CHANGED], 0, tmp);
}

/**
 * gcm_calibrate_get_property:
 **/
static void
gcm_calibrate_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	switch (prop_id) {
	case PROP_REFERENCE_KIND:
		g_value_set_uint (value, priv->reference_kind);
		break;
	case PROP_DEVICE_KIND:
		g_value_set_uint (value, priv->device_kind);
		break;
	case PROP_PRINT_KIND:
		g_value_set_uint (value, priv->print_kind);
		break;
	case PROP_DISPLAY_KIND:
		g_value_set_uint (value, priv->display_kind);
		break;
	case PROP_SENSOR_KIND:
		g_assert (priv->sensor != NULL);
		g_value_set_uint (value, cd_sensor_get_kind (priv->sensor));
		break;
	case PROP_OUTPUT_NAME:
		g_value_set_string (value, priv->output_name);
		break;
	case PROP_FILENAME_SOURCE:
		g_value_set_string (value, priv->filename_source);
		break;
	case PROP_FILENAME_REFERENCE:
		g_value_set_string (value, priv->filename_reference);
		break;
	case PROP_FILENAME_RESULT:
		g_value_set_string (value, priv->filename_result);
		break;
	case PROP_BASENAME:
		g_value_set_string (value, priv->basename);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, priv->serial);
		break;
	case PROP_DEVICE:
		g_value_set_string (value, priv->device);
		break;
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_WORKING_PATH:
		g_value_set_string (value, priv->working_path);
		break;
	case PROP_PRECISION:
		g_value_set_uint (value, priv->precision);
		break;
	case PROP_XYZ:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, priv->xyz));
		break;
	case PROP_TARGET_WHITEPOINT:
		g_value_set_uint (value, priv->target_whitepoint);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


/**
 * gcm_calibrate_set_property:
 **/
static void
gcm_calibrate_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	switch (prop_id) {
	case PROP_OUTPUT_NAME:
		g_free (priv->output_name);
		priv->output_name = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_SOURCE:
		g_free (priv->filename_source);
		priv->filename_source = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_REFERENCE:
		g_free (priv->filename_reference);
		priv->filename_reference = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_RESULT:
		g_free (priv->filename_result);
		priv->filename_result = g_strdup (g_value_get_string (value));
		break;
	case PROP_BASENAME:
		g_free (priv->basename);
		priv->basename = g_strdup (g_value_get_string (value));
		gcm_utils_ensure_sensible_filename (priv->basename);
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		g_free (priv->description);
		priv->description = g_strdup (g_value_get_string (value));
		break;
	case PROP_SERIAL:
		g_free (priv->serial);
		priv->serial = g_strdup (g_value_get_string (value));
		break;
	case PROP_DEVICE:
		g_free (priv->device);
		priv->device = g_strdup (g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		g_free (priv->manufacturer);
		priv->manufacturer = g_strdup (g_value_get_string (value));
		break;
	case PROP_DEVICE_KIND:
		priv->device_kind = g_value_get_uint (value);
		break;
	case PROP_PRECISION:
		priv->precision = g_value_get_uint (value);
		break;
	case PROP_DISPLAY_KIND:
		priv->display_kind = g_value_get_uint (value);
		break;
	case PROP_WORKING_PATH:
		g_free (priv->working_path);
		priv->working_path = g_strdup (g_value_get_string (value));
		break;
	case PROP_XYZ:
		if (priv->xyz != NULL)
			cd_color_xyz_free (priv->xyz);
		priv->xyz = g_boxed_copy (CD_TYPE_COLOR_XYZ, value);
		break;
	case PROP_TARGET_WHITEPOINT:
		priv->target_whitepoint = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_class_init:
 **/
static void
gcm_calibrate_class_init (GcmCalibrateClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_calibrate_finalize;
	object_class->get_property = gcm_calibrate_get_property;
	object_class->set_property = gcm_calibrate_set_property;

	pspec = g_param_spec_uint ("reference-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_REFERENCE_KIND, pspec);

	pspec = g_param_spec_uint ("display-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DISPLAY_KIND, pspec);

	pspec = g_param_spec_uint ("print-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRINT_KIND, pspec);

	pspec = g_param_spec_uint ("device-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE_KIND, pspec);

	pspec = g_param_spec_uint ("sensor-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SENSOR_KIND, pspec);

	pspec = g_param_spec_string ("output-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OUTPUT_NAME, pspec);

	pspec = g_param_spec_string ("filename-source", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_SOURCE, pspec);

	pspec = g_param_spec_string ("filename-reference", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_REFERENCE, pspec);

	pspec = g_param_spec_string ("filename-result", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_RESULT, pspec);

	pspec = g_param_spec_string ("basename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BASENAME, pspec);

	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	pspec = g_param_spec_string ("serial", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SERIAL, pspec);

	pspec = g_param_spec_string ("device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE, pspec);

	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	pspec = g_param_spec_string ("working-path", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WORKING_PATH, pspec);

	pspec = g_param_spec_uint ("precision", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_PRECISION, pspec);

	pspec = g_param_spec_boxed ("xyz", NULL, NULL,
				    CD_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_XYZ, pspec);

	pspec = g_param_spec_uint ("target-whitepoint", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TARGET_WHITEPOINT, pspec);

	signals[SIGNAL_TITLE_CHANGED] =
		g_signal_new ("title-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, title_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[SIGNAL_MESSAGE_CHANGED] =
		g_signal_new ("message-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, message_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[SIGNAL_IMAGE_CHANGED] =
		g_signal_new ("image-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, image_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[SIGNAL_INTERACTION_REQUIRED] =
		g_signal_new ("interaction-required",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, interaction_required),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (GcmCalibratePrivate));
}

/**
 * gcm_calibrate_init:
 **/
static void
gcm_calibrate_init (GcmCalibrate *calibrate)
{
	calibrate->priv = GCM_CALIBRATE_GET_PRIVATE (calibrate);
	calibrate->priv->xyz = cd_color_xyz_new ();

	calibrate->priv->print_kind = GCM_CALIBRATE_PRINT_KIND_UNKNOWN;
	calibrate->priv->reference_kind = GCM_CALIBRATE_REFERENCE_KIND_UNKNOWN;
	calibrate->priv->precision = GCM_CALIBRATE_PRECISION_UNKNOWN;

	// FIXME: this has to be per-run specific
	calibrate->priv->working_path = g_strdup ("/tmp");
	calibrate->priv->old_title = g_ptr_array_new_with_free_func (g_free);
	calibrate->priv->old_message = g_ptr_array_new_with_free_func (g_free);
}

/**
 * gcm_calibrate_finalize:
 **/
static void
gcm_calibrate_finalize (GObject *object)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	g_free (priv->filename_source);
	g_free (priv->filename_reference);
	g_free (priv->filename_result);
	g_free (priv->output_name);
	g_free (priv->basename);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->description);
	g_free (priv->device);
	g_free (priv->serial);
	g_free (priv->working_path);
	cd_color_xyz_free (priv->xyz);
	g_ptr_array_unref (calibrate->priv->old_title);
	g_ptr_array_unref (calibrate->priv->old_message);

	G_OBJECT_CLASS (gcm_calibrate_parent_class)->finalize (object);
}

/**
 * gcm_calibrate_new:
 *
 * Return value: a new GcmCalibrate object.
 **/
GcmCalibrate *
gcm_calibrate_new (void)
{
	GcmCalibrate *calibrate;
	calibrate = g_object_new (GCM_TYPE_CALIBRATE, NULL);
	return GCM_CALIBRATE (calibrate);
}
