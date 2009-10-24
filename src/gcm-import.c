#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>

#include "egg-debug.h"

#include "gcm-profile.h"

#define GCM_STOCK_ICON		"mail-attachment"
#define GCM_PROFILE_PATH	"/.color/icc"

/**
 * gcm_import_get_destination:
 **/
static gchar *
gcm_import_get_destination (const gchar *filename)
{
	gchar *basename;
	gchar *destination;

	/* get destination filename for this source file */
	basename = g_path_get_basename (filename);
	destination = g_build_filename (g_get_home_dir (), GCM_PROFILE_PATH, basename, NULL);

	g_free (basename);
	return destination;
}

/**
 * gcm_import_copy_file:
 **/
static gboolean
gcm_import_copy_file (const gchar *filename, const gchar *destination, GError **error)
{
	gboolean ret;
	gchar *path;
	GFile *source;
	GFile *destfile;
	GFile *destpath;

	/* setup paths */
	source = g_file_new_for_path (filename);
	path = g_path_get_dirname (destination);
	destpath = g_file_new_for_path (path);
	destfile = g_file_new_for_path (destination);

	/* ensure desination exists */
	ret = g_file_test (path, G_FILE_TEST_EXISTS);
	if (!ret) {
		ret = g_file_make_directory_with_parents  (destfile, NULL, error);
		if (!ret)
			goto out;
	}

	/* do the copy */
	egg_debug ("copying from %s to %s", filename, path);
	ret = g_file_copy (source, destfile, G_FILE_COPY_NONE, NULL, NULL, NULL, error);
	if (!ret)
		goto out;
out:
	g_free (path);
	g_object_unref (source);
	g_object_unref (destpath);
	g_object_unref (destfile);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean ret;
	gboolean verbose = FALSE;
	gchar *copyright = NULL;
	gchar *description = NULL;
	gchar *destination = NULL;
	gchar **files = NULL;
	guint retval = 1;
	GcmProfile *profile = NULL;
	GError *error = NULL;
	GOptionContext *context;
	GString *string = NULL;
	GtkWidget *dialog;
	GtkResponseType response;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			"Show extra debugging information", NULL },
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
		  /* TRANSLATORS: command line option: a list of catalogs to install */
		  _("ICC profile to install"), NULL },
		{ NULL}
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager import program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	/* Save the usage string in case command parsing fails. */
	g_option_context_free (context);

	egg_debug_init (verbose);

	/* nothing sent */
	if (files == NULL) {
		/* TRANSLATORS: nothing was specified on the command line */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("No filename specified"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* load profile */
	profile = gcm_profile_new ();
	ret = gcm_profile_load (profile, files[0], &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Failed to open ICC profile"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		/* TRANSLATORS: parsing error */
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("Failed to parse file: %s"), error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		g_error_free (error);
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* get data */
	g_object_get (profile,
		      "description", &description,
		      "copyright", &copyright,
		      NULL);

	/* check file does't already exist */
	destination = gcm_import_get_destination (files[0]);
	ret = g_file_test (destination, G_FILE_TEST_EXISTS);
	if (ret) {
		/* TRANSLATORS: color profile already been installed */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("ICC profile already installed"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s\n%s", description, copyright);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* create message */
	string = g_string_new ("");
	if (description != NULL) {
		/* TRANSLATORS: message text */
		g_string_append_printf (string, _("Import ICC color profile %s?"), description);
	} else {
		/* TRANSLATORS: message text */
		g_string_append (string, _("Import ICC color profile?"));
	}

	/* add copyright */
	if (copyright != NULL)
		g_string_append_printf (string, "\n%s", copyright);

	/* ask confirmation */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CLOSE, "%s", _("Import ICC profile"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	/* TRANSLATORS: button text */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_OK);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	/* not the correct response */
	if (response != GTK_RESPONSE_OK)
		goto out;

	/* copy icc file to ~/.color/icc */
	ret = gcm_import_copy_file (files[0], destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Failed to copy file"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		g_error_free (error);
		gtk_widget_destroy (dialog);
		goto out;
	}

out:
	if (string != NULL)
		g_string_free (string, TRUE);
	if (profile != NULL)
		g_object_unref (profile);
	g_free (destination);
	g_free (description);
	g_free (copyright);
	g_strfreev (files);
	return retval;
}

