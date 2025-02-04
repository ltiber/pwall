/*videoViewerWayland.c 
It uses GST framework to play the video
specific implementation for wayland
*/

#include <string.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkwayland.h>
#include <gst/wayland/wayland.h>
#include <tsoft.h>
#include <photoDialogs.h>
#include <photoOrganizer.h>
#include <multiViewer.h>
#include <videoViewerWayland.h>


typedef struct {
  GtkWidget *widget;              /*the widget where the video should be loaded */
  GstElement *playbin;            /* the Gst pipeline */
  GstVideoOverlay *overlay;       /* to connect Gstreamer with GTK */
  GstState state;                 /* Current state of the pipeline */
  gboolean endOfVideo;
  GtkWidget *slider;              /* Slider widget to keep track of current position */
  gulong slider_update_signal_id; /* Signal ID for the slider update signal */
  gint64 duration;                /* Duration of the clip, in nanoseconds */
} Video;

static void errorCB (GstBus * bus, GstMessage * msg, gpointer user_data);
static GstBusSyncReply bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data);
static gboolean videoWidgetDrawCB (GtkWidget * widget, cairo_t * cr, gpointer user_data);

static gboolean  winViewer4VideoCloseCB (GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean winViewer4VideoKeyCB(GtkWidget *widget,  GdkEventKey *event, gpointer data);
static gboolean  winViewer4VideoClickCB (GtkWidget *widget, GdkEventButton *event, gpointer data);
static void videoWidgetStateChangedCB (GstBus *bus, GstMessage *msg, gpointer data);

int stickVideoFix(gpointer user_data);

Video *video;
static  gulong winViewer4VideoKeyHandler; // key input in the window
static  gulong winViewer4VideoClickHandler; //click the window
static  gulong winViewer4VideoCloseHandler; //close the window wth the cross on the window (just before the destroy event )

static gboolean stopRefreshUI=FALSE;
static glong refreshSliderIndex=0; //counter used to fix the closing of the refreshSlider repeating function  
static gboolean refreshSlider (LongIntObj *userdata);
static void sliderCB (GtkRange *range, gpointer data);

/*
This creates all the GTK+ widgets that build the video widget 
*/
void videoWidgetInitW () {
    if (!gst_is_initialized())  gst_init (NULL, NULL);
    video = g_slice_new0 (Video);

    
    //start CSS decoration
    gchar *CSS="\
    #mainBox{\
        background-color:rgba(0,0,0,1);\
    }\
    ";
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);    
    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); 
    gtk_css_provider_load_from_data(provider, CSS, -1, NULL);
    g_object_unref (provider);
    //end CSS decoration
    
    
    
    GtkWidget *mainBox=gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(mainBox,"mainBox");
    
    GtkWidget *videoWidget = gtk_event_box_new ();
    //gtk_widget_set_can_focus (pEventBox, TRUE);
    gtk_box_pack_start (GTK_BOX (mainBox), videoWidget, TRUE, TRUE, 0);
    
    //add slider
    video->slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_widget_set_valign(video->slider,GTK_ALIGN_END);
    gtk_box_pack_start (GTK_BOX (mainBox), video->slider, FALSE, FALSE, 0);
    gtk_scale_set_draw_value (GTK_SCALE (video->slider), 0);
    video->slider_update_signal_id = g_signal_connect (G_OBJECT (video->slider), "value-changed", G_CALLBACK (sliderCB), NULL);
    gtk_widget_set_no_show_all (video->slider, TRUE); //hidden
    
    //add messageViewer   
    statusMessageViewer=gtk_label_new("                   ");
    gtk_widget_set_halign(statusMessageViewer,GTK_ALIGN_END);
    gtk_widget_set_valign(statusMessageViewer,GTK_ALIGN_END);
    gtk_widget_set_name(statusMessageViewer,"statusMessage");
    gtk_box_pack_start (GTK_BOX (mainBox), statusMessageViewer, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all (statusMessageViewer, TRUE); //hidden
    
    gtk_container_add(GTK_CONTAINER(winViewer), mainBox);

/*
    //with overlay not working in wayland
    GtkWidget *overlay=gtk_overlay_new ();
    //add video
    gtk_container_add(GTK_CONTAINER(overlay), mainBox);
    //add the status message
    statusMessageViewer=gtk_label_new("                   ");
    gtk_widget_set_halign(statusMessageViewer,GTK_ALIGN_END);
    gtk_widget_set_valign(statusMessageViewer,GTK_ALIGN_END);
    gtk_widget_set_name(statusMessageViewer,"statusMessage");
    gtk_overlay_add_overlay (GTK_OVERLAY (overlay), statusMessageViewer); 
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(overlay),statusMessageViewer,TRUE);
    gtk_widget_set_no_show_all (statusMessageViewer, TRUE); //hidden
    */
    


    video->widget = GTK_WIDGET (videoWidget);
    
    g_signal_connect (video->widget, "draw",G_CALLBACK (videoWidgetDrawCB), video);

    gtk_widget_show_all (winViewer);
    
}


/*
Load the URI and play
*/  
int videoWidgetLoadW(int index){
    g_print("\nvideoWidgetLoadW\n");
    
    GstBus *bus;
    GError *error = NULL;

    // Set the URI to play // 
    FileObj *fileObj=g_ptr_array_index(viewedFileSet,index); //get the file
    char *currentDir=getFullPathFromidNode(viewedDirNode,NULL); //get the dir  
    
    // set the global viewedfullpath used in popupmenu
    if (viewedFullPath!=NULL) g_free(viewedFullPath);
    viewedFullPath=g_strdup_printf("%s/%s", currentDir, fileObj->name);
    char *uri=g_filename_to_uri (viewedFullPath, NULL,NULL);
    g_print("uri :%s\n",uri);
    
    // fill the Video Object //
    video->duration = GST_CLOCK_TIME_NONE;
    
    //init the Gstreamer pipeline
    video->playbin = gst_parse_launch ("playbin video-sink=waylandsink", NULL);
    g_object_set (video->playbin, "uri", uri, NULL);
    
    //accroche le pipeline au bus
    bus = gst_pipeline_get_bus (GST_PIPELINE (video->playbin));
    gst_bus_add_signal_watch (bus);
    g_signal_connect (bus, "message::error", G_CALLBACK (errorCB), NULL);
    g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)videoWidgetStateChangedCB, NULL);
    gst_bus_set_sync_handler (bus, bus_sync_handler, NULL, NULL);
    gst_object_unref (bus);
    
    //start playing
    gst_element_set_state (video->playbin, GST_STATE_PLAYING);

    //workaround pour que le stream video se mette bien dans la fenêtre parent
    //c'est comme quand on bouge la souris sur la fenêtre. Ca la rafraichit et elle se colle à la fenêtre parent
    gdk_threads_add_timeout(500,stickVideoFix,NULL);  

    //start slider refresh
    stopRefreshUI=FALSE; //reset the flag for new function
    ++refreshSliderIndex; //pass the refreshSliderIndex to the function
    // Register a function that GLib will call every second //
    LongIntObj *userdata=malloc(sizeof(LongIntObj)); userdata->value=refreshSliderIndex; 
    
    g_timeout_add_seconds (1, (GSourceFunc)refreshSlider, userdata); 

    return TRUE;
}

static void errorCB (GstBus * bus, GstMessage * msg, gpointer user_data){
  gchar *debug = NULL;
  GError *err = NULL;

  gst_message_parse_error (msg, &err, &debug);

  g_print ("Error: %s\n", err->message);
  g_error_free (err);

  if (debug) {
    g_print ("Debug details: %s\n", debug);
    g_free (debug);
  }

  gst_element_set_state (video->playbin, GST_STATE_NULL);
}


static GstBusSyncReply bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data){

  if (gst_is_wl_display_handle_need_context_message (message)) {
    GstContext *context;
    GdkDisplay *display;
    struct wl_display *display_handle;

    display = gtk_widget_get_display (video->widget);
    display_handle = gdk_wayland_display_get_wl_display (display);
    context = gst_wl_display_handle_context_new (display_handle);
    gst_element_set_context (GST_ELEMENT (GST_MESSAGE_SRC (message)), context);
    gst_context_unref (context);
    g_print("we receive gst_is_wl_display_handle_need_context_message");
    goto drop;
  } else if (gst_is_video_overlay_prepare_window_handle_message (message)) {
    GtkAllocation allocation;
    GdkWindow *window;
    struct wl_surface *window_handle;

    /* GST_MESSAGE_SRC (message) will be the overlay object that we have to
     * use. This may be waylandsink, but it may also be playbin. In the latter
     * case, we must make sure to use playbin instead of waylandsink, because
     * playbin resets the window handle and render_rectangle after restarting
     * playback and the actual window size is lost */
    video->overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));

    gtk_widget_get_allocation (video->widget, &allocation);
    window = gtk_widget_get_window (video->widget);
    window_handle = gdk_wayland_window_get_wl_surface (window);
    g_print("we receive gst_is_video_overlay_prepare_window_handle_message."); 
    g_print ("So we set window handle and size (%d x %d)\n",
        allocation.width, allocation.height);

    gst_video_overlay_set_window_handle (video->overlay, (guintptr) window_handle);
    gst_video_overlay_set_render_rectangle (video->overlay, allocation.x,
        allocation.y, allocation.width, allocation.height);

    goto drop;
  }

  return GST_BUS_PASS;

drop:
  gst_message_unref (message);
  return GST_BUS_DROP;
}

/* This function is called when an End-Of-Stream message is posted on the bus.
 * We just set the pipeline to READY (which stops playback) */
static void videoWidgetEOSCB (GstBus *bus, GstMessage *msg, gpointer data) {
  g_print ("End-Of-Stream reached.\n");
  video->endOfVideo=TRUE;
  //gst_element_set_state (video->playbin, GST_STATE_READY); 
  gst_element_set_state (video->playbin, GST_STATE_PAUSED); //to fix dark screen bug
}

/* This function is called when the pipeline changes states. We use it to
 * keep track of the current state. */
static void videoWidgetStateChangedCB (GstBus *bus, GstMessage *msg, gpointer data) {
  GstState old_state, new_state, pending_state;

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (video->playbin)) {
    video->state = new_state;
    g_print ("State set to %s\n", gst_element_state_get_name (new_state));
  }
}


/* We use the "draw" callback to change the size of the sink
 * because the "configure-event" is only sent to top-level widgets. */
static gboolean videoWidgetDrawCB (GtkWidget * widget, cairo_t * cr, gpointer user_data){
  Video *v = user_data;
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  g_print ("draw_cb x %d, y %d, w %d, h %d\n",
      allocation.x, allocation.y, allocation.width, allocation.height);
  g_print("new draw \n");
  if (v->overlay) {
    gst_video_overlay_set_render_rectangle (v->overlay, allocation.x,
        allocation.y, allocation.width, allocation.height);
  }

  /* There is no need to call gst_video_overlay_expose().
   * The wayland compositor can always re-draw the window
   * based on its last contents if necessary */

  return FALSE;
}

int stickVideoFix(gpointer user_data){
  g_print("We force a new window draw to stick the video panel to the window\n");
  gtk_widget_queue_draw(GTK_WIDGET(winViewer));
  return FALSE; 
}



//connect the handler to the multiViewer Window for photo display features
void connectWinVideoHandlerW(){
  g_print("connectWinVideoHandlerW\n");
  winViewer4VideoKeyHandler=g_signal_connect(G_OBJECT(winViewer), "key-press-event", G_CALLBACK(winViewer4VideoKeyCB), NULL);
  winViewer4VideoClickHandler=g_signal_connect (G_OBJECT(winViewer),"button-press-event",G_CALLBACK (winViewer4VideoClickCB), NULL); 
  winViewer4VideoCloseHandler=g_signal_connect (G_OBJECT(winViewer),"delete-event",G_CALLBACK (winViewer4VideoCloseCB), NULL);   
}

//remove the connection the handler to the multiViewer Window for further connections
void disconnectWinVideoHandlerW(){
  g_print("disconnectWinVideoHandlerW\n");
  g_signal_handler_disconnect(G_OBJECT(winViewer), winViewer4VideoKeyHandler);
  g_signal_handler_disconnect(G_OBJECT(winViewer), winViewer4VideoClickHandler);
  g_signal_handler_disconnect(G_OBJECT(winViewer), winViewer4VideoCloseHandler);
}

static gboolean  winViewer4VideoCloseCB (GtkWidget *widget, GdkEventButton *event, gpointer data)  {
  g_print("\nwinViewer4VideoCloseCB\n");
  videoWidgetCloseW();
  return FALSE; 
  //TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
}

/*
used to pause, play
*/
static gboolean winViewer4VideoKeyCB(GtkWidget *widget,  GdkEventKey *event, gpointer data) {
    //common callback to video and photo
   if (multiViewerKeyCB(widget, event)) return FALSE; //no more processing in this function

    switch (event->keyval) {
    //use the space bar to play and pause
    case GDK_KEY_space:
        g_print("-space");
        if (video->state==GST_STATE_PAUSED) {
          g_print("paused");
          if (video->endOfVideo) { 
            gst_element_seek_simple (video->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,(gint64)(0));
            video->endOfVideo=FALSE;
          }
          gst_element_set_state (video->playbin, GST_STATE_PLAYING);
        }
        else if (video->state==GST_STATE_PLAYING) {g_print("playing"); gst_element_set_state (video->playbin, GST_STATE_PAUSED);}
        break;
    case GDK_KEY_Return:
        if (event->state & GDK_MOD1_MASK) {
            g_print("-alt return");
            showPropertiesDialog();
        }    
        break;
    case GDK_KEY_i:
    case GDK_KEY_I: //OSX shortcut
        if (event->state & GDK_META_MASK) {
            g_print("-cmd i");
            showPropertiesDialog();
        }
        break;
    
    }
    return FALSE;
}

/*
used to show or hide the slider
*/
static gboolean  winViewer4VideoClickCB (GtkWidget *widget, GdkEventButton *event, gpointer data)  {
    //common callback to video and photo
    if (multiViewerClickCB(widget,event,data)) return FALSE; //no more processing in this function
    g_print("\nclick\n");

    //rightclick
    if (event->type == GDK_BUTTON_PRESS){        
        if (event->button == GDK_BUTTON_SECONDARY || event->state & GDK_CONTROL_MASK)  {
            g_print("clickright"); //ctrl click for mac support
            if (video->state==GST_STATE_PLAYING) {g_print("playing"); gst_element_set_state (video->playbin, GST_STATE_PAUSED);} //pause the video
            gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, NULL, NULL, event->button, event->time); //show the popupmenu in gtk-3.18
            //gtk_menu_popup_at_pointer(GTK_MENU(pMenuPopup), NULL); //gtk-3.22
            return TRUE;
        }
    }

    /*show, hide
    if (!gtk_widget_is_visible(video->slider)) gtk_widget_show(video->slider);
    else gtk_widget_hide(video->slider);*/
    if (video->state==GST_STATE_PAUSED) {
      g_print("paused");
      if (video->endOfVideo) { 
        gst_element_seek_simple (video->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,(gint64)(0));
        video->endOfVideo=FALSE;
      }
      gst_element_set_state (video->playbin, GST_STATE_PLAYING);
    }   else if (video->state==GST_STATE_PLAYING) {g_print("playing"); gst_element_set_state (video->playbin, GST_STATE_PAUSED);}
    
    return TRUE; //To stop propagation
    //TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
}


/* 
It ends properly the gst components
*/
void videoWidgetCloseW(){
  g_print("videoWidgetCloseW\n");
  stopRefreshUI=TRUE;
  gst_element_set_state (video->playbin, GST_STATE_NULL);
  gst_object_unref (video->playbin);
  g_slice_free (Video, video);
  g_print("end videoWidgetCloseW\n");
}

/* This function is called when the slider changes its position. We perform a seek to the
 * new position here. */
static void sliderCB (GtkRange *range, gpointer data) {
    gdouble value = gtk_range_get_value (GTK_RANGE (video->slider));
    //we've reached the end of the video ->restart
    if (video->state==GST_STATE_READY){
        GstStateChangeReturn ret = gst_element_set_state (video->playbin, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr ("Unable to set the pipeline to the playing state.\n");
        }
        g_usleep(500000); //to get the pipe ready for new seek
    }
    gst_element_seek_simple (video->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
          (gint64)(value * GST_SECOND));
}



/* This function is called periodically to refresh the GUI */
static gboolean refreshSlider (LongIntObj *userdata) {	
    static int lastPointerX=-1, lastPointerY=-1;
    g_print("refreshSlider stopRefreshUI=%i,userdata=%li\n",stopRefreshUI,userdata->value);
    gint64 current = -1;  

    if (userdata->value!=refreshSliderIndex) {g_print("Not the current refreshSliderIndex %li\n",userdata->value);return FALSE;} //stop the periodical calling because this function is for an old video
    
    if (stopRefreshUI) {return FALSE;} //stop the periodical calling because we receive the order to stop this video
    
    if (!GST_IS_ELEMENT(video->playbin)) return FALSE; //if the videowidget is removed before the refreshlider 
    
    /* We do not want to update anything unless we are in the PAUSED or PLAYING states */
    if (video->state < GST_STATE_PAUSED) return TRUE;

    //get the position of the pointer 
    int x,y;
    GdkDisplay *display = gdk_display_get_default ();
    GdkDeviceManager *device_manager = gdk_display_get_device_manager (display);
    GdkDevice *device = gdk_device_manager_get_client_pointer (device_manager);
    gdk_window_get_device_position (gtk_widget_get_window(winViewer), device, &x, &y, NULL);    
    //g_print("\nposition x%i,y%i\n",x,y);
    
    //get the height of the slider add margin=10px
    int pointerInSlider=FALSE;
    GtkAllocation *allocation=malloc(sizeof(GtkAllocation));
    gtk_widget_get_allocation (video->slider,allocation);    
    int heightWin=gdk_window_get_height (gtk_widget_get_window(winViewer));
    //g_print("y:%i,allocation height:%i,winheight:%i\n",y, allocation->height,heightWin);
    if (y>=(heightWin - allocation->height - 10 )) pointerInSlider=TRUE;
    free(allocation);
    
    //show or hide the slider
    if (pointerInSlider){ //pointer in the slider container
        if (!gtk_widget_is_visible(video->slider)) gtk_widget_show(video->slider);    
    } else  if ((lastPointerX!=-1) && (lastPointerX!=x || lastPointerY !=y)){ //pointer in movement
        if (!gtk_widget_is_visible(video->slider)) gtk_widget_show(video->slider);    
    } else { // pointer static
        if (gtk_widget_is_visible(video->slider)) gtk_widget_hide(video->slider);    
    }    
    lastPointerX=x; lastPointerY=y;
    
  /* If we didn't know it yet, query the stream duration */
    if (!GST_CLOCK_TIME_IS_VALID (video->duration)) {
        if (!gst_element_query_duration (video->playbin, GST_FORMAT_TIME, &video->duration)) {
            g_printerr ("Could not query current duration.\n");
        } else {
            /* Set the range of the slider to the clip duration, in SECONDS */
            gtk_range_set_range (GTK_RANGE (video->slider), 0, (gdouble)video->duration / GST_SECOND);

        }
    }

    if (gst_element_query_position (video->playbin, GST_FORMAT_TIME, &current)) {
        /* Block the "value-changed" signal, so the sliderCB function is not called
         * (which would trigger a seek the user has not requested) */
        g_signal_handler_block (video->slider, video->slider_update_signal_id);
        /* Set the position of the slider to the current pipeline positoin, in SECONDS */
        gtk_range_set_value (GTK_RANGE (video->slider), (gdouble)current / GST_SECOND);
        /* Re-enable the signal */
        g_signal_handler_unblock (video->slider, video->slider_update_signal_id);
    }
    return TRUE;
}

