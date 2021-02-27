/*photoViewer.c
This source will be called by MultiViewer.c .
if the media is a photo.
photoWidgetInit will create the widget structure to display the photo
photoWidgetLoad will find, load, add the photo to the widget structure.
it handles zoom, delete, move features.
*/

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <tsoft.h>
#include <photoOrganizer.h>
#include <multiViewer.h>
#include <photoViewer.h>
#include <photoDialogs.h>

typedef enum ZoomType {
	ZOOM_IN,ZOOM_OUT
}ZoomType;


static  gulong winViewer4PhotoKeyHandler; // key input in the window
static  gulong winViewer4PhotoClickHandler; //click the window
static GtkWidget *pBoxViewWindow;
static GtkWidget *pImage;
static float zoomScale=1.0f; //used for crl+ and ctrl-


static gboolean winViewer4PhotoClickCB (GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean winViewer4PhotoKeyCB(GtkWidget *widget,  GdkEventKey *event);
static void zoomPhoto(ZoomType type);
static gboolean  scrolledPanelChangedCB (GtkAdjustment  *pAdjustment ,  gpointer data);



//connect the handler to the multiViewer Window for photo display features
void connectWinPhotoHandler(){
    winViewer4PhotoKeyHandler=g_signal_connect(G_OBJECT(winViewer), "key-press-event", G_CALLBACK(winViewer4PhotoKeyCB), NULL);
    winViewer4PhotoClickHandler=g_signal_connect (G_OBJECT(winViewer),"button-press-event",G_CALLBACK (winViewer4PhotoClickCB), NULL);  
}

//remove the connection the handler to the multiViewer Window for further connections
void disconnectWinPhotoHandler(){
    g_signal_handler_disconnect(G_OBJECT(winViewer), winViewer4PhotoKeyHandler);
    g_signal_handler_disconnect(G_OBJECT(winViewer), winViewer4PhotoClickHandler);
}    

/*
create the widget structure to display the photo
*/
void photoWidgetInit(){
    //css init
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);

    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); 
    
    gchar *CSS="\
    #boxViewWindow{\
        background-color:rgba(51,51,49,1);\
        color:white;\
    }\
    ";     
    gtk_css_provider_load_from_data(provider, CSS, -1, NULL);
    g_object_unref (provider);
	
    //add 2 layers overlay to managed a status message 
    GtkWidget *pMainOverlay = gtk_overlay_new ();
    gtk_container_add(GTK_CONTAINER(winViewer), pMainOverlay);
	
	//add scrolled panel for layer 1
    GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (pScrolledWindow), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC); //scroll vertical
	gtk_container_add(GTK_CONTAINER(pMainOverlay), pScrolledWindow);
	
	GtkAdjustment *pAdjustmentV = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(pScrolledWindow));
	GtkAdjustment *pAdjustmentH = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW(pScrolledWindow));
	g_signal_connect (G_OBJECT (pAdjustmentV),"changed", G_CALLBACK(scrolledPanelChangedCB), NULL);
	g_signal_connect (G_OBJECT (pAdjustmentH),"changed", G_CALLBACK(scrolledPanelChangedCB), NULL);
	
	GtkWidget *pVBox=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);	
	gtk_container_add(GTK_CONTAINER(pScrolledWindow), pVBox);
	//the boxViewWindow manages the photo to display
	pBoxViewWindow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(pBoxViewWindow,"boxViewWindow");
    gtk_box_pack_start(GTK_BOX(pVBox), pBoxViewWindow, TRUE, TRUE, 0);
   //add the status message - layer 2
    statusMessageViewer=gtk_label_new("                   ");
    gtk_widget_set_halign(statusMessageViewer,GTK_ALIGN_END);
    gtk_widget_set_valign(statusMessageViewer,GTK_ALIGN_END);
    //gtk_label_set_selectable (GTK_LABEL(statusMessageViewer),TRUE);
    gtk_widget_set_name(statusMessageViewer,"statusMessage");
    gtk_overlay_add_overlay (GTK_OVERLAY (pMainOverlay), statusMessageViewer); 
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(pMainOverlay),statusMessageViewer,TRUE);
    gtk_widget_set_no_show_all (statusMessageViewer, TRUE);
    //reset the zoom scale
    zoomScale=1.0f;
}

/*
 find, load, add the photo to the photowidget
*/
int photoWidgetLoad(int index){
    GError *err = NULL;
    GdkPixbufLoader *loader=NULL;
    int ret=FALSE;
    if (viewedFullPath!=NULL) g_free(viewedFullPath);
    //get the PhotoObj
    FileObj *fileObj=g_ptr_array_index(viewedFileSet,index);
    char *currentDir=getFullPathFromidNode(viewedDirNode,NULL);
    viewedFullPath=g_strdup_printf("%s/%s", currentDir, fileObj->name);
    //retreive screen height and screen width
    /*GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);
    int screenWidth=gdk_screen_get_width (screen);
    int screenHeight=gdk_screen_get_height (screen);*/
    int screenWidth=getMonitorWidth(pWindowOrganizer); //we can't use winViewer because not realized yet
    int screenHeight=getMonitorHeight(pWindowOrganizer);

	//g_print("screen width%i ,height%i",screenWidth,screenHeight);

    //get image width and height
    int imgWidth, imgHeight;
    ret=getPhotoSize(viewedFullPath, &imgWidth, &imgHeight);
    //debug
    /*if (!ret) {
        imgWidth=744;
        imgHeight=992;
        ret=TRUE;
    }*/
    //enddebug
    g_print("photosize %ix%i",imgWidth,imgHeight);
	if (!ret) return FALSE;
	
	//get orientation
    int j=getPhotoOrientation(viewedFullPath);   
    int rotate90Needed=FALSE;
    if (j==6 || j==8) rotate90Needed=TRUE;
    if (rotate90Needed) {int _imgWidth=imgWidth;imgWidth=imgHeight;imgHeight=_imgWidth;} //on inverse width et height

    //process the scale to see the maximum of the image
    int compX,compY;// compression in x or y direction
    float ratioScreen,ratioImage;
    ratioScreen=(float)screenHeight/screenWidth;
    ratioImage=(float)imgHeight/imgWidth;
    int screenWider = (ratioScreen<=ratioImage); //screen wider than image 16/9 and 3/4
    if (screenWider){ 
        compY=screenHeight;
        compX=(float)imgWidth * screenHeight / imgHeight +1; //round to upper int    
    } else {
        compX=screenWidth;
        compY=(float)imgHeight * screenWidth / imgWidth +1; //round to upper int
    }
    //g_print("compX%i-compY%i",compX,compY);
    //g_print("imgWidth%i-imgHeight%i",imgWidth,imgHeight);
    GdkPixbuf *src;
    
    //zoomScale adjusted if dividing width/height<0.66
    float ratioImage2=(float)imgWidth/imgHeight;
    //if (rotate90Needed) ratioImage2=(float)1/ratioImage2; deja inversé
    g_print("-ratio %f-",ratioImage2);
    if (zoomScale == 1.0f && ratioImage2<0.66f) zoomScale=1.4f;
    g_print("rotate_neededed  %i, compX %i, compY%i, zoomscale%f",rotate90Needed,compX,compY,zoomScale);
    //load image from file at the correct scale
    if (!rotate90Needed) src = gdk_pixbuf_new_from_file_at_size(viewedFullPath, compX * zoomScale, compY * zoomScale, &err); 
    else src = gdk_pixbuf_new_from_file_at_size(viewedFullPath, compY * zoomScale,compX * zoomScale, NULL); 
    if (err){
        //g_print ("-error: %s\n", err->message);
        g_print("pixbuf loader needed");
        //if file is too large files, gdk_pixbuf_new_from_file_at_size can fail
        //we give a second chance with GdkPixbufLoader
        GFile *_file=g_file_new_for_path(viewedFullPath);
        err=NULL;
        GFileInputStream *input_stream = g_file_read (_file, NULL, &err);
        GdkPixbufLoader *loader=gdk_pixbuf_loader_new();
        if (!rotate90Needed) gdk_pixbuf_loader_set_size(loader,compX * zoomScale, compY * zoomScale); 
        else gdk_pixbuf_loader_set_size(loader, compY * zoomScale, compX * zoomScale); 
        guchar *buffer = g_new0 (guchar, 65535);
	    goffset bytes_read, bytes_read_total = 0;
        while (TRUE) {
            bytes_read = g_input_stream_read (G_INPUT_STREAM (input_stream), buffer, 65535, NULL, &err);
            if  (bytes_read==0) break;
            gdk_pixbuf_loader_write (loader, buffer, bytes_read, &err);
            bytes_read_total+=bytes_read;
        }
        //g_print("totalbytesread %i",bytes_read_total);
        if (err){g_print ("-error: %s\n", err->message);}
        src=gdk_pixbuf_loader_get_pixbuf (loader);
        g_free (buffer);
        g_object_unref (G_OBJECT (input_stream));
    }
    if (src==NULL) return FALSE;
    
    GdkPixbuf *rot=NULL;  //on ne gère que les 3 6 8 sinon en plus de la rotation il faut faire une symétrie

    if (j==3) rot=gdk_pixbuf_rotate_simple(src,GDK_PIXBUF_ROTATE_UPSIDEDOWN); //rotate 180
    if (j==6) rot=gdk_pixbuf_rotate_simple(src,GDK_PIXBUF_ROTATE_CLOCKWISE); //rotate 90 dans le sens horaire
    if (j==8) rot=gdk_pixbuf_rotate_simple(src,GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE); //rotate 90 dans le sens contrairehoraire
	
	//create GtkImage and add widget to gtk container
	if (rot) pImage=gtk_image_new_from_pixbuf(rot);  
	else     pImage=gtk_image_new_from_pixbuf(src);      
    
   if (loader){
       gdk_pixbuf_loader_close (loader, NULL);
       g_object_unref(loader);
   }  
    g_object_unref(src);  
    if (rot) g_object_unref(rot);
    //old version g_free(fileFullPath);            
    gtk_box_pack_start(GTK_BOX(pBoxViewWindow), pImage, TRUE, TRUE, 0);

    return TRUE;
}



/*
Give zooming facilities to the photo
*/
static void zoomPhoto(ZoomType type){
    static float zoomStep=1.40001f; //workaround 0.00001 for ratioImage2
    static float zoomLimitMax=10.0f;
    static float zoomLimitMin=0.05f;
    if (type==ZOOM_IN) {
        if (zoomScale * zoomStep > zoomLimitMax) return;
        zoomScale= zoomScale * zoomStep;
    } else {
        if (zoomScale / zoomStep < zoomLimitMin) return;
        zoomScale= zoomScale / zoomStep;
    }
    g_print("-zoomscale%f", zoomScale);
    gtk_widget_destroy(pImage);

    int ret=photoWidgetLoad(viewedIndex); 
            
    if (!ret) updateStatusMessageViewer("Sorry, an error happens loading the photo!");
    
    gtk_widget_show_all(pImage);     
}

/*
Set scroll cursor to center after zooming
*/
static gboolean  scrolledPanelChangedCB (GtkAdjustment  *pAdjustment ,  gpointer data){
    // g_print("scrolledPanelChangedCB page size, %0.f, upper %0.f, val %0.f",gtk_adjustment_get_page_size (pAdjustment),
    //gtk_adjustment_get_upper (pAdjustment),gtk_adjustment_get_value (pAdjustment));
    gdouble upper=gtk_adjustment_get_upper (pAdjustment);
    gdouble pageSize=gtk_adjustment_get_page_size (pAdjustment);
    gdouble value=gtk_adjustment_get_value (pAdjustment);
    if (upper>pageSize){
        gdouble _value=upper/2.0f-pageSize/2.0f;
        if (value!=_value) gtk_adjustment_set_value(pAdjustment,_value);
    }
}

/*
Show the popup menu with ctrl click.
widget is winViewer
*/
static gboolean  winViewer4PhotoClickCB (GtkWidget *widget, GdkEventButton *event, gpointer data)  {
  //common callback to video and photo
  if (multiViewerClickCB(widget,event,data)) return FALSE; //no more processing in this function
    
  if (event->type == GDK_BUTTON_PRESS){        
        if (event->button == GDK_BUTTON_SECONDARY || event->state & GDK_CONTROL_MASK)  {
            g_print("clickright"); //ctrl click for mac support
            gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, NULL, NULL, event->button, event->time); //gtk-3.18
            //gtk_menu_popup_at_pointer(GTK_MENU(pMenuPopup), NULL); //gtk-3.22
            return TRUE;
        }
    }
    return FALSE;
}

/*
Handle the input keys of winViewer for photos 
*/
static gboolean winViewer4PhotoKeyCB(GtkWidget *widget,  GdkEventKey *event) {
    //common callback to video and photo
   if (multiViewerKeyCB(widget, event)) return FALSE; //no more processing in this function

    switch (event->keyval) {
    case GDK_KEY_Return:
        if (event->state & GDK_MOD1_MASK) {
            g_print("-alt return");
            showPropertiesDialog();
            break;
        }    
        g_print("-return");
        #ifdef OSX
        gtk_window_unfullscreen(GTK_WINDOW(winViewer));
        #endif
        multiViewerWidgetDestroy();
        gtk_widget_destroy(widget);//we destroy the window
        break;
    case GDK_KEY_i:
    case GDK_KEY_I: //OSX shortcut
        if (event->state & GDK_META_MASK) {
            g_print("-alt return");
            showPropertiesDialog();
        }
        break;
    case GDK_KEY_equal:        
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            g_print("ctrl plus ");
            zoomPhoto(ZOOM_IN);
        }
        break;    
    case GDK_KEY_plus:        
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            g_print("ctrl plus ");        
            zoomPhoto(ZOOM_IN);
        }
        break;
    case GDK_KEY_minus:        
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            g_print("ctrl minus ");
            zoomPhoto(ZOOM_OUT);        
        }
        break;
    case GDK_KEY_c:
    case GDK_KEY_C:
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            g_print("-ctrl c");
            copyToDialog();
        }
        break;
    case GDK_KEY_x:
    case GDK_KEY_X:
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            g_print("-ctrl x");
            moveToDialog();
        }
        break; 
    case  GDK_KEY_Menu:
        g_print("-key menu");
        gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, NULL, NULL, 0, event->time); //gtk-3.18
        //gtk_menu_popup_at_pointer(GTK_MENU(pMenuPopup), NULL); //gtk-3.22
        break;
    case GDK_KEY_o:
    case GDK_KEY_O:
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            g_print("-ctrl o");
            //on ouvre le context menu 
            gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, NULL, NULL, 0, event->time);//gtk-3.18
            //gtk_menu_popup_at_pointer(GTK_MENU(pMenuPopup), NULL); //gtk-3.22
        }
        break;   
    case GDK_KEY_Delete:
    case GDK_KEY_BackSpace:
        deleteInViewer();
        break;
    }   
    return FALSE;
}
