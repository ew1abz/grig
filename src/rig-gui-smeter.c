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
/** \file rig-gui-smeter.c
 *  \ingroup smeter
 *  \brief Signal strength meter widget.
 *
 * The signal strength meter is implemented using a GtkDrawingArea widget
 * and using the GDK drawing primitives to draw the background pixmap and
 * the needle onto the GdkWindow of the drawing area.
 *
 * The s-meter widget contains also combo boxes for selection of the meter
 * scale and meter mode when the rig is in TX mode.
 *
 * \bug Only the signal strength meterworks (RX).
 */
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>
#include <math.h>
#include "rig-data.h"
#include "rig-gui-smeter-conv.h"
#include "rig-gui-smeter.h"



//#define SMETER_TEST 1

/* smeter */
static smeter_t smeter;

/* needle coordinates - can be made local */
static coordinate_t coor;


/* offscreen drawable */
static GdkPixmap *buffer;


/* TX mode strings used for optionmenu */
static const gchar *TX_MODE_S[] = {
	N_("None"),
	N_("Power"),
	N_("SWR"),
	N_("ALC"),
	N_("Comp."),
	N_("IC")
};


/* TX scale strings used for optionmenu */
static const gchar *TX_SCALE_S[] = {
	N_("0..5"),
	N_("0..10"),
	N_("0..50"),
	N_("0..100"),
	N_("0..500")
};


/* private fuunction prototypes */
static void       rig_gui_smeter_create_canvas  (void);
static GtkWidget *rig_gui_mode_selector_create  (void);
static GtkWidget *rig_gui_scale_selector_create (void);

static gint rig_gui_smeter_timeout_exec  (gpointer);
static gint rig_gui_smeter_timeout_stop  (gpointer);

static void rig_gui_smeter_mode_cb     (GtkWidget *, gpointer);
static void rig_gui_smeter_scale_cb    (GtkWidget *, gpointer);

static gboolean rig_gui_smeter_expose_cb   (GtkWidget *, GdkEventExpose *, gpointer);


/** \brief Create signal strength meter widget.
 *  \return A mega widget containing the signal strength meter.
 *
 * This function creates and initializes the signal strength meter
 * and the other related widgets: the combo box, which is used to
 * select the functionality of the meter and the combo box used to
 * select the scale of the meter.
 */
GtkWidget *
rig_gui_smeter_create ()
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	guint      timerid;


	/* initialize some data */
	smeter.value     = convert_db_to_angle (-54, DB_TO_ANGLE_MODE_POLY);
	smeter.lastvalue = smeter.value;
	smeter.tval      = RIG_GUI_SMETER_DEF_TVAL;
	smeter.falloff   = RIG_GUI_SMETER_DEF_FALLOFF;
	smeter.txmode    = SMETER_TX_MODE_NONE;
	smeter.scale     = SMETER_SCALE_100;
	smeter.exposed   = FALSE;

	/* create horizontal box containing selectors */
	hbox = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), rig_gui_scale_selector_create ());
	gtk_box_pack_start_defaults (GTK_BOX (hbox), rig_gui_mode_selector_create ());

	/* disable for now (unsupported) */
	gtk_widget_set_sensitive (hbox, FALSE);

	/* create cnvas */
	rig_gui_smeter_create_canvas ();


	/* create vertical box */
	vbox = gtk_vbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), smeter.canvas, FALSE, FALSE, 5);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 0);

	/* start readback timer but only if service is available */
	if (rig_data_has_get_strength ()) {
		timerid = g_timeout_add (RIG_GUI_SMETER_DEF_TVAL,
					 rig_gui_smeter_timeout_exec,
					 NULL);

		/* register timer_stop function at exit */
		gtk_quit_add (gtk_main_level (), rig_gui_smeter_timeout_stop,
			      GUINT_TO_POINTER (timerid));
	}

	return vbox;
}


/** \brief Create canvas widget.
 *
 * This function creates the drawing area widget, loads the background pixmap and
 * creates the needle. The background pixmap is stored in
 * the global smeter structure as a GdkPixbuf to allow redrawing of the meter
 * when needed.
 *
 * \bug drawing area is created with hard-coded size.
 *
 */
static void
rig_gui_smeter_create_canvas ()
{
	gchar             *fname;

	/* create canvas */
	smeter.canvas = gtk_drawing_area_new ();
	gtk_widget_set_size_request (smeter.canvas, 160, 80);

	/* connect expose handler which will take care of adding
	   contents.
	*/
	g_signal_connect (G_OBJECT (smeter.canvas), "expose_event",  
			  G_CALLBACK (rig_gui_smeter_expose_cb), NULL);	

	/* create background pixmap and add it to canvas */
	fname = g_strconcat (PACKAGE_DATA_DIR, G_DIR_SEPARATOR_S, "pixmaps",
			     G_DIR_SEPARATOR_S, "smeter.png", NULL);
	smeter.pixbuf = gdk_pixbuf_new_from_file (fname, NULL);
	g_free (fname);

	/* get initial cordinates */
	convert_angle_to_rect (smeter.value, &coor);
					       
}


/** \brief Execute timeout function.
 *  \param data User data; currently NULL.
 *  \return Always TRUE to keep the timer running.
 *
 * This function is in charge for updating the signal strength meter. It acquires
 * the signal strength from the rig-data object, converts it to needle endpoint
 * coordinates and repaints the s-meter.
 *
 * The function is called peridically by the Gtk+ scheduler.
 *
 * \note Optimize?
 */
static gint 
rig_gui_smeter_timeout_exec  (gpointer data)
{
	gfloat             rdang;  /* angle obtained from rig-data */
	gint               db;
	gfloat             maxdelta;
	gfloat             delta;



	/* are we in RX or TX mode? */
	if (rig_data_get_ptt () == RIG_PTT_OFF) {

		/* get current value from rig-data */
		db = rig_data_get_strength ();

#ifdef SMETER_TEST
		/* test s-meter with random numbers */
		db = (gint) g_random_int_range (-100, 100);
#endif

		rdang = convert_db_to_angle (db, DB_TO_ANGLE_MODE_POLY);

		delta = fabs (rdang - smeter.value);
	}
	else {
		/* TX mode; use -54dB */
		db = -54;
		rdang = convert_db_to_angle (db, DB_TO_ANGLE_MODE_POLY);

		delta = fabs (rdang - smeter.value);
	}

	/* is there a significant change? */
	if (delta > 0.1) {

		/* calculate max delta = deg/sec * sec  */
		maxdelta = smeter.falloff * (smeter.tval * 0.001);
		
		smeter.lastvalue = smeter.value;
			
		/* check whether the delta is less than what the falloff allows */
		if (delta < maxdelta) {
			smeter.value = rdang;
		}

		/* otherwise use maxdelta */
		else {
			if (rdang > smeter.value) {
				smeter.value += maxdelta;
			}
			else {
				smeter.value -= maxdelta;
			}
		}

		/* update widget */
		convert_angle_to_rect (smeter.value, &coor);
 
		/* checkwhether s-meter is visible */
		if (smeter.exposed) {

			/* raw background pixmap */
			gdk_draw_pixbuf (GDK_DRAWABLE (buffer), NULL, smeter.pixbuf,
					 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);

			/* draw needle */
			gdk_draw_line (GDK_DRAWABLE (buffer), smeter.gc,
				       coor.x1, coor.y1, coor.x2, coor.y2);

			/* draw border around the meter */
			gdk_draw_rectangle (GDK_DRAWABLE (buffer), smeter.gc,
					    FALSE, 0, 0, 160, 80);

			/* copy offscreen buffer to visible widget */
			gdk_draw_drawable (GDK_DRAWABLE (smeter.canvas->window), smeter.gc,
					   GDK_DRAWABLE (buffer),
					   0, 0, 0, 0, -1, -1);
		}
	}


	return TRUE;
}



/** \brief Stop timeout function.
 *  \param timer The ID of the timer to stop.
 *  \return Always TRUE.
 *
 * This function is used to stop the readback timer just before the
 * program is quit. It should be called automatically by Gtk+ when
 * the gtk_main_loop is exited.
 */
static gint 
rig_gui_smeter_timeout_stop  (gpointer timer)
{

	g_source_remove (GPOINTER_TO_UINT (timer));

	return TRUE;
}


/** \brief Create TX display mode selector widget.
 *  \return The mode selctor widget.
 *
 * This function creates the combo box which is used to select the function of the s-meter
 * in TX mode (Power, SWR, ALC, ...).
 */
static GtkWidget *
rig_gui_mode_selector_create  ()
{
	GtkWidget *combo;
	guint i;

	combo = gtk_combo_box_new_text ();

	/* Add entries to combo box */
	for (i = SMETER_TX_MODE_NONE; i < SMETER_TX_MODE_LAST; i++) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), TX_MODE_S[i]);
	}

	/* temporary disable */
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), SMETER_TX_MODE_NONE);

	/* connect changed signal */
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (rig_gui_smeter_mode_cb),
			  NULL);

	return combo;
}


/** \brief Create scale selector widget.
 *  \return The scale selector widget.
 *
 * This function is used to create the combo box whih can be used to select the
 * scale/range of the s-meter in TX mode.
 */
static GtkWidget *
rig_gui_scale_selector_create ()
{
	GtkWidget *combo;
	guint i;

	combo = gtk_combo_box_new_text ();

	/* Add entries to combo box */
	for (i = SMETER_SCALE_5; i < SMETER_SCALE_LAST; i++) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), TX_SCALE_S[i]);
	}

	/* temporary disable */
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), SMETER_SCALE_100);

	/* connect changed signal */
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (rig_gui_smeter_scale_cb),
			  NULL);

	return combo;
}


/** \brief Select s-meter mode.
 *  \param widget The widget which received the signal.
 *  \param data   User data, always NULL.
 *
 * This function is called when the user selects a new mode for the s-meter.
 * It acquires the selected menu item, and stores the corresponding display
 * mode in the s-meter structure.
 */
static void
rig_gui_smeter_mode_cb   (GtkWidget *widget, gpointer data)
{
	gint index;

	/* get selected item */
	index = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	/* store the mode if value is self-consistent */
	if ((index > -1) && (index < SMETER_TX_MODE_LAST)) {
		smeter.txmode = index;
	}

}


/** \brief Select s-meter range.
 *  \param widget The widget which received the signal.
 *  \param data   User data, always NULL.
 *
 * This function is called when the user selects a new range for the s-meter.
 * It acquires the selected menu item, and stores the corresponding display
 * mode in the s-meter structure.
 */
static void
rig_gui_smeter_scale_cb   (GtkWidget *widget, gpointer data)
{
	gint index;

	/* get selected item */
	index = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	/* store the mode if value is self-consistent */
	if ((index > -1) && (index < SMETER_SCALE_LAST)) {
		smeter.scale = index;
	}

}



/** \brief Handle expose events for the drawing area.
 *  \param widget The drawing area widget.
 *  \param event  The event.
 *  \param data   User data; always NULL.
 * 
 * This function is called when the rawing area widget is finalized
 * and exposed. Itis used to finish the initialization of those
 * parameters, which need attributes rom visible widgets.
 */ 
static gboolean
rig_gui_smeter_expose_cb   (GtkWidget      *widget,
			    GdkEventExpose *event,
			    gpointer        data)
{
	GdkColor color;

	/* draw background pixmap */
	gdk_draw_pixbuf (GDK_DRAWABLE (widget->window), NULL, smeter.pixbuf,
			 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);

	/* 0x3b3428 scaled to 3x16 bits */
	color.red = 257*0x5B;
	color.green = 257*0x54;
	color.blue = 257*0x48;

	/* finalize the graphics context */
	smeter.gc = gdk_gc_new (GDK_DRAWABLE (widget->window));
	gdk_gc_set_rgb_fg_color (smeter.gc, &color);
	gdk_gc_set_rgb_bg_color (smeter.gc, &color);
	gdk_gc_set_line_attributes (smeter.gc, 2,
				    GDK_LINE_SOLID,
				    GDK_CAP_ROUND,
				    GDK_JOIN_ROUND);
				    
	/* draw needle */
	gdk_draw_line (GDK_DRAWABLE (widget->window), smeter.gc,
		       coor.x1, coor.y1, coor.x2, coor.y2);

	/* draw border around the meter */
	gdk_draw_rectangle (GDK_DRAWABLE (widget->window), smeter.gc,
			    FALSE, 0, 0, 160, 80);

	/* initialize offscreen buffer */
	buffer = gdk_pixmap_new (GDK_DRAWABLE (smeter.canvas->window),
				 160, 80, -1);

	/* indicate that widget is ready to 
	   be used
	*/
	smeter.exposed = TRUE;


	return TRUE;
}
