/*multiViewer.c
This source create a new fullscreen window to display photo or video
It degelates the display to photoViewer.c or videoViewer.c through function multiViewerWidgetInit, multiViewerWidgetLoad 
It will handle the navigation which is common between photo and videos 
You can navigate directly to the next or the previous photo of the current dir but also if you get to the first photo of a dir, go to the last photo of the previous dir. You are not limited to the current dir. 
*/
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <tsoft.h>
#include <photoOrganizer.h>
#include <multiViewer.h>
#include <photoViewer.h>
#include <videoViewer.h>
#include <photoDialogs.h>
#include <photoInit.h>





//private global function
static void windowDestroyCB(GtkWidget *pWidget, gpointer pData);
static gboolean  windowMapCallBack (GtkWidget *widget, GdkEvent *event, gpointer data) ;

//private global var


static int viewedRow; //index in rowArray
static int initViewedIndex;
static GtkWidget *parentWindow;
static ViewerType viewerType;
static int winViewerReady=FALSE;
static gboolean notFullScreen=FALSE;

//public function are declared in the header

//public global var
char *viewedFullPath; //full path of the current photo
int viewedDirNode; // idNode  Folder for the current photo 
GPtrArray *viewedFileSet= NULL; //set of files filled with the current folder
int viewedIndex;  //this index is the place of the photo/video in the CURRENT FOLDER 

int organizerNeed2BeRefreshed=FALSE; //to call refreshArray when we destroy the viewer
GtkWidget *winViewer;
GtkWidget *statusMessageViewer;


void multiViewerInit(GtkWindow *parent, int index, int monitor){
    //init the winviewer
    parentWindow=GTK_WIDGET(parent);
    winViewer = gtk_application_window_new(app);
    gtk_widget_set_events (winViewer, GDK_BUTTON_PRESS_MASK );
	//gtk_window_set_title(GTK_WINDOW(winViewer),  "");
	//gtk_window_set_decorated(GTK_WINDOW(winViewer),FALSE);  //no window border 
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(winViewer),TRUE);  //to avoid multi windows in the app manager
    gtk_window_set_modal (GTK_WINDOW(winViewer),TRUE);
    gtk_window_set_transient_for (GTK_WINDOW(winViewer), parent);
    g_signal_connect(G_OBJECT(winViewer),"map-event",G_CALLBACK(windowMapCallBack), NULL);
    
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);
    
    #ifdef OSX
    //bug with gtk_window_fullscreen on mac osx
    gtk_window_set_default_size(GTK_WINDOW(winViewer),getMonitorWidth(parentWindow), getMonitorHeight(parentWindow));
    gtk_window_fullscreen(GTK_WINDOW(winViewer)); //fullscreen
    //gtk_window_set_keep_above(GTK_WINDOW(winViewer), TRUE);

	#else
    //manage monitor number we change the location of the viewer to put it in the same monitor
    //we have to move the window to the position of the new monitor before doing fullscreen
    GdkRectangle rect;    
    gdk_screen_get_monitor_geometry(screen, monitor, &rect); //we use this function to be compliant with gtk-3.18    
    g_print("\nmonitor%i rectX %i rectY%i\n",monitor, rect.x, rect.y);
    gtk_window_move (GTK_WINDOW(winViewer), rect.x, rect.y);
	gtk_window_fullscreen(GTK_WINDOW(winViewer)); //fullscreen 
    
	#endif
    
    //init navigation variable with PhotoObj
    initViewedIndex=index;
    PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,index);    
    
    g_print("viewer row %i\n",pPhotoObj->row);
    RowObj *row=g_ptr_array_index(rowArray,pPhotoObj->row);
    viewedDirNode=row->idNode;
    viewedIndex=row->index+pPhotoObj->col;
    viewedRow=pPhotoObj->row;
    char *currentDir= getFullPathFromidNode(viewedDirNode, NULL);
    g_print("current dir =%s",currentDir);
    viewedFileSet=getDirSortedByDate(currentDir); 
    
    multiViewerWidgetInit(viewedIndex); //create widgets/containers hierarchy in the winViewer to show a photo and display errors 
    int ret=multiViewerWidgetLoad(viewedIndex);     //add the image to display 
    if (!ret)  updateStatusMessageViewer("Sorry, an error happens in rendering the file!"); //add error if needed
        
    gtk_widget_show_all(winViewer);        
}

void multiViewerWidgetInit(int index){
    FileObj *fileObj=g_ptr_array_index(viewedFileSet,index);
    if (hasPhotoExt(fileObj->name)) viewerType=PHOTO;
    else if (hasVideoExt(fileObj->name)) viewerType=VIDEO;
    if (viewerType == PHOTO) {
        gtk_window_fullscreen(GTK_WINDOW(winViewer)); // reset to fullscreen 
        photoWidgetInit(); //create widgets/containers hierarchy in the winViewer to show a photo and display errors 
        if (winViewerReady) connectWinPhotoHandler(); //issue double click, we dont connect if winviewer is not realized
    } else if (viewerType == VIDEO) {
        gtk_window_unfullscreen(GTK_WINDOW(winViewer));//workaround to a video capture bug when PC sleep 
        gtk_window_maximize (GTK_WINDOW(winViewer));// reset to maximize 
        videoWidgetInit(); //create widgets/containers hierarchy in the winViewer to show a photo and display errors 
        if (winViewerReady) connectWinVideoHandler(); //issue double click
    }
}

int multiViewerWidgetLoad(int index){
    int ret=FALSE;
    FileObj *fileObj=g_ptr_array_index(viewedFileSet,index);
    if (viewerType == PHOTO){
        ret=photoWidgetLoad(index);     //add the image to display 
    } else if (viewerType == VIDEO){
        ret=videoWidgetLoad(index);
    }
    return ret;
}

void multiViewerWidgetDestroy(){
     if (viewerType == PHOTO){
         disconnectWinPhotoHandler();
     } else  if (viewerType == VIDEO){
         videoWidgetClose();
         disconnectWinVideoHandler();
     }
    removeAllWidgets(GTK_CONTAINER(winViewer)); //we clean the content of the window
}

/*
return the previous photo index 
*/
int getPrevious(int index){
    if (viewedIndex==0){ //first photo of the current dir
        int previousDir=getPreviousDir(&viewedRow);
        if (previousDir!=-1){
            char *currentDir= getFullPathFromidNode(previousDir, NULL);
            g_ptr_array_unref(viewedFileSet);
            viewedFileSet=getDirSortedByDate(currentDir);
            viewedDirNode=previousDir;
            viewedIndex=viewedFileSet->len -1;            
        }else {
            return -1; //error
        }
    } else {
        int row2sub=viewedIndex%colMax;
        viewedIndex--;
        if (row2sub==0) viewedRow--;
    }
    return viewedIndex;
}

/*
return the next photo index 
*/
int getNext(int index){
    if (viewedIndex==(viewedFileSet->len-1)){ //last photo of the current dir
        int nextDir=getNextDir(&viewedRow);
        if (nextDir!=-1){
            char *currentDir= getFullPathFromidNode(nextDir, NULL);
            g_ptr_array_unref(viewedFileSet);
            viewedFileSet=getDirSortedByDate(currentDir);
            viewedDirNode=nextDir;
            viewedIndex=0;            
        }else {
            return -1; //error
        }
    } else {
        viewedIndex++;
        int row2add=viewedIndex%colMax;
        if (row2add==0) viewedRow++;
    }
    return viewedIndex;
}

void updateStatusMessageViewer(char *value){
    if (value==NULL || value[0]=='\0'){g_print("\nHideStatusMessageWithValue%s\n",value);gtk_widget_hide (statusMessageViewer);return;} //hide the status message
    gtk_label_set_text(GTK_LABEL(statusMessageViewer),value);
    gtk_widget_show(statusMessageViewer);
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
    FileObj *fileObj=g_ptr_array_index(viewedFileSet,viewedIndex);
    char *currentDir=getFullPathFromidNode(viewedDirNode,NULL);
    fileFullPath=g_strdup_printf("%s/%s", currentDir, fileObj->name);

    flags = GTK_DIALOG_DESTROY_WITH_PARENT;
    dialog = gtk_message_dialog_new (GTK_WINDOW(winViewer),flags,
                            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,"Do you really want to delete the file %s ?",fileFullPath);
    gtk_widget_grab_focus(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK)); //set default button
    deleteConfirm = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (deleteConfirm==GTK_RESPONSE_OK) { 
        removeResult = remove(fileFullPath);
        //g_print("remove result %i",removeResult);
        if(removeResult==0) {           
            g_print("File deleted successfully %s",fileFullPath);
            //remove the thumbnail
            int iDir=getFileNode(currentDir);
            gchar *thumbnailPath = getThumbnailPath(thumbnailDir,iDir,fileFullPath);
            remove(thumbnailPath); 
            g_free(thumbnailPath); 
            //find the new photo to show
            g_ptr_array_remove_index(viewedFileSet,viewedIndex);
            viewedIndex--;
            organizerNeed2BeRefreshed=TRUE;
            //try to show the next or the previous photo if exists
            res=getNext(viewedIndex);
            //g_print("getnext %i\n",res);
            if (res==-1) {
                res=getPrevious(viewedIndex);
                if (res==-1) {
                    //g_print("you are in\n");
                    viewedIndex=-1;
                    gtk_widget_destroy(dialog);
                    multiViewerWidgetDestroy();
                    gtk_widget_destroy(winViewer);
                    return ;
                }
            }
            multiViewerWidgetDestroy();
            viewedIndex=res;
            multiViewerWidgetInit(viewedIndex);
            int ret=multiViewerWidgetLoad(viewedIndex); 
            if (!ret)   updateStatusMessageViewer("Sorry, an error happens loading the photo!");
            gtk_widget_show_all(winViewer);//we show the image 
        } else {           
          g_print("Error: unable to delete the file %s",fileFullPath);
          updateStatusMessageViewer(g_strdup_printf("Error: unable to delete the file %s",fileFullPath));
        }
    }  
}
/*
refresh the viewer after a move action (called by moveToDialog)
*/
void moveInViewer(void){
            g_ptr_array_remove_index(viewedFileSet,viewedIndex);
            viewedIndex--;
            organizerNeed2BeRefreshed=TRUE;
            //try to show the next or the previous photo if exists
            int res=getNext(viewedIndex);
            if (res==-1) {
                res=getPrevious(viewedIndex);
                if (res==-1) {
                    viewedIndex=-1;
                    multiViewerWidgetDestroy();
                    gtk_widget_destroy(winViewer);
                    return ;
                }
            }
            multiViewerWidgetDestroy();
            viewedIndex=res;        
            multiViewerWidgetInit(viewedIndex);        
            int ret=multiViewerWidgetLoad(viewedIndex); 
            if (!ret)   updateStatusMessageViewer("Sorry, an error happens loading the photo!");
            gtk_widget_show_all(winViewer);//we show the image 
}



/*
When the window is closing 
*/
static void windowDestroyCB(GtkWidget *pWidget, gpointer pData){
	g_print("\nwindow distroyed\n");
    winViewerReady=FALSE; //reset this value for next time
    activeWindow=ORGANIZER; //we set it here, too late in photoorganizer.c 
    
    if (organizerNeed2BeRefreshed){
        refreshPhotoArray(TRUE);
        organizerNeed2BeRefreshed=FALSE;
    } else {
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,initViewedIndex); //to get the initViewedfullpath
        //if we've just viewed a single photo with <enter> no navigation neither action on photos we don't do any changefocus or scroll in the organiser which will change our selection
        if (pPhotoObj!=NULL && g_strcmp0(pPhotoObj->fullPath,viewedFullPath) == 0) return;
    }
 
    /*scroll and setfocus (the scroll is done with the changefocus function)*/
    int indexInPhotoArray=findPhotoFromFullPath(viewedFullPath);        
    viewedRow=getPhotoRow(indexInPhotoArray); //update the viewedRow (it can have changed after refreshPhotoArray)
    RowObj *rowObj=g_ptr_array_index(rowArray,viewedRow);
    /*scroll and setfocus (the scroll is done with the changefocus function)*/
    //int indexInPhotoArray=getPhotoIndex(viewedIndex%colMax,viewedRow);
    
    if (!rowObj->loaded==TRUE){
        if (indexInPhotoArray>=initViewedIndex) scroll2Row(viewedRow,BOTTOM); else scroll2Row(viewedRow,TOP);
        focusIndexPending=indexInPhotoArray;  //postponed the grab focus-> processed by the next run of scShowAllGtk
        g_print("back to organiser, postpone the focus for %i, row %i\n",focusIndexPending,viewedRow);
    }else {
        if (indexInPhotoArray>=initViewedIndex) changeFocus(indexInPhotoArray,BOTTOM,TRUE); else changeFocus(indexInPhotoArray,TOP,TRUE);
    }
    
}

/*
When the winViewer window has been shown
*/
static gboolean  windowMapCallBack (GtkWidget *widget, GdkEvent *event, gpointer data)  {
    g_print("\nwindowMapCallBack for winviewer\n");
    activeWindow=VIEWER;
    winViewerReady=TRUE;
    g_signal_connect(G_OBJECT(winViewer), "destroy", G_CALLBACK(windowDestroyCB), NULL); //the same for photo or video 

    if (viewerType==PHOTO) {
        // we set this event handler after the initialisation to avoid conflict with the double click in the main window
        connectWinPhotoHandler(winViewer);
    } else if (viewerType==VIDEO){
        connectWinVideoHandler(winViewer);
    }
}

gboolean multiViewerClickCB (GtkWidget *widget, GdkEventButton *event, gpointer data){
    updateStatusMessageViewer(NULL); //reset the message
    //we close the window on double click 
    if (event->type==GDK_2BUTTON_PRESS) {
        g_print("-doubleclicked on the view window");
        //old call removeAllWidgets(GTK_CONTAINER(widget)); //we clean the content of the window
        multiViewerWidgetDestroy();
        gtk_widget_destroy(widget);//we destroy the window        
        return TRUE; //interruption
    }
    return FALSE;
}

gboolean multiViewerKeyCB(GtkWidget *widget,  GdkEventKey *event){
    updateStatusMessageViewer(NULL); //reset the message
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
            return TRUE; //no action
        else 
            lastTime=now; //action
    }
    
    int newSelection=-1;    
    int res=-1;
    int ret=-1;
    
    gtk_widget_hide(statusMessageViewer); //reset the message status before action

    switch (event->keyval) {
    case GDK_KEY_Up:
    case GDK_KEY_Left:
        if (event->state & GDK_CONTROL_MASK) break;
        res=getPrevious(viewedIndex);
        if (res==-1) {
            updateStatusMessageViewer("You can't go to the left. This is the first photo/video!");
            break;
        } 
        multiViewerWidgetDestroy();
        viewedIndex=res;
        multiViewerWidgetInit(viewedIndex);
        ret=multiViewerWidgetLoad(viewedIndex); 
        if (!ret) updateStatusMessageViewer("Sorry, an error happens loading the photo/video!");
        gtk_widget_show_all(widget);//we show the image
        g_print("-left");
        break;
    case GDK_KEY_Down:
    case GDK_KEY_Right: 
        if (event->state & GDK_CONTROL_MASK) break;
        res=getNext(viewedIndex);
        if (res==-1) {
            updateStatusMessageViewer("You can't go to the right. This is the last photo/video!");
            break;
        }
        multiViewerWidgetDestroy();
        viewedIndex=res;
        multiViewerWidgetInit(viewedIndex);
        ret=multiViewerWidgetLoad(viewedIndex); 
        if (!ret)   updateStatusMessageViewer("Sorry, an error happens loading the photo/video!");
        gtk_widget_show_all(widget);//we show the image
        g_print("-right");
        break;
    case GDK_KEY_Escape:
        g_print("-escape");
        #ifdef OSX
        gtk_window_unfullscreen(GTK_WINDOW(winViewer));
        #endif
        multiViewerWidgetDestroy();
        gtk_widget_destroy(widget);//we destroy the window
        break;
    }
    return FALSE; //we continue
}


