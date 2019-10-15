//photoViewer.c
//This source show the photo in fullscreen
//you can navigate directly to the next or the previous photo of the current dir but also if you get to the first photo of a dir, go to the last photo of the previous dir. You are not limited to the current dir.
//You can also zoom in and out the photo.
//You can trigger actions to the photo file as you do in the photoorganizer (copy, delete, share, change date...)
//to compile see gcc in photoOrganizer.c

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <tsoft.h>
#include <photoOrganizer.h>
#include <photoViewer.h>
#include <photoDialogs.h>
#include <photoInit.h>

typedef enum ZoomType {
	ZOOM_IN,ZOOM_OUT
}ZoomType;

//private global function
static int addPhoto2Viewer(int index);
static void windowDestroyCB(GtkWidget *pWidget, gpointer pData);
static gboolean windowKeyCB(GtkWidget *widget,  GdkEventKey *event);
static gboolean  windowPressCB (GtkWidget *widget, GdkEventButton *event, gpointer data);
static void zoomPhoto(ZoomType type);
static gboolean  scrolledPanelChangedCB (GtkAdjustment  *pAdjustment ,  gpointer data);
static gboolean  windowMapCallBack (GtkWidget *widget, GdkEvent *event, gpointer data) ;
static int getPrevious(int index);
static int getNext(int index);
static void chooserSelectionCB (GtkAppChooserWidget *self,    GAppInfo   *application, gpointer  user_data);
static void chooserActivationCB (GtkAppChooserWidget *self,    GAppInfo   *application, gpointer  user_data);

//private global var
static GtkWidget *pBoxViewWindow;
static GtkWidget *pImage;
static float zoomScale=1.0f; //used for crl+ and ctrl-
static GtkWidget *pWindow, *pStatusMessage;
GtkWidget *pWindowViewer;
static int viewedPhotoIndex;  //index is the place of the photo in the CURRENT FOLDER 
static int viewedPhotoRow; //index in rowArray
static int viewedPhotoDirNode;
static GPtrArray *fileSet= NULL; //used in ALL organizer type, filled with the current folder
static int initPhotoIndex;
static GtkWidget *parentWindow;
//public function are declared in the header

//public global var
char *viewedFullPath; //full path of the current photo
int organizerNeed2BeRefreshed=FALSE; //to call refreshArray when we destroy the viewer


void photoViewerInit(GtkWindow *parent, int index, int monitor){
    initPhotoIndex=index;
    parentWindow=GTK_WIDGET(parent);
    //add pWindow
    //pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    pWindow = gtk_application_window_new(app);

    pWindowViewer=pWindow; //for sharing
    g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(windowDestroyCB), NULL); 
    g_signal_connect(G_OBJECT(pWindow), "key-press-event", G_CALLBACK(windowKeyCB), NULL);
    gtk_widget_set_events (pWindow, GDK_BUTTON_PRESS_MASK );
	gtk_window_set_title(GTK_WINDOW(pWindow),  "");
	gtk_window_set_decorated(GTK_WINDOW(pWindow),FALSE);  //no window border 
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pWindow),TRUE);  //to avoid multi windows in the app manager
    gtk_window_set_modal (GTK_WINDOW(pWindow),TRUE);
    gtk_window_set_transient_for (GTK_WINDOW(pWindow), parent);
    g_signal_connect(G_OBJECT(pWindow),"map-event",G_CALLBACK(windowMapCallBack), NULL);
     
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);
    
    #ifdef OSX
    //bug with gtk_window_fullscreen on mac osx
    gtk_window_set_default_size(GTK_WINDOW(pWindow),getMonitorWidth(parentWindow), getMonitorHeight(parentWindow));
	#else
    //manage monitor number we change the location of the viewer to put it in the same monitor
    //we have to move the window to the position of the new monitor before doing fullscreen
    GdkRectangle rect;    
    gdk_screen_get_monitor_geometry(screen, monitor, &rect); //we use this function to be compliant with gtk-3.18    
    g_print("\nmonitor%i rectX %i rectY%i\n",monitor, rect.x, rect.y);
    gtk_window_move (GTK_WINDOW(pWindow), rect.x, rect.y);    
	gtk_window_fullscreen(GTK_WINDOW(pWindow)); //fullscreen
	#endif
    //css init
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
    gtk_container_add(GTK_CONTAINER(pWindow), pMainOverlay);
	
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
		
	pBoxViewWindow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(pBoxViewWindow,"boxViewWindow");
    gtk_box_pack_start(GTK_BOX(pVBox), pBoxViewWindow, TRUE, TRUE, 0);
    
    //get the PhotoObj
    PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,index);    
    
    g_print("viewer row %i\n",pPhotoObj->row);
    RowObj *row=g_ptr_array_index(rowArray,pPhotoObj->row);
    viewedPhotoDirNode=row->idNode;
    viewedPhotoIndex=row->index+pPhotoObj->col;
    viewedPhotoRow=pPhotoObj->row;
    char *currentDir= getFullPathFromidNode(viewedPhotoDirNode, NULL);
    g_print("current dir =%s",currentDir);
    fileSet=getDirSortedByDate(currentDir); 
    
    //add image
	int ret=addPhoto2Viewer(viewedPhotoIndex);
	
	//add error if necessary
	if (!ret){
	    GtkWidget *pLabel1=gtk_label_new("Sorry, an error happens!");
        gtk_box_pack_start(GTK_BOX(pBoxViewWindow), pLabel1, TRUE, TRUE, 0);
	}

    //add the status message - layer 2
    pStatusMessage=gtk_label_new("                   ");
    gtk_widget_set_halign(pStatusMessage,GTK_ALIGN_END);
    gtk_widget_set_valign(pStatusMessage,GTK_ALIGN_END);
    //gtk_label_set_selectable (GTK_LABEL(pStatusMessage),TRUE);
    gtk_widget_set_name(pStatusMessage,"statusMessage");
    gtk_overlay_add_overlay (GTK_OVERLAY (pMainOverlay), pStatusMessage); 
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(pMainOverlay),pStatusMessage,TRUE);
    gtk_widget_set_no_show_all (pStatusMessage, TRUE);
    
    gtk_widget_show_all(pWindow);        
}

/*
 find the photo, create the widget and attach it to the gtk container
*/
static int addPhoto2Viewer(int index){
    GError *err = NULL;
    GdkPixbufLoader *loader=NULL;
    int ret=FALSE;
    if (viewedFullPath!=NULL) g_free(viewedFullPath);
    //get the PhotoObj
    FileObj *fileObj=g_ptr_array_index(fileSet,index);
    char *currentDir=getFullPathFromidNode(viewedPhotoDirNode,NULL);
    viewedFullPath=g_strdup_printf("%s/%s", currentDir, fileObj->name);
    //retreive screen height and screen width
    /*GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);
    int screenWidth=gdk_screen_get_width (screen);
    int screenHeight=gdk_screen_get_height (screen);*/
    int screenWidth=getMonitorWidth(parentWindow); //we can't use pWindow because not realized yet
    int screenHeight=getMonitorHeight(parentWindow);

	//g_print("screen width%i ,height%i",screenWidth,screenHeight);

    //get image width and height
    int imgWidth, imgHeight;
    ret=getPhotoSize(viewedFullPath, &imgWidth, &imgHeight);
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

    int ret=addPhoto2Viewer(viewedPhotoIndex); 
	if (!ret){
	    GtkWidget *pLabel1=gtk_label_new("Sorry, an error happens!");
        gtk_box_pack_start(GTK_BOX(pBoxViewWindow), pLabel1, TRUE, TRUE, 0);
	}
    
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
Handle the input keys
*/
static gboolean windowKeyCB(GtkWidget *widget,  GdkEventKey *event) {
    //g_print("-key %s modif%i", gdk_keyval_name (event->keyval), event->is_modifier); // google  gdkkeysyms.h
    //handle repeat key (to heavy to display image at normal key repeat      
    static gint64 lastTime=0;
    static gint lastKeyVal=0;
    static gint repeatKeyStepMin=250; //in ms
    gint64 now=g_get_monotonic_time () / 1000;// clock from 1970 in ms
    if (lastKeyVal!=event->keyval){
        lastKeyVal=event->keyval;
        lastTime=now; 
    } else { //we've got a repeat key
        if (now-lastTime < repeatKeyStepMin)
            return FALSE; //no action
        else 
            lastTime=now; //action
    }
    
    int newSelection=-1;    
    int res=-1;
    int ret=-1;
    char *msg;
    GtkWidget *pLabel;
    GtkWidget *appChooser;
    GFile *pFile;
    GtkWidget *appChooserWidget;
    GList *listApp, *l;
    GAppInfo *appInfo;
    
    gtk_widget_hide(pStatusMessage); //reset the message status before action
    
    switch (event->keyval) {
    case GDK_KEY_Up:
    case GDK_KEY_Left:
        if (event->state & GDK_CONTROL_MASK) break;
        res=getPrevious(viewedPhotoIndex);
        if (res==-1) {
            updateStatusMessageViewer("You can't go to the left. This is the first photo!");
            //gtk_widget_show(pStatusMessage);
            break;
        } 
        removeAllWidgets(GTK_CONTAINER(pBoxViewWindow)); //we destroy the image
        zoomScale=1.0f;
        /*while ( (res=addPhoto2Viewer(--viewedPhotoIndex))==FALSE || viewedPhotoIndex==0){} //we add a new image
        if (!res) {
            pPhotoObj=g_ptr_array_index(photoArray,viewedPhotoIndex);        
            msg=g_strdup_printf ("Sorry, I can't display this file %s",pPhotoObj->fullPath);        
            GtkWidget *pLabel =gtk_label_new(msg);
            gtk_box_pack_start(GTK_BOX(pBoxViewWindow), pLabel, TRUE, TRUE, 0);
            g_free(msg);
        }*/
        viewedPhotoIndex=res;
        ret=addPhoto2Viewer(viewedPhotoIndex); 
        if (!ret){
            GtkWidget *pLabel1=gtk_label_new("Sorry, an error happens!");
            gtk_box_pack_start(GTK_BOX(pBoxViewWindow), pLabel1, TRUE, TRUE, 0);
        }
        gtk_widget_show_all(widget);//we show the image
        g_print("-left");
        break;
    case GDK_KEY_Down:
    case GDK_KEY_Right: 
        if (event->state & GDK_CONTROL_MASK) break;
        res=getNext(viewedPhotoIndex);
        if (res==-1) {
            updateStatusMessageViewer("You can't go to the right. This is the last photo!");
            //gtk_widget_show(pStatusMessage);
            break;
        }
        removeAllWidgets(GTK_CONTAINER(pBoxViewWindow)); //we destroy the image
        zoomScale=1.0f;
        viewedPhotoIndex=res;
        ret=addPhoto2Viewer(viewedPhotoIndex); 
        if (!ret){
            GtkWidget *pLabel1=gtk_label_new("Sorry, an error happens!");
            gtk_box_pack_start(GTK_BOX(pBoxViewWindow), pLabel1, TRUE, TRUE, 0);
        }
        gtk_widget_show_all(widget);//we show the image
        g_print("-right");
        break;
    case GDK_KEY_Escape:
        g_print("-escape");
        removeAllWidgets(GTK_CONTAINER(widget)); //we clean the content of the window
        gtk_widget_destroy(widget);//we destroy the window
        break;
    case GDK_KEY_Return:
        if (event->state & GDK_MOD1_MASK) {
            g_print("-alt return");
            showExifDialog(TRUE);
            break;
        }    
        g_print("-return");
        removeAllWidgets(GTK_CONTAINER(widget)); //we clean the content of the window
        gtk_widget_destroy(widget);//we destroy the window
        break;
    case GDK_KEY_equal:        
        if (event->state & GDK_CONTROL_MASK) {
            g_print("ctrl plus ");
            zoomPhoto(ZOOM_IN);
        }
        break;    
    case GDK_KEY_plus:        
        if (event->state & GDK_CONTROL_MASK) {
            g_print("ctrl plus ");        
            zoomPhoto(ZOOM_IN);
        }
        break;
    case GDK_KEY_minus:        
        if (event->state & GDK_CONTROL_MASK) {
            g_print("ctrl minus ");
            zoomPhoto(ZOOM_OUT);        
        }
        break;
    case GDK_KEY_c:
    case GDK_KEY_C:
        if (event->state & GDK_CONTROL_MASK) {
            g_print("-ctrl c");
            copyToDialog();
        }
        break; 
    case  GDK_KEY_Menu:
        g_print("-key menu");
        gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, NULL, NULL, 0, event->time); //gtk-3.18
        //gtk_menu_popup_at_pointer(GTK_MENU(pMenuPopup), NULL); //gtk-3.22
        break;
    case GDK_KEY_o:
    case GDK_KEY_O:
        if (event->state & GDK_CONTROL_MASK) {
            g_print("-ctrl o");
            //on ouvre le context menu 
            gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, NULL, NULL, 0, event->time);//gtk-3.18
            //gtk_menu_popup_at_pointer(GTK_MENU(pMenuPopup), NULL); //gtk-3.22
        }
        break;   
    case GDK_KEY_Delete:
        deleteInViewer();
        break;
    }   
    return FALSE;
}

//we don't use the delete from photoOrganizer.c because it becomes too complex to play with active window
void deleteInViewer(void){
    GtkDialogFlags flags;
    GtkWidget *dialog;
    PhotoObj *pPhotoObj;
    int deleteConfirm;
    int removeResult;    
    int res=-1;
    char *fileFullPath;
    //get the current PhotoObj
    FileObj *fileObj=g_ptr_array_index(fileSet,viewedPhotoIndex);
    char *currentDir=getFullPathFromidNode(viewedPhotoDirNode,NULL);
    fileFullPath=g_strdup_printf("%s/%s", currentDir, fileObj->name);

    flags = GTK_DIALOG_DESTROY_WITH_PARENT;
    dialog = gtk_message_dialog_new (GTK_WINDOW(pWindow),flags,
                            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,"Do you really want to delete the file %s ?",fileFullPath);
    gtk_widget_grab_focus(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK)); //set default button
    deleteConfirm = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (deleteConfirm==GTK_RESPONSE_OK) { 
        removeResult = remove(fileFullPath);
        //g_print("remove result %i",removeResult);
        if(removeResult==0) {           
            g_print("File deleted successfully %s",fileFullPath);
            g_ptr_array_remove_index(fileSet,viewedPhotoIndex);
            viewedPhotoIndex--;
            organizerNeed2BeRefreshed=TRUE;
            //try to show the next or the previous photo if exists
            res=getNext(viewedPhotoIndex);
            //g_print("getnext %i\n",res);
            if (res==-1) {
                res=getPrevious(viewedPhotoIndex);
                if (res==-1) {
                    //g_print("you are in\n");
                    viewedPhotoIndex=-1;
                    gtk_widget_destroy(dialog);
                    removeAllWidgets(GTK_CONTAINER(pWindow));
                    gtk_widget_destroy(pWindow);
                    return ;
                }
            }
            removeAllWidgets(GTK_CONTAINER(pBoxViewWindow)); //we destroy the image
            zoomScale=1.0f;
            viewedPhotoIndex=res;
            int ret=addPhoto2Viewer(viewedPhotoIndex); 
            if (!ret){
                GtkWidget *pLabel1=gtk_label_new("Sorry, an error happens!");
                gtk_box_pack_start(GTK_BOX(pBoxViewWindow), pLabel1, TRUE, TRUE, 0);
            }
            gtk_widget_show_all(pBoxViewWindow);//we show the image 

        }
        else {           
          g_print("Error: unable to delete the file %s",fileFullPath);
        }
    }  
}
/*
refresh the viewer after a move action (called by moveToDialog)
*/
void moveInViewer(void){
            g_ptr_array_remove_index(fileSet,viewedPhotoIndex);
            viewedPhotoIndex--;
            organizerNeed2BeRefreshed=TRUE;
            //try to show the next or the previous photo if exists
            int res=getNext(viewedPhotoIndex);
            if (res==-1) {
                res=getPrevious(viewedPhotoIndex);
                if (res==-1) {
                    viewedPhotoIndex=-1;
                    removeAllWidgets(GTK_CONTAINER(pWindow));
                    gtk_widget_destroy(pWindow);
                    return ;
                }
            }
            removeAllWidgets(GTK_CONTAINER(pBoxViewWindow)); //we destroy the image
            zoomScale=1.0f;
            viewedPhotoIndex=res;
            int ret=addPhoto2Viewer(viewedPhotoIndex); 
            if (!ret){
                GtkWidget *pLabel1=gtk_label_new("Sorry, an error happens!");
                gtk_box_pack_start(GTK_BOX(pBoxViewWindow), pLabel1, TRUE, TRUE, 0);
            }
            gtk_widget_show_all(pBoxViewWindow);//we show the image 
}

/*
Handle double click to close the window.
Show the popup menu with ctrl click.
*/
static gboolean  windowPressCB (GtkWidget *widget, GdkEventButton *event, gpointer data)  {
    if (event->type==GDK_2BUTTON_PRESS) {
        g_print("-doubleclicked on the view window");
        removeAllWidgets(GTK_CONTAINER(widget)); //we clean the content of the window
        gtk_widget_destroy(widget);//we destroy the window        
        return TRUE; //interruption
    } else if (event->type == GDK_BUTTON_PRESS){        
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
When the window is closing 
*/
static void windowDestroyCB(GtkWidget *pWidget, gpointer pData){
	g_print("window distroyed");
    activeWindow=ORGANIZER; //we set it here, too late in photoorganizer.c 
	zoomScale=1.0f;

    if (organizerNeed2BeRefreshed){
        refreshPhotoArray(TRUE);
        organizerNeed2BeRefreshed=FALSE;
    }
    RowObj *rowObj=g_ptr_array_index(rowArray,viewedPhotoRow);
    //scroll and setfocus (the scroll is done with the changefocus function)
    int indexInPhotoArray=getPhotoIndex(viewedPhotoIndex%colMax,viewedPhotoRow);
    if (!rowObj->loaded==TRUE){
        if (indexInPhotoArray>=initPhotoIndex) scroll2Row(viewedPhotoRow,BOTTOM); else scroll2Row(viewedPhotoRow,TOP);
        focusIndexPending=indexInPhotoArray;  //postponed the grab focus-> processed by the next run of scShowAllGtk
        g_print("back to organiser, postpone the focus for %i, row %i\n",focusIndexPending,viewedPhotoRow);
    }else {
        if (indexInPhotoArray>=initPhotoIndex) changeFocus(indexInPhotoArray,BOTTOM,TRUE); else changeFocus(indexInPhotoArray,TOP,TRUE);
    }
}

/*
When the Photoviewer main window has been shown
*/
static gboolean  windowMapCallBack (GtkWidget *widget, GdkEvent *event, gpointer data)  {
    //g_print("view window ready");
    activeWindow=VIEWER;
    g_signal_connect (G_OBJECT (pWindow),"button-press-event",G_CALLBACK (windowPressCB), NULL);   // we set this event handler after the initialisation to avoid conflict with the double click in the main window
}

/*
return the previous photo index 
*/
static int getPrevious(int index){
    if (viewedPhotoIndex==0){ //first photo of the current dir
        int previousDir=getPreviousDir(&viewedPhotoRow);
        if (previousDir!=-1){
            char *currentDir= getFullPathFromidNode(previousDir, NULL);
            g_ptr_array_unref(fileSet);
            fileSet=getDirSortedByDate(currentDir);
            viewedPhotoDirNode=previousDir;
            viewedPhotoIndex=fileSet->len -1;            
        }else {
            return -1; //error
        }
    } else {
        int row2sub=viewedPhotoIndex%colMax;
        viewedPhotoIndex--;
        if (row2sub==0) viewedPhotoRow--;
    }
    return viewedPhotoIndex;
}

/*
return the next photo index 
*/
static int getNext(int index){
    if (viewedPhotoIndex==(fileSet->len-1)){ //last photo of the current dir
        int nextDir=getNextDir(&viewedPhotoRow);
        if (nextDir!=-1){
            char *currentDir= getFullPathFromidNode(nextDir, NULL);
            g_ptr_array_unref(fileSet);
            fileSet=getDirSortedByDate(currentDir);
            viewedPhotoDirNode=nextDir;
            viewedPhotoIndex=0;            
        }else {
            return -1; //error
        }
    } else {
        viewedPhotoIndex++;
        int row2add=viewedPhotoIndex%colMax;
        if (row2add==0) viewedPhotoRow++;
    }
    return viewedPhotoIndex;
}

void updateStatusMessageViewer(char *value){
    gtk_label_set_text(GTK_LABEL(pStatusMessage),value);
    gtk_widget_show(pStatusMessage);
}

