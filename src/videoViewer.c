/*videoViewer.c 
It uses GST framework to play the video
*/

#include <string.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <tsoft.h>
#include <photoDialogs.h>
#include <photoOrganizer.h>
#include <multiViewer.h>
#include <videoViewer.h>


/* Structure to contain all our information, so we can pass it around */
typedef struct _VideoObj {
  GstElement *playbin;           /* Our one and only pipeline */
  GtkWidget *slider;              /* Slider widget to keep track of current position */
  GtkWidget *container;        /* video widget container */
  gulong slider_update_signal_id; /* Signal ID for the slider update signal */
  GstState state;                 /* Current state of the pipeline */
  gint64 duration;                /* Duration of the clip, in nanoseconds */
} VideoObj;


static gboolean stopRefreshUI=FALSE;
static gboolean endOfVideo=FALSE;
static gboolean videoErrorLoad=FALSE;
static VideoObj *video=NULL;
static  gulong winViewer4VideoKeyHandler; // key input in the window
static  gulong winViewer4VideoClickHandler; //click the window
static  gulong winViewer4VideoCloseHandler; //close the window wth the cross on the window (just before the destroy event )
static guintptr video_window_handle = 0;


static glong refreshSliderIndex=0; //counter used to fix the closing of the refreshSlider repeating functio  


static gboolean winViewer4VideoKeyCB(GtkWidget *widget,  GdkEventKey *event, gpointer data);
static gboolean  winViewer4VideoClickCB (GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean  winViewer4VideoCloseCB (GtkWidget *widget, GdkEventButton *event, gpointer data);

static void sliderCB (GtkRange *range, gpointer data);
static void videoWidgetRealizeCB (GtkWidget *widget, gpointer data);
static gboolean videoWidgetDrawCB (GtkWidget *widget, cairo_t *cr, gpointer data);
static void videoWidgetTagsCB (GstElement *playbin, gint stream, gpointer data);
static void videoWidgetStateChangedCB (GstBus *bus, GstMessage *msg, gpointer data);
static void videoWidgetErrorCB (GstBus *bus, GstMessage *msg, gpointer data);
static void videoWidgetEOSCB (GstBus *bus, GstMessage *msg, gpointer data);
static gboolean refreshSlider(LongIntObj *userdata);
static GstBusSyncReply bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data);




//connect the handler to the multiViewer Window for photo display features
void connectWinVideoHandler(){
    winViewer4VideoKeyHandler=g_signal_connect(G_OBJECT(winViewer), "key-press-event", G_CALLBACK(winViewer4VideoKeyCB), NULL);
    winViewer4VideoClickHandler=g_signal_connect (G_OBJECT(winViewer),"button-press-event",G_CALLBACK (winViewer4VideoClickCB), NULL); 
    winViewer4VideoCloseHandler=g_signal_connect (G_OBJECT(winViewer),"delete-event",G_CALLBACK (winViewer4VideoCloseCB), NULL);   
}

//remove the connection the handler to the multiViewer Window for further connections
void disconnectWinVideoHandler(){
    g_signal_handler_disconnect(G_OBJECT(winViewer), winViewer4VideoKeyHandler);
    g_signal_handler_disconnect(G_OBJECT(winViewer), winViewer4VideoClickHandler);
    g_signal_handler_disconnect(G_OBJECT(winViewer), winViewer4VideoCloseHandler);

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
      if (endOfVideo) { 
        gst_element_seek_simple (video->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,(gint64)(0));
        endOfVideo=FALSE;
      }
      gst_element_set_state (video->playbin, GST_STATE_PLAYING);
    }   else if (video->state==GST_STATE_PLAYING) {g_print("playing"); gst_element_set_state (video->playbin, GST_STATE_PAUSED);}
    
    return TRUE; //To stop propagation
    //TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
}

/*
check movement of the pointer
*/
static gboolean  winViewer4VideoMoveCB (GtkWidget *widget, GdkEventButton *event, gpointer data)  {
    g_print("\nwinViewer4VideoMoveCB\n");
    if (event->type==GDK_MOTION_NOTIFY) {
        GdkEventMotion* e=(GdkEventMotion*)event;
        g_print("Coordinates: (%u,%u)\n", (guint)e->x,(guint)e->y);
    }
    return FALSE; 
    //TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
}

static gboolean  winViewer4VideoCloseCB (GtkWidget *widget, GdkEventButton *event, gpointer data)  {
    g_print("\nwinViewer4VideoCloseCB\n");
    videoWidgetClose();
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
          if (endOfVideo) { 
            gst_element_seek_simple (video->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,(gint64)(0));
            endOfVideo=FALSE;
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
This creates all the GTK+ widgets that build the video widget 
*/
void videoWidgetInit () {
    video_window_handle=0;
    endOfVideo=FALSE;
    videoErrorLoad=FALSE;

    if (!gst_is_initialized())  gst_init (NULL, NULL);
    
    if (!video) video =malloc(sizeof(VideoObj)); //if video is NULL

    //start CSS decoration
    gchar *CSS="\
    #controls{\
        background-color:rgba(0,0,0,1);\
        padding-left:20px;\
        padding-right:20px;\
    }\
    ";
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);    
    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); 
    gtk_css_provider_load_from_data(provider, CSS, -1, NULL);
    g_object_unref (provider);
  //end CSS decoration
    
    GtkWidget *videoWidget = gtk_drawing_area_new ();
    gtk_widget_set_double_buffered (videoWidget, FALSE);
    g_signal_connect (videoWidget, "realize", G_CALLBACK (videoWidgetRealizeCB), NULL);
    g_signal_connect (videoWidget, "draw", G_CALLBACK (videoWidgetDrawCB), NULL);

    video->slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value (GTK_SCALE (video->slider), 0);
    video->slider_update_signal_id = g_signal_connect (G_OBJECT (video->slider), "value-changed", G_CALLBACK (sliderCB), NULL);
    
    GtkWidget *videoLayer=gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (videoLayer), videoWidget, TRUE, TRUE, 0);
    
    GtkWidget *controlLayer=gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (controlLayer), video->slider, TRUE, TRUE, 2);
    gtk_widget_set_valign(controlLayer,GTK_ALIGN_END);
    gtk_widget_set_name(controlLayer,"controls"); 
    //gtk_widget_set_no_show_all (controlLayer, TRUE); //workaround a suspend, wake up bug

    GtkWidget *overlay=gtk_overlay_new ();
    video->container=overlay;
    //add video
    gtk_container_add(GTK_CONTAINER(overlay), videoLayer);
    //add controls
    gtk_overlay_add_overlay (GTK_OVERLAY (overlay), controlLayer); 
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(overlay),controlLayer,TRUE);
    //add the status message
    statusMessageViewer=gtk_label_new("                   ");
    gtk_widget_set_halign(statusMessageViewer,GTK_ALIGN_END);
    gtk_widget_set_valign(statusMessageViewer,GTK_ALIGN_END);
    gtk_widget_set_name(statusMessageViewer,"statusMessage");
    gtk_overlay_add_overlay (GTK_OVERLAY (overlay), statusMessageViewer); 
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(overlay),statusMessageViewer,TRUE);
    gtk_widget_set_no_show_all (statusMessageViewer, TRUE); //hidden
    //add overlay to the window
    gtk_container_add (GTK_CONTAINER (winViewer), overlay);
    gtk_widget_realize(videoWidget); //for syncro avec bus_sync_handler

}


/*
Load the URI and play
*/  
int videoWidgetLoad(int index){
    g_print("\nvideoWidgetLoad\n");
    GstStateChangeReturn ret;
    // fill the VideoObj //
    video->duration = GST_CLOCK_TIME_NONE;

    // Create the elements //
    //video->playbin = gst_element_factory_make ("playbin", "playbin");
    //pipeline = gst_parse_launch ("playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);
    //gchar *cmd="filesrc location=/media/leon/HDD/Pictures/GoPro/720p/test_960.mp4 ! qtdemux name=demux  demux.audio_0 ! queue ! decodebin ! audioconvert ! audioresample ! autoaudiosink    demux.video_0 ! queue ! decodebin ! videoflip method=clockwise ! videoconvert ! autovideosink";
    //OK gchar *cmd="videotestsrc ! videoconvert ! autovideosink";
    //OK gchar *cmd="playbin uri=\"file:///media/leon/HDD/Pictures/2012-10 Violaine/20121025_114520.mp4\"";
    //OK gchar *cmd="filesrc location=\"/media/leon/HDD/Pictures/2012-10 Violaine/20121025_114520.mp4\" ! decodebin ! clockoverlay font-desc=\"Sans, 48\" ! videoconvert ! autovideosink";
  
    // Set the URI to play // 
    FileObj *fileObj=g_ptr_array_index(viewedFileSet,index); //get the file
    char *currentDir=getFullPathFromidNode(viewedDirNode,NULL); //get the dir
  
// set the global viewedfullpath used in popupmenu
    if (viewedFullPath!=NULL) g_free(viewedFullPath);
    viewedFullPath=g_strdup_printf("%s/%s", currentDir, fileObj->name);
    
    /* support of image-orientation  but not stable
    if (!g_str_has_suffix(fileObj->name,".avi") && !g_str_has_suffix(fileObj->name,".AVI") ) {

      //for mp4 files we support rotation, so we use decodebin
      gboolean flip=FALSE;
      gchar *flipCmd="", *audioCmd="";
      if (needVideoFlip(viewedFullPath)) flipCmd="! videoflip method=clockwise";
      if (hasAudioCodec(viewedFullPath)) audioCmd= "demux.audio_0 ! queue ! decodebin ! audioconvert ! audioresample ! autoaudiosink";
      gchar *cmd=g_strdup_printf("filesrc location=\"%s\"\
      ! qtdemux name=demux  %s \
      demux.video_0 ! queue ! decodebin %s ! videoconvert ! autovideosink",viewedFullPath, audioCmd, flipCmd);
      g_print("\n%s\n",cmd);  
      video->playbin=gst_parse_launch (cmd,NULL);
      g_free(cmd);
    } else {*/

      //standart playbin more stable//
      video->playbin = gst_element_factory_make ("playbin", "playbin");
      //char *uri=g_strdup_printf("file://%s", viewedFullPath); //bug with special chars like # in the name of the video
      char *uri=g_filename_to_uri (viewedFullPath, NULL,NULL);
      //g_print("uri:  %s\n",uri);
      g_object_set (video->playbin, "uri", uri, NULL);
    //}

    if (!video->playbin) {
        g_printerr ("Not all elements could be created.\n");
        return FALSE;
    }

    // Connect to interesting signals in playbin //
    //g_signal_connect (G_OBJECT (video->playbin), "video-tags-changed", (GCallback) videoWidgetTagsCB, NULL);
    //g_signal_connect (G_OBJECT (video->playbin), "audio-tags-changed", (GCallback) videoWidgetTagsCB, NULL);
    //g_signal_connect (G_OBJECT (video->playbin), "text-tags-changed", (GCallback) videoWidgetTagsCB, NULL);

    // Instruct the bus to emit signals for each received message, and connect to the interesting signals //
    GstBus *bus = gst_element_get_bus (video->playbin);
    
    //will do gst_video_overlay_set_window_handle to put the video in the correct window
    gst_bus_set_sync_handler (bus, (GstBusSyncHandler) bus_sync_handler, NULL, NULL);  
    
    gst_bus_add_signal_watch (bus);
    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)videoWidgetErrorCB, NULL);
    g_signal_connect (G_OBJECT (bus), "message::eos", (GCallback)videoWidgetEOSCB, NULL);
    g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)videoWidgetStateChangedCB, NULL);
    gst_object_unref (bus);

    // Start playing //
    ret = gst_element_set_state (video->playbin, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_print ("Error : Unable to set the pipeline to the playing state.\n");
        gst_object_unref (video->playbin);
        videoErrorLoad=TRUE;
        return FALSE ;
    }

    stopRefreshUI=FALSE; //reset the flag for new function
    ++refreshSliderIndex; //pass the refreshSliderIndex to the function
    // Register a function that GLib will call every second //
    LongIntObj *userdata=malloc(sizeof(LongIntObj)); userdata->value=refreshSliderIndex; 
    
    g_timeout_add_seconds (1, (GSourceFunc)refreshSlider, userdata); 

    return TRUE;
}


/* 
It ends properly the gst components
*/
void videoWidgetClose(){
    g_print("videoWidgetClose\n");
    stopRefreshUI=TRUE;
    if (!videoErrorLoad){ //to avoid a app lock in case of some errors
      if (GST_IS_ELEMENT(video->playbin)) gst_element_set_state (video->playbin, GST_STATE_NULL);
      if (GST_IS_ELEMENT(video->playbin)) gst_object_unref (video->playbin); else g_print("video->playbin videoWidgetClose is not a GST_ELEMENT\n");
    }
    g_free(video); video=NULL;   

    //g_usleep(1000000); //no effect on suspend/restore bug
}


/* This function is called when the GUI toolkit creates the physical window that will hold the video.
 * At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
 * and pass it to GStreamer through the VideoOverlay interface. */
static void videoWidgetRealizeCB (GtkWidget *widget, gpointer data) {
    g_print("\nrealized\n");
    GdkWindow *window = gtk_widget_get_window (widget);
    if (!gdk_window_ensure_native (window))
    g_error ("Couldn't create native window needed for GstVideoOverlay!");

  /* Retrieve window handler from GDK */
  video_window_handle = GDK_WINDOW_XID (window);
  
  /*will do gst_video_overlay_set_window_handle to put the video in the correct window*/
  /*GstBus *bus = gst_element_get_bus (video->playbin);    
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) bus_sync_handler, NULL, NULL);  
  gst_object_unref (bus);*/

  /* Pass it to playbin, which implements VideoOverlay and will forward it to the video sink */
  // we do it via bus_sync_handler to troubleshhot the sleep and wakeup
  //it can work with the playbin but not the parse launcher and it's better to run the bus_sync_handler
  //gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (video->playbin), video_window_handle);
}

static GstBusSyncReply bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data){
  // ignore anything but 'prepare-window-handle' element messages
  if (!gst_is_video_overlay_prepare_window_handle_message (message))
    return GST_BUS_PASS;

  if (video_window_handle != 0) {
    GstVideoOverlay *overlay;

    // GST_MESSAGE_SRC (message) will be the video sink element
    g_print("bus_sync_handler - set overlay\n");
    overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));
    gst_video_overlay_set_window_handle (overlay, video_window_handle);
  } else {
    g_warning ("Should have obtained video_window_handle by now!");
  }

  gst_message_unref (message);
  return GST_BUS_DROP;
 }

/* This function is called everytime the video window needs to be redrawn (due to damage/exposure,
 * rescaling, etc). GStreamer takes care of this in the PAUSED and PLAYING states, otherwise,
 * we simply draw a black rectangle to avoid garbage showing up. */
static gboolean videoWidgetDrawCB (GtkWidget *widget, cairo_t *cr, gpointer data) {
  if (video->state < GST_STATE_PAUSED) {
    GtkAllocation allocation;
    /* Cairo is a 2D graphics library which we use here to clean the video window.
     * It is used by GStreamer for other reasons, so it will always be available to us. */
    gtk_widget_get_allocation (widget, &allocation);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
    cairo_fill (cr);
  }
  return FALSE;
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
    } else  if (lastPointerX!=x || lastPointerY !=y){ //pointer in movement
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

/* This function is called when new metadata is discovered in the stream */
static void videoWidgetTagsCB (GstElement *playbin, gint stream, gpointer data) {
  /* We are possibly in a GStreamer working thread, so we notify the main
   * thread of this event through a message in the bus */
  gst_element_post_message (playbin, gst_message_new_application (GST_OBJECT (playbin), gst_structure_new_empty ("tags-changed")));
}

/* This function is called when an error message is posted on the bus */
static void videoWidgetErrorCB (GstBus *bus, GstMessage *msg, gpointer data) {
  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  gst_message_parse_error (msg, &err, &debug_info);
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error (&err);
  g_free (debug_info);
  /* Set the pipeline to READY (which stops playback)  */
  if (!videoErrorLoad) gst_element_set_state (video->playbin, GST_STATE_READY); 
}

/* This function is called when an End-Of-Stream message is posted on the bus.
 * We just set the pipeline to READY (which stops playback) */
static void videoWidgetEOSCB (GstBus *bus, GstMessage *msg, gpointer data) {
  g_print ("End-Of-Stream reached.\n");
  endOfVideo=TRUE;
  //gst_element_set_state (video->playbin, GST_STATE_READY); 
    gst_element_set_state (video->playbin, GST_STATE_PAUSED); //to fix dark screen bug
}

/* This function is called when the pipeline changes states. We use it to
 * keep track of the current state. */
static void videoWidgetStateChangedCB (GstBus *bus, GstMessage *msg, gpointer data) {
  GstState old_state, new_state, pending_state;
  g_print("videoWidgetStateChangedCB\n");
  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (video->playbin)) {
    video->state = new_state;
    g_print ("State set to %s\n", gst_element_state_get_name (new_state));
    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
      /* For extra responsiveness, we refresh the GUI as soon as we reach the PAUSED state */
      //refreshSlider ();
    }
  }
}

