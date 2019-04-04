#include <string.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined(GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined(GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData{
    GstElement *playbin;        /* Our one and only pipeline */

    GtkWidget *slider;          /* Slider widget to keep track of current position */
    GtkWidget *streams_list;    /* Text widget to display info about the streams */
    gulong slider_update_signal_id; /* Signal ID for the slider update signal */

    GstState state;             /*Current state of the pipeline*/
    gint64 duration;            /* Duration of the clip, in nanoseconds */

} CustomData;

/* This function is called when the GUI toolkit creates the physical window that will hold the video.
 * At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
 * and pass it to Gstreamer through the VideoOverlay interface */
static void realize_cb(GtkWidget *widget, CustomData *data){
    guintptr window_handle;

    if(!gdk_window_ensure_native(window))
        g_error("Couldn't create native window needed for GstVideoOverlay!");

    /* Retreive window handler from GDK */

    #if defined(GDK_WINDOWING_WIN32)
        window_handle = (guinptr)GDK_WINDOW_HWND(window);
    #elif defined(GDK_WINDOWING_QUARTZ)
        window_handle = gdk_quartz_window_get_nsview(window);
    #elif defined(GDK_WINDOWING_X11)
        window_handle = GDK_WINDOW_XID(window);
    #endif
    
    /* Pass it to playbin, which implements VideoOverlay and will forward it to the video sink */
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY (data->playbin), window_handle);
}

/* This function is called when the PLAY button is clicked*/
static void play_cb(GtkButton *button, CustomData *data){
    gst_element_set_state(data->playbin, GST_STATE_PLAYING);
}

/* This function is called when the PAUSE button is clicked */
static void pause_cb(GtkButton *button, CustomData *data){
    gst_element_set_state(data->playbin, GST_STATE_PAUSED);
}

/* This function is called when the STOP button is clicked */
static void stop_cb(GtkButton *button, CustomData *data){
    gst_element_set_state(data->playbin, GST_STATE_READY);
}

/* This function is called when the main window is closed */
static void delete_event_cb(GtkWidget *widget, GdkEvent *event, CustomData *data){
    stop_cb(NULL, data);
    gtk_main_quit();    
}

/* This function is called everytime the video window needs to be redrawn (due to damage/exposure,
 * rescaling, etc). Gstreamer takes care of this in the PAUSED and PLAYING states, otherwise,
 * we simply draw a black rectangle to avoid garbage showing up. */
static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, CustomData *data){
    if(data->state < GST_STATE_PAUSED){
        GtkAllocation allocation;

        /* Cairo is a 2D graphics library which we use here to clean the video window.
         * It is used by Gstreamer for other reasons, so it will always be available to us */
        gtk_widget_get_allocation(widget, &allocation);
        cairo_set_source_rgb(cr, 0, 0 ,0);
        cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
        cairo_fill(cr);
    }
    return FALSE;
}

/* This function is called when the slider changes its position. We perform a seek to the
 * new position here. */
static void slider_cb(GtkRange *range, CustomData *data){
    gdouble value = gtk_range_get_value(GTK_RANGE (data->slider));
    gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
    (gint64)(value * GST_SECOND));
}

/* This creates all the GTK+ widgets that compose our application, and registers the callbacsk */
static void create_ui(CustomData *data){
    GtkWidget *main_window;         /*The uppermost window, containing all other windows */
    GtkWidget *video_window;        /* The drawing area where the video will be shown */
    GtkWidget *main_box;            /* VBox to hold main_hbox and the controls */
    GtkWidget *main_hbox;           /* HBox to hold the video_window and the stream info text widget */
    GtkWidget *controls;            /* HBox to hold the buttons and the slider */
    GtkWidget *play_button, *pause_button, *stop_button; /*Buttons */

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT (main_window), "delete-event", G_CALLBACK(delete_event_cb), data);

    video_window = gtk_drawing_area_new();
    gtk_widget_set_double_buffered(video_window, FALSE);
    g_signal_connect(video_vindow, "realize", G_CALLBACK(realize_cb), data);
    g_signal_connect(video_window, "draw", G_CALLBACK(draw_cb), data);

    play_button = gtk_button_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(G_OBJECT(play_button), "clicked", G_CALLBACK(play_cb), data);

    pause_button = gtk_button_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect (G_OBJECT(pause_button), "clicked", G_CALLBACK(pause_cb), data);

    stop_button = gtk_button_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(G_OBJECT (stop_button), "clicked", G_CALLBACK(stop_cb), data);

    data->slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(data->slider), 0);
    data->slider_update_signal_id = g_signal_connect(G_OBJECT (data->slider), "value-changed", G_CALLBACK(slider_cb), data);

    data->streams_list = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->streams_list), FALSE);

    controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (controls), play_button, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (controls), pause_button, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (controls), stop_button, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (controls), data->slider, TRUE, TRUE, 2);

    main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (main_hbox), video_window, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (main_hbox), data->streams_list, FALSE, FALSE, 2);

    main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (main_box), main_hbox, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (main_box), controls, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (main_window), main_box);
    gtk_window_set_default_size (GTK_WINDOW (main_window), 640, 480);

    gtk_widget_show_all (main_window);
}

/*This function is called periodically to refresh the GUI */
static gboolean refresh_ui(CustomData *data){
    gint64 current = -1;

    /*We do not want to update anything unless we are in the PAUSED or PLYING states */
    if(data->state < GST_STATE_PAUSED)
        return TRUE;

    /* If we didn't know it yet, query the stream duration */
    if(!GST_CLOCK_TIME_IS_VALID(data->duration)){
        if(!gst_element_query_duration (data->playbin, GST_FORMAT_TIME, &data->duration)){
            g_printerr("Could not query current duration.\n");
        }else {
            /* Set the range of the slider to the clip duration, in SECONDS */
            gtk_range_set_range(GTK_RANGE (data->slider), 0, (gdouble)data->duration / GST_SECOND);
        
        }
    }

    
}