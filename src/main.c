/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
    Grig:  Gtk+ user interface for the Hamradio Control Libraries.

    Copyright (C)  2001-2004  Alexandru Csete.

    Authors: Alexandru Csete <csete@users.sourceforge.net>

    Comments, questions and bugreports should be submitted via
    http://sourceforge.net/projects/groundstation/
    More details can be found at the project home page:

            http://groundstation.sourceforge.net/
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
  
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
  
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the
          Free Software Foundation, Inc.,
	  59 Temple Place, Suite 330,
	  Boston, MA  02111-1307
	  USA
*/
/** \file    main.c
 *  \ingroup main
 *  \bief    Main program file.
 *
 * Add some more text.
 *
 * \bug What do we do if we don't have getopt.h?
 *
 * \bug Debug level is not read from hamlib.
 */
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <hamlib/rig.h>
#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#endif
#include "grig-config.h"
#include "rig-gui.h"
#include "rig-daemon.h"
#include "rig-data.h"



/** \brief Main GUI application widget */
GtkWidget    *grigapp;


/* command line arguments */
/* FIXME: These need not be global!
    not in this baseline anyway.
 */
static gint     rignum    = 0;       /*!< Flag indicating which radio to use.*/
static gchar   *rigfile   = NULL;    /*!< The port where the rig is atached. */
static gchar   *civaddr   = NULL;    /*!< CI-V address for ICOM rig. */
static gint     rigspeed  = 0;       /*!< Optional serial speed. */
static gboolean listrigs  = FALSE;   /*!< List supported radios and exit. */ 
       gint     debug     = RIG_DEBUG_NONE; /*!< Hamlib debug level. */
static gboolean version   = FALSE;   /*!< Show version and exit. */
static gboolean help      = FALSE;   /*!< Show help and exit. */


/** \brief Short options. */
#define SHORT_OPTIONS "m:r:s:c:d:lhv"  /* group those which take no arg */

/** \brief Table of command line options. */
static struct option long_options[] =
{
	{"model",        1, 0, 'm'},
	{"rig-file",     1, 0, 'r'},
	{"speed",        1, 0, 's'},
	{"civaddr",      1, 0, 'c'},
	{"debug",        1, 0, 'd'},
	{"list",         0, 0, 'l'},
	{"help",         0, 0, 'h'},
	{"version",      0, 0, 'v'},
	{NULL, 0, 0, 0}
};


/** \brief Radio info to be used by list-rigs. */
typedef struct {
	gint    id;       /*!< Model ID. */
	gchar  *mfg;      /*!< Manufacurer name (eg. KENWOOD). */
	gchar  *model;    /*!< Radio model (eg. TS-440). */
	gchar  *version;  /*!< Driver version (eg. 0.3.2) */
        gint    status;   /*!< Driver status (0..5 use rig_strstatus). */
} grig_rig_info_t;


/* Private function prototypes */
static void        grig_list_rigs      (void);
static GtkWidget  *grig_app_create     (gint);
static gint        grig_app_delete     (GtkWidget *, GdkEvent *, gpointer);
static void        grig_app_destroy    (GtkWidget *, gpointer);
static void        grig_show_help      (void);
static void        grig_show_version   (void);
static gint        grig_list_add       (const struct rig_caps *, void *);
static gint        grig_list_compare   (gconstpointer, gconstpointer);



/** \bief Main program execution entry.
 *  \param argc The number o command line arguments.
 *  \param argv List of command line arguments.
 *  \return Execution status (non-zero mean error ocurred).
 *
 * Some description.
 *
 * \bug Add more text.
 *
 */
int
main (int argc, char *argv[])
{


	/* Initialize NLS support */
#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);
#endif

	gtk_set_locale ();
	gtk_init (&argc, &argv);
/* 	setlocale (LCNUMERIC, "C"); */


	/* initialize threads */
	if (!g_thread_supported ())
		g_thread_init (NULL);

	/* decode command line arguments; this part of the code only sets the
	   global flags and variables, whereafter we check each variable in
	   descending priority order. This way it is easy to exit the program
	   in case of -v -h and such.
	*/
	
	while(1) {
		int c;
		int option_index = 0;

		/* get next option */
		c = getopt_long (argc, argv, SHORT_OPTIONS,
			long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {

			/* set rig model*/
		case 'm':
			if (!optarg) {
				help = TRUE;
			}
			else {
				rignum = atoi (optarg);
			}
			break;

			/* set rig port */
		case 'r':
			if (!optarg) {
				help = TRUE;
			}
			else {
				rigfile = optarg;
			}
			break;

			/* set serial speed */
		case 's':
			if (!optarg) {
				help = TRUE;
			}
			else {
				rigspeed = atoi (optarg);
			}
			break;

			/* set CIV address */
		case 'c':
			if (!optarg) {
				help = TRUE;
			}
			else {
				civaddr = optarg;
			}
			break;

			/* list supported radios */
		case 'l':
			listrigs = TRUE;
			break;

			/* set debug level */
		case 'd':
			if (!optarg) {
				help = TRUE;
			}
			else {
				debug = atoi (optarg);
			}
			break;

			/* show help */
		case 'h':
			help = TRUE;
			break;

			/* show version */
		case 'v':
			version = TRUE;
			break;

			/* unknown option: show usage */
		default:
			help = TRUE;
			break;
		}
	}


	/* check command line option flags in decreasing
	   priority.
	*/
	if (help) {
		grig_show_help ();
		return 0;
	}

	if (version) {
		grig_show_version ();
		return 0;
	}

	if (listrigs) {
		grig_list_rigs ();
		return 0;
	}


	/* we set hamlib debug level to TRACE while we fire up the daemon;
	   it will be reset when we create the menubar
	*/
	rig_set_debug (RIG_DEBUG_TRACE);

	/* launch rig daemon */
	if (rig_daemon_start (rignum, rigfile, rigspeed, civaddr)) {
		return 1;
	}

	/* check whether the debug level is something meaningful
	   (it could be set to something junk by user); if yes, set
	   debuglevel, otherwise use RIG_DEBUG_WARN.
	*/
	if ((debug >= RIG_DEBUG_NONE) && (debug <= RIG_DEBUG_TRACE)) {
		rig_set_debug (debug);
	}
	else {
		rig_set_debug (RIG_DEBUG_WARN);
	}

	/* create application */
	grigapp = grig_app_create (rignum);

	/* add contents */
	gtk_container_add (GTK_CONTAINER (grigapp), rig_gui_create ());
	gtk_widget_show_all (grigapp);

	gtk_main ();

	return 0;
}





/** \brief Create and initialize main application window.
 *  \param rignum The index of the radio wich is controled by the app
 *  \return The new GnomeApp widget or NULL if the specified rig number
 *          is invalid
 *
 * This function creates and initializes a new application widget, adds
 * menubar and statusbar, etc...
 *
 * \bug Add more text
 */
static GtkWidget *
grig_app_create       (gint rignum)
{
	GtkWidget *app;        /* The main application */
	gchar     *title;      /* the window title  */
	gchar     *brand;
	gchar     *model;
	gchar     *icon;


	/* fixme: add rig_data_get_brand() and model () */
	brand = g_strdup ("Hamlib");
	model = g_strdup ("Dummy");

	/* construct title */
	title = g_strdup_printf (_("GRIG: %s %s"), brand, model);

	/* window icon file */
	icon = g_strconcat (PACKAGE_DATA_DIR, G_DIR_SEPARATOR_S, "pixmaps",
			    G_DIR_SEPARATOR_S, "ic910.png", NULL);

	/* create application */
	app = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (app), title);
	gtk_window_set_icon_from_file (GTK_WINDOW (app), icon, NULL);

	g_free (title);
	g_free (brand);
	g_free (model);
	g_free (icon);

	/* connect delete and destroy signals */
	g_signal_connect (G_OBJECT (app), "delete_event",
			  G_CALLBACK (grig_app_delete), NULL);    
	g_signal_connect (G_OBJECT (app), "destroy",
			  G_CALLBACK (grig_app_destroy), NULL);


	return app;
}



/** \brief Handle delete events.
 *  \param widget The widget which received the delete event signal.
 *  \param event  Data structure describing the event.
 *  \param data   User data (NULL).
 *  \param return Always FALSE to indicate that the app should be destroyed.
 *
 * This function handles the delete event received by the main application
 * window (eg. when the window is closed by the WM). This function simply
 * returns FALSE indicating that the main application window should be
 * destroyed by emiting the destroy signal.
 *
 */
static gint
grig_app_delete      (GtkWidget *widget,
		      GdkEvent  *event,
		      gpointer   data)
{

	/* return FALSE so that Gtk+ will emit the destroy signal */
	return FALSE;
}



/** \brief Handle destroy signals.
 *  \param widget The widget which received the signal.
 *  \param data   User data (NULL).
 *
 * This function is called when the main application window receives the
 * destroy signal, ie. it is destroyed. This function signals all daemons
 * and other threads to stop and exits the Gtk+ main loop.
 *
 */
static void
grig_app_destroy    (GtkWidget *widget,
		     gpointer   data)
{

	/* set debug level to TRACE */
	rig_set_debug (RIG_DEBUG_TRACE);

	/* stop daemons */
	rig_daemon_stop ();

	/* GUI timers are stopped automatically */

	/* stop timeouts */

	/* exit Gtk+ */
	gtk_main_quit ();
}



/** \brief Show help message.
 *
 * This function displays a brief help message for grig.
 */
static void
grig_show_help      ()
{

	g_print (_("Usage: grig [OPTION]...\n\n"));
	g_print (_("  -m, --model=ID           "\
		   "select radio model number; see --list\n"));
	g_print (_("  -r, --rig-file=DEVICE    "\
		   "set device of the radio, eg. /dev/ttyS0\n"));
	g_print (_("  -s, --speed=BAUD         "\
		   "set transfer rate (serial port only)\n"));
	g_print (_("  -c, --civaddr=ID         "\
		   "set CI-V address (decimal, ICOM only)\n"));
	g_print (_("  -d, --debug=LEVEL        "\
		   "set hamlib debug level (0..5)\n"));
	g_print (_("  -l, --list               "\
		   "list supported radios and exit\n"));
	g_print (_("  -h, --help               "\
		   "show this help message and exit\n"));
	g_print (_("  -v, --version            "\
		   "show version information and exit\n"));
	g_print ("\n");
	g_print (_("Example:"));
	g_print ("\n");
	g_print (_("Start grig using YAESU FT-990 connected to the first "\
		   "serial port, using 4800 baud and debug level set to "\
		   "warning:"));
	g_print ("\n\n");
	g_print ("     grig -m 116 -r /dev/ttyS0 -s 4800 -d 3");
	g_print ("\n\n");
	g_print (_("or if you prefer the long options:"));
	g_print ("\n\n");
	g_print ("     grig --model=116 --rig-file=/dev/ttyS0 "\
		 "--speed=4800 --debug=3");
	g_print ("\n\n");
	g_print (_("It is usually enough to specify the model "\
		   "ID and the DEVICE."));
	g_print ("\n\n");
	g_print (_("If you start grig without any options it "\
		   "will use the Dummy backend "\
		   "and set the debug level to RIG_DEBUG_NONE. "\
		   "If you don't specify "\
		   "the transfer rate for the serial port, the "\
		   "default value will be "\
		   "used by the backend and even if you specify "\
		   "a value, it can be "\
		   "overridden by the backend."));
	g_print ("\n\n");
}


/** \brief Show version info.
 *
 * This function shows the version information about grig.
 */
static void
grig_show_version   ()
{
	g_print (_("grig %s\n"), VERSION);
	g_print (_("Graphical User Interface for the "\
		   "Hamradio Control Libraries."));
	g_print ("\n\n");
	g_print (_("Copyright (C)  2001-2004  Alexandru Csete."));
	g_print ("\n");
	g_print (_("This is free software; see the source for "\
		   "copying conditions. "));
	g_print (_("There is NO warranty; not even for MERCHANTABILITY "
		   "or FITNESS FOR A PARTICULAR PURPOSE."));
	g_print ("\n");

}



/** \brief List rigs.
 *
 * This function lists the radios suported by hamlib. It shows the
 * manufacturer, model, driver version and driver status in a list
 * sorted by model number.
 *
 * The list of radios is obtained using the rig_list_foreach hamlib
 * function and storing each entry in a GArray. When all models have
 * been stored, the list is sorted by model number and printed.
 *
 * \bug Header string is not translated.
 *
 * \bug Should check retcode returned by rig_list_foreach.
 *
 * \sa grig_list_add, grig_list_compare
 */
static void
grig_list_rigs ()
{
	GArray *array;
	gint status;
	gint i;
	grig_rig_info_t *info;


	/* create the array */
	array = g_array_new (FALSE, FALSE, sizeof (grig_rig_info_t));

	/* make hamlib quiet and load all backends */
	rig_set_debug (RIG_DEBUG_NONE);
	rig_load_all_backends();

	/* fill list using rig_list_foreach */
	status = rig_list_foreach (grig_list_add, (void *) array);

	/* sort the array */
	g_array_sort (array, grig_list_compare);

	g_print ("\n");
	g_print ("   ID  Manufacurer      Model                  "\
		 "Ver.   Status\n");
	g_print ("-----------------------------------------------"\
		 "----------------\n");

	/* loop over each element of array; after printing one element
	   free the dynamically allocated strings because GArray does
	   not know about them
	*/
	for (i = 0; i < array->len; i++) {

		info = &g_array_index (array, grig_rig_info_t, i);

		g_print ("%5d  %-16s %-22s %-6s %s\n",
			 info->id,
			 info->mfg,
			 info->model,
			 info->version,
			 rig_strstatus (info->status));

		/* free dynamic strings */
		g_free (info->mfg);
		g_free (info->model);
		g_free (info->version);
	}
	g_print ("\n");

	g_array_free (array,TRUE);
}


/** \brief Add new entry to list of radios.
 *  \param caps Structure with the capablities of thecurrent radio.
 *  \param array Pointer to the GArray into which the new entry should be 
 *               stored.
 *  \return Always 1 to keep rig_list_foreach running.
 *
 * This function is called by the rig_list_foreach hamlib function for each
 * supported radio. It copies the relevant data into a grig_rig_info_t
 * structure and adds the new entry to the GArray containing the list of
 * supported radios.
 *
 * \sa grig_list_rigs, grig_list_compare
 */
static gint
grig_list_add (const struct rig_caps *caps, void *array)
{
	grig_rig_info_t *info;

	/* create new entry */
	info = g_malloc (sizeof (grig_rig_info_t));

	/* fill values */
	info->id      = caps->rig_model;
	info->mfg     = g_strdup (caps->mfg_name);
	info->model   = g_strdup (caps->model_name);
	info->version = g_strdup (caps->version);
	info->status  = caps->status;

	/* append new element to array */
	array = (void *) g_array_append_vals ((GArray *) array, info, 1);

	/* keep on running */
	return 1;
}



/** \brief Compare two rig info entries.
 *  \param a Pointer to the first entry.
 *  \param b Pointer to the second entry.
 *  \return Negative value if a < b; zero if a = b; positive value if a > b.
 *
 * This function is used to compare two rig entries in the list of radios
 * when the list is sorted. It compares the model ID of the two radios.
 *
 * \sa grig_list_rigs, grig_list_add
 */
static gint
grig_list_compare  (gconstpointer a, gconstpointer b)
{
	gint ida, idb;

	ida = ((grig_rig_info_t *) a)->id;
	idb = ((grig_rig_info_t *) b)->id;

	if (ida < idb) {
		return -1;
	}
	else if (ida > idb) {
		return 1;
	}
	else {
		return 0;
	}

}
