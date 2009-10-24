#include <gtk/gtk.h>
#include <math.h>

#include "egg-debug.h"

int
main (int argc, char **argv)
{
	gchar *options_help = NULL;
	GOptionContext *context;
	gboolean verbose = FALSE;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			"Show extra debugging information", NULL },
		{ NULL}
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("Program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	/* Save the usage string in case command parsing fails. */
	options_help = g_option_context_get_help (context, TRUE, NULL);
	g_option_context_free (context);

	egg_debug_init (verbose);

	g_free (options_help);
	return 0;
}

