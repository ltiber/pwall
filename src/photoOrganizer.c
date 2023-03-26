/*
photoOrganizer.c
Photo organizer program
Like a filemanager, it shows the folders in the left pane and a photowall in the right.
The photowall is a kind of grid showing a square thumbnail of the photo in each cell.
Loading and unloading of this scrolling panel is made dynamicaly viewing page by viewing page like a smartphone does.
To scroll through your picture library you can select a folder in the left panel or scroll directly in the photowall. Going down and up will lead you to next or previous directory.
Most actions(copy, delete, share, change date...) on photos are available in photoDialogs.c to shorten this source and make it usable by photoViewer.c
*/

//gcc -o pepper thumbnail.c tsoft.c photoInit.c photoOrganizer.c photoViewer.c -I. `pkg-config --libs --cflags gtk+-3.0`
//gcc -o pepper thumbnail.c tsoft.c photoInit.c photoOrganizer.c photoViewer.c -I. `pkg-config --libs --cflags gtk+-3.0 libwnck-3.0`  //if libwnck needed but rejected because doesn't fit.

#include <stdlib.h>
#include <math.h>
#include <string.h>
//#define WNCK_I_KNOW_THIS_IS_UNSTABLE
//#include <libwnck/libwnck.h>
#include <gtk/gtk.h>
#include <tsoft.h>
#include <photoOrganizer.h>
#include <multiViewer.h>
#include <thumbnail.h>
#include <photoInit.h>
#include <photoDialogs.h>
#include <fileSystemMonitor.h>

//private const
#define DOUBLE_CLICK_TIME_DIF 500000   //double click time diff in microsec
#define STAGE_WIDTH  400
#define STAGE_HEIGHT 400
#define WAITING 2 //used for row->loaded (FALSE,WAITING,TRUE)

//public constante
#ifdef OSX
int META_KEY = 16; //apple command key for click
int PHOTO_SIZE = 64;
#else
int PHOTO_SIZE = 92;
#endif
int MARGIN = 2;
enum {CTRL,SHIFT};
enum {NOTHING,ONLY_VIDEOS,ONLY_PHOTOS, MIX_PHOTOS_VIDEOS};
enum {PHOTO_APP, VIDEO_APP};


//private global functions
static gboolean  clickPhotoCB (GtkWidget *event_box, GdkEventButton *event, gpointer data) ;
static void multiSelectPhoto(int index, int mode); 
static void clearSelected(int first,int last);
static int firstSelected(void);
static int lastSelected(void);
static void setSelected(int first, int last);
static GArray *rowsSelected(void);
static gboolean  keyPressCallBack(GtkWidget *widget,  GdkEventKey *event);
static int keyPostponed(gpointer user_data);
static void windowMaximizeCallBack(GtkWidget* widget,GParamSpec* property,gpointer data);
static void activateFocusCB(GtkWindow *window, gpointer   user_data);
static int whereIsTheFocus(void);
static void focusCB(GtkWindow *window, GtkWidget *widget, gpointer   user_data);
static gboolean  windowMapCB (GtkWidget *widget, GdkEvent *event, gpointer data) ;
static void windowDestroyCB(GtkWidget *pWidget, gpointer pData);
static void insertPhotoWithImage(GtkWidget *image, int col, int row, int iDir, char *fullPath, long int time);
static void updatePhotoWithImage(GtkWidget *image, int col, int row);
static GtkWidget *loadThumbnail(const char *fullPathFile, int iDir);
static int getNextRow(int index);
static int getPreviousRow(int index);
static int getRowFromFolder(int idNode);
static int getPhotoX(int index);
static int getPhotoY(int index);
static void scrollToPhoto(int index,ScrollType type);
static int getAdjustedHeight(int height);
static void refreshTree();
static void treeSelectionChangedCB (GtkTreeSelection *selection, gpointer data);
static gint sortIterCompareCB (GtkTreeModel *model, GtkTreeIter  *a, GtkTreeIter  *b, gpointer userdata);
static int treeNewSelectionCB (gpointer user_data);
static void openWithCB(GtkWidget* widget, gpointer data);
static void mailToCB(GtkWidget* widget, gpointer data);
static void whatsappCB(GtkWidget* widget, gpointer data);
static void facebookCB(GtkWidget* widget, gpointer data);
static void createPopupMenu(void);
static void setMenuPopupPosition(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data);
static void updateStatusMessageWithDetails(int index);
static void changeFileDateWithExif(void);
static void deleteMenu(void);
static gboolean  btnChangeCB (GtkWidget *event_box, GdkEventButton *event, gpointer data);
static gboolean  btnCancelChangeCB (GtkWidget *event_box, GdkEventButton *event, gpointer data);
static void createMenuTree(void);
static void renameFolderCB(GtkWidget* widget, gpointer data);
static void moveFolderCB(GtkWidget* widget, gpointer data);
static void deleteFolderCB(GtkWidget* widget, gpointer data);
static void newFolderCB(GtkWidget* widget, gpointer data);
static gboolean clickTreeCB(GtkWidget *event_box, GdkEventButton *event, gpointer data);
static gboolean expandCollapseTreeCB (GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data);
static int reClickSameFolder(gpointer user_data);
//functions to calculate the rows of the photowall
static int findCountersInArray(int idNode);
static void calculateRows ();
static gboolean calculateRowsEach (GtkTreeModel *model,GtkTreePath *path,GtkTreeIter *iter,gpointer data);
static void scrollChanged (GtkAdjustment *adjustment,  gpointer user_data);
//scrolling asynchrone functions
static void scThreadStart(void *pointer);
static int scCheckMove(int position);
static void scShowRows(int top);
static int insertImagesInRow(int row,int idNode,int index,int hasMore); //idNode of a directory
static void scCreateNewThumbnails(void);
static void scCleanArrays(void);
static int scReleaseThread(gpointer user_data);
static int scShowAllGtk(gpointer user_data);
static int getNextIdNodeNotEmpty(int idNode, GtkTreeIter *parent);
static void scSelectFolder(int idNode);
static int scShowMessageWait(gpointer user_data);
static int scShowMessageDate(gpointer user_data);
static int scHideMessageWait(gpointer user_data);
static int scUpdateNewThumbnailInGtk(gpointer user_data);
static int scRefreshPhotoWall(gpointer user_data);
//searching the treeview functions
static void searchNodes(const char *searchingText, GtkTreeIter *parent);
static void searchNext(void);
static void searchPrevious(void);
static void nextCB(GtkWidget* widget, gpointer data);
static void previousCB(GtkWidget* widget, gpointer data);
static void showHideSearchBtn(void);
static void hideSearchBtn(GtkTreePath *treePath);
static void showSearchBarCB(GtkWidget* widget, gpointer data);
static void resetCurrentFilesInFolder(void);
static int whatTypeIsSelected(void);

    
//private global variable (static makes the variable private)
static GtkWidget *pWindow;
static GtkWidget *scPhotoWall;
static GtkWidget *photoWall;
static GtkWidget *pButton;
static GtkWidget *pTree;
static GtkWidget *pMenuBar, *pMenu;
static GtkWidget *searchEntry, *searchBar, *btnDown, *btnUp;
static GtkWidget *pStatusMessage, *pScrollingDate;
static int screenWidth=-1; //initialized in the main
static int screenHeight=-1;
static char *noImagePng="noimage.png";
static char *noVideoPng="novideo.png";
static char *videoIconFile="videoIcon.png";
//static char *photosDir; useless
static int detailBoxEnabled=FALSE;
static int treeTimerFunctionOn=FALSE;
static int interruptLoading=FALSE; //used for the tree selection
static ScrollType loadPosition = TOP;
static int preferedWidth, preferedHeight;
//sc prefix is used to name global variable used in the scrolling management to fill or clear the photowall
static int scPage=-1;
static GThread *scThread=NULL;
static int scWait=FALSE;
int scMutex=0;  //used to lock portion of code in scroll thread and in refreshPhotoArray 
G_LOCK_DEFINE (scMutex);
static int scInterrupt=FALSE;
static int scStopThread=FALSE;
static int scTop=-1;
static GPtrArray *scWaitingImages;
static GPtrArray *scNewThumbnails2Create=NULL;
static GPtrArray *scWaitingNewThumbnails;
static GArray *scWaitingLabels;
static int scFocusIndexPending=-1;
static gboolean newFocusFromRefreshPhotoWall=FALSE;
static int scSelectingFolder=FALSE; //used to change the tree selection when the focus in the photowall get to a photo belonging to another directory
static int scSelectingFolderDisabled=FALSE; //used when the user change his treeselecion
static int scLastPosition=-1; //last position of the scrollbar
static char *scBeforeMessageWait=NULL; //used in scHideMessageWait to reset the final message after waiting
static GPtrArray *searchRes=NULL; //used for searching
static int searchIndex=-1; //used for the up and down searching button
static int _keyPostponed=FALSE;
static GtkWidget *_waitingScreen;
static gboolean expandCollapseTreePending; 

//public variable accessible for all the .c modules
//you need extern on the .h to let it run
GPtrArray *photoArray; //contains the photos loaded in the photowall
GPtrArray *rowArray=NULL;  //contains the rows of the photowall
ActiveWindowType activeWindow;
GtkWidget *pMenuPopup; //popup menu on the photo wall
GtkWidget *pMenuTree; //popup menu on the treeview
GtkTreeStore *pStore;
GtkTreeModel *pSortedModel;
char *photosRootDir=NULL;
GtkWidget *pWindowOrganizer=NULL;
int treeIdNodeSelected=-1;
char *treeDirSelected=NULL;
GArray *folderNodeArray; //temporary variable used only for readRecursiveDir
GArray *folderCounterArray; //temporary variable used only for readRecursiveDir
int colMax=-1; //calculated at init time depending of the size of the screen 
GtkAdjustment *scAdjustment; 
int focusIndexPending=-1; 
int scRowsInPage=-1; 
//public functions are located in the photoOrganizer.h

//main function
void photoOrganizerInit(GtkWidget *waitingScreen) {            
    GtkWidget *pVBox,  *leftPanel ,*pMainOverlay, *pHBox;    
    GtkWidget *pLabel,*pLabelLeft;
    GtkWidget *pScrolledTree;

    pWindow = gtk_application_window_new(app);
    pWindowOrganizer=pWindow; //for sharing with other modules
    gtk_window_set_position(GTK_WINDOW(pWindow), GTK_WIN_POS_CENTER); //default position
 
    //    add the menu to the titleBar     //  
    GtkWidget *headerBar= gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(headerBar),"pwall");
    GtkWidget *hamburger=gtk_button_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_MENU);//list-add-symbolic//send-to-symbolic
 
    initPrimaryMenu(hamburger);
    
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerBar),hamburger);
 
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerBar), TRUE);

    gtk_window_set_titlebar (GTK_WINDOW(pWindow), headerBar);

    //The application icon is defined in the .desktop
    g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(windowDestroyCB), NULL);
    gtk_widget_set_events (pWindow, GDK_STRUCTURE_MASK); //set which events are enabled 
    g_signal_connect(G_OBJECT(pWindow),"map-event",G_CALLBACK(windowMapCB), waitingScreen); 
    g_signal_connect(G_OBJECT(pWindow), "notify::is-maximized", G_CALLBACK(windowMaximizeCallBack), NULL);
    //TODO for testing purpose
    g_signal_connect(G_OBJECT(pWindow), "activate-focus", G_CALLBACK(activateFocusCB), NULL);
    g_signal_connect(G_OBJECT(pWindow), "activate-default", G_CALLBACK(activateFocusCB), NULL);
    //g_signal_connect(G_OBJECT(pWindow), "set-focus", G_CALLBACK(focusCB), NULL);
    
    //init css for components decoration
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);
    
    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); 

    gchar *CSS="\
    .focusBox{\
      border-style: solid;\
      border-width:4px;\
      border-color:rgba(236,129,86,1);\
      border-radius:0px;\
    }\
    .focusBoxOSX{\
      border-style: solid;\
      border-width:3px;\
      border-color:rgba(236,129,86,1);\
      border-radius:0px;\
    }\
    .photoDetailBox {\
      background-color:rgba(39,39,38,0.6);\
      font-family: Ubuntu Condensed;\
      font-size:small;\
      color: white;\
    }\
    .labelInWall {\
        color: white;\
    }\
    #photoWall{\
        background-color:rgba(51,51,49,1);\
    }\
    #pVBox{\
        background-color:rgba(51,51,49,1);\
    }\
    #thumbnailExplanation{\
        font-size:large;\
    }\
    #statusMessage{\
      background-color:rgba(51,51,49,0.5);\
      border-top-style: solid;\
      border-top-width:1px;\
      border-top-color:rgba(255,255,255,1);\
      border-top-left-radius:5px;\
      border-left-style: solid;\
      border-left-width:1px;\
      border-left-color:rgba(255,255,255,1);\
      padding:5px;\
      color:white;\
    }\
    #scrollingDate{\
      background-color:rgba(51,51,49,0.5);\
      border-bottom-style: solid;\
      border-bottom-width:1px;\
      border-bottom-color:rgba(255,255,255,1);\
      border-bottom-left-radius:5px;\
      border-left-style: solid;\
      border-left-width:1px;\
      border-left-color:rgba(255,255,255,1);\
      padding:5px;\
      color:white;\
    }\
    #treeFolder row:selected {\
        background: #f07746;\
        color: #ffffff;\
    }\
    ";
    //bg_color and fg_color of the Ambiance theme have been overriden to avoid inactive selected node to turn gray
    //it's a workaround for unpleasant change of color in the treeview but it affect dialogs
    //@define-color bg_color #f07746;\
    //@define-color fg_color #ffffff;\
    //#treeFolder row:selected {...}  is the right workaround,it overrides the gray

    gtk_css_provider_load_from_data(provider, CSS, -1, NULL);
    g_object_unref (provider);

    //calculate default size of the window
    screenWidth=getMonitorWidth(waitingScreen); //we can't use pWindow because it's not realized yet
    screenHeight=getMonitorHeight(waitingScreen);
    _waitingScreen =waitingScreen;
    g_print("screen %i,%i\n",screenWidth,screenHeight);    
    gtk_window_set_default_size(GTK_WINDOW(pWindow), screenWidth*0.8f, getAdjustedHeight(screenHeight*0.86f));

    //add 2 layers overlay to managed a status message 
    pMainOverlay = gtk_overlay_new ();
    gtk_container_add(GTK_CONTAINER(pWindow), pMainOverlay);

    /*menu layer adapt with https://developer.gnome.org/ApplicationMenu/
    GtkWidget *pMenuBox =gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous (GTK_BOX(pMenuBox), FALSE);
    gtk_container_add(GTK_CONTAINER(pMainOverlay), pMenuBox); 
    
    //add the main menu
    pMenuBar = gtk_menu_bar_new();
    createMainMenu();   
    //gtk_box_pack_start(GTK_BOX(pHBox), pMenuBar, FALSE, FALSE, 0);
    //gtk_box_pack_start(GTK_BOX(pMenuBox), pMenuBar, FALSE, FALSE, 0);
    */
    
    // 1st layer the content
    pHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous (GTK_BOX(pHBox), FALSE);
    gtk_container_add(GTK_CONTAINER(pMainOverlay), pHBox);
    //gtk_box_pack_start(GTK_BOX(pMenuBox), pHBox, TRUE, TRUE,0);
    
    // 2nd layer the status message
    pStatusMessage=gtk_label_new("                   ");
    gtk_widget_set_halign(pStatusMessage,GTK_ALIGN_END);
    gtk_widget_set_valign(pStatusMessage,GTK_ALIGN_END);
    //gtk_label_set_selectable (GTK_LABEL(pStatusMessage),TRUE);
    gtk_widget_set_name(pStatusMessage,"statusMessage");
    gtk_overlay_add_overlay (GTK_OVERLAY (pMainOverlay), pStatusMessage); 
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(pMainOverlay),pStatusMessage,TRUE);

    // 2nd layer the date scrolling info
    pScrollingDate=gtk_label_new("                   ");
    gtk_widget_set_halign(pScrollingDate,GTK_ALIGN_END);
    gtk_widget_set_valign(pScrollingDate,GTK_ALIGN_START);
    gtk_widget_set_name(pScrollingDate,"scrollingDate");
    gtk_overlay_add_overlay (GTK_OVERLAY (pMainOverlay), pScrollingDate); 
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(pMainOverlay),pScrollingDate,TRUE);
    
    
    
    
    //create a left panel
    leftPanel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(pHBox), leftPanel, TRUE, TRUE, 0);


    //we add the searching capabilities
    searchBar = gtk_search_bar_new();
    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(searchBar),TRUE);
    searchEntry=gtk_search_entry_new ();
    
    GtkWidget *searchPanel= gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(searchPanel),FALSE);
    gtk_box_pack_start (GTK_BOX (searchPanel), searchEntry, FALSE, FALSE,0);

    btnDown=gtk_button_new (); //gtk_widget_set_can_focus (btnUp, FALSE);
    GtkWidget *imgDown =gtk_image_new_from_icon_name("go-down-symbolic",GTK_ICON_SIZE_BUTTON);
    gtk_container_add (GTK_CONTAINER (btnDown), imgDown);
    gtk_box_pack_start(GTK_BOX(searchPanel), btnDown, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (btnDown),"button-press-event",G_CALLBACK (nextCB), NULL); //click
    g_signal_connect (G_OBJECT (btnDown),"key-press-event",G_CALLBACK (nextCB), NULL); //click
    
    btnUp=gtk_button_new (); //gtk_widget_set_can_focus (btnUp, FALSE);
    GtkWidget *imgUp =gtk_image_new_from_icon_name("go-up-symbolic",GTK_ICON_SIZE_BUTTON);
    gtk_container_add (GTK_CONTAINER (btnUp), imgUp);
    gtk_box_pack_start(GTK_BOX(searchPanel), btnUp, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (btnUp),"button-press-event",G_CALLBACK (previousCB), NULL); //click
    g_signal_connect (G_OBJECT (btnUp),"key-press-event",G_CALLBACK (previousCB), NULL); //click
    
    gtk_container_add (GTK_CONTAINER (searchBar), searchPanel);
    gtk_box_pack_start(GTK_BOX(leftPanel), searchBar, FALSE, FALSE, 0);
        
    //add scrolled panel to left panel
    pScrolledTree = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (pScrolledTree), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledTree),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC); //scroll vertical
    gtk_box_pack_start(GTK_BOX(leftPanel), pScrolledTree, TRUE, TRUE, 0);                                  
    
    //we prepare the sorting facilities
    pSortedModel = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(pStore));
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(pSortedModel), BASE_NAME_COL,sortIterCompareCB, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(pSortedModel), BASE_NAME_COL, GTK_SORT_DESCENDING);
    
    //we create the widget with the sorted model and init the rendering
    pTree = gtk_tree_view_new_with_model (pSortedModel);
    GtkCellRenderer *pRenderer = gtk_cell_renderer_text_new ();
    GtkTreeViewColumn *pColumn = gtk_tree_view_column_new_with_attributes (NULL, pRenderer,"text", BASE_NAME_COL,NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (pTree), pColumn);    
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pTree), FALSE);
    //gtk_widget_set_can_focus(pTree, FALSE); //release the focus
    gtk_widget_set_can_focus (pTree, TRUE);
    gtk_widget_set_name(pTree,"treeFolder");
    
    //we add the tree to the left pane
	gtk_container_add(GTK_CONTAINER(pScrolledTree),pTree);
    
    //add eventhandlers for the tree 
    GtkTreeSelection *pSelect = gtk_tree_view_get_selection (GTK_TREE_VIEW (pTree));
    gtk_tree_selection_set_mode (pSelect, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (pSelect), "changed", G_CALLBACK (treeSelectionChangedCB),NULL);
    
    //add scrolled panel for right panel
    scPhotoWall = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (scPhotoWall), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scPhotoWall),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC); //scroll vertical
    gtk_box_pack_start(GTK_BOX(pHBox), scPhotoWall, FALSE, FALSE, 0);
                                  
    //calculate the size of the photowall panel                              
    preferedWidth=screenWidth*0.6f; //60% of the screen for the photowall
    colMax=((float)preferedWidth -MARGIN)/(MARGIN+PHOTO_SIZE); //How many images by line?
    preferedWidth=colMax*(MARGIN+PHOTO_SIZE) + MARGIN; //We adjust the prefered size
    g_print("%i images/line\n",colMax);
    gtk_widget_set_size_request(scPhotoWall,preferedWidth,100); //the 100 has no effect, we only play on the width 
    
    //scrolling arrays needed to load the photos dynamically
    scWaitingImages = g_ptr_array_new_with_free_func (NULL);
    scWaitingNewThumbnails = g_ptr_array_new_with_free_func (NULL);
    scWaitingLabels = g_array_new (FALSE, FALSE, sizeof (gint));
    scAdjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(scPhotoWall));
    g_signal_connect(scAdjustment, "changed", G_CALLBACK(scrollChanged), NULL);  //settings are changing
    
    // vertical box 
    pVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous (GTK_BOX(pVBox), FALSE);
    gtk_container_add(GTK_CONTAINER(scPhotoWall), pVBox);
    gtk_widget_set_name(pVBox,"pVBox"); //bugfix for mac support, we set the background color because it doesn't work in photoWall
    
    // fixed container
    photoWall=gtk_fixed_new();
    gtk_box_pack_start(GTK_BOX(pVBox), photoWall, FALSE, FALSE, 0);
    gtk_widget_set_name(photoWall,"photoWall");

    //Create the photowall array to store some properties of the photo and widgets (see type photoObj ).
    //We define a big static photoarray with the space for all the photos of the photoWall.
    //As we load and unload photos in the photowall depending on the scroll, only the visible photos are in the array. Others are null elements.
    //photoArray is sized with photosRootDirCounter but is filled only by the visible photo.
    //for instance, if there are 12 images in a row, index 14, is the 3rd image in the 2nd line of the photowall 
    photoArray = g_ptr_array_new_with_free_func (g_free); 
    
    //event handler on the window for arrows keys
    //think the signal emission will reoccur at the key-repeat rate
    g_signal_connect(pWindow, "key-press-event", G_CALLBACK(keyPressCallBack), NULL);

    //create popupMenu
    createPopupMenu();
    createMenuTree();
    
    //show all
    gtk_widget_show_all(pWindow);

    //hide the searchBar
    gtk_widget_hide(searchBar);      
}

/*
The input parameters of the function give the location of the photo. 
The function loads the thumbnail if it exists in thumbnailDir and doesn't need to be refreshed
else return NULL.
We don't create the thumbnail in this function.
*/
static GtkWidget *loadThumbnail(const char *fullPathFile, int iDir){
    GtkWidget *pImage1; 
    //g_print("%s\n",pFileObj->name);
    //if (g_strcmp0(pFileObj->name, "IMG_2532.JPG")==0) g_print("debug IMG_2532-idnode%i\n", pFileObj->idNode);
    
    //check thumbnail to refresh (if file date != thumbnail date)
    gchar *fullPathThumbnail = getThumbnailPath (thumbnailDir,iDir,fullPathFile);
    int thumbnailTime=getFileTime(fullPathThumbnail);
    int fileTime=getFileTime(fullPathFile);

    if (thumbnailTime>-1 && fileTime !=thumbnailTime) {
        remove(fullPathThumbnail);
        g_free(fullPathThumbnail);
        return NULL;
    }

    //load thumbnail    
    GdkPixbuf *pBuffer1 = gdk_pixbuf_new_from_file_at_size(fullPathThumbnail,PHOTO_SIZE,PHOTO_SIZE,NULL);
    g_free(fullPathThumbnail);
    if (pBuffer1==NULL) {
        return NULL;
    }
    pImage1 = gtk_image_new_from_pixbuf (pBuffer1);
    gtk_widget_set_name(pImage1,"image1");
    g_object_unref(pBuffer1);
    return pImage1;
}

/*
Add the thumbnail image in the photoWall at the position specified in the parameters 
*/
static void insertPhotoWithImage(GtkWidget *image, int col, int row, int iDir, char *fullPath, long int time){
    GtkWidget *pEventBox, *pOverlay, *pFocusBox;
    GtkWidget *pLabel1, *pLabel2, *pLabel3, *pLabel4, *pEntry1;  

    //check if the photo is already registered in the photoArray so we need to do an update
    int _index=getPhotoIndex(col,row);
    if (_index!=-1 && g_ptr_array_index(photoArray,_index)!=NULL) {
        updatePhotoWithImage(image,col,row);
        return;
    }        
    
    int x=col * (PHOTO_SIZE + MARGIN) + MARGIN ;
    int y= row * (PHOTO_SIZE + MARGIN);//+MARGIN //pas de margin pour la 1ere row
    
    //add a component placeholder to the photoWall
    pOverlay = gtk_overlay_new ();    
    gtk_widget_set_name(pOverlay,"boxEvent");
    gtk_fixed_put(GTK_FIXED(photoWall),pOverlay, x, y);

    //pour permettre de gerer les events sur l'image on met un gtkeventbox devant l'image
    pEventBox=gtk_event_box_new();
    gtk_widget_set_can_focus (pEventBox, TRUE); 
    gtk_container_add (GTK_CONTAINER (pOverlay), pEventBox); //1er composant de l'overlay 
    
    //the image is NULL when the thumbnail is not already created in the thumbnailDir
    if (image!=NULL)  gtk_container_add (GTK_CONTAINER (pEventBox), image);
    else  gtk_widget_set_size_request(pEventBox,PHOTO_SIZE,PHOTO_SIZE);

    //extract the lastmoddate of the image no more used
    /*long int _time=time;
    char _date[20];
    strftime(_date, 20, "%Y/%m/%d %H:%M:%S", localtime(&_time)); //localtime create a new tm struc which represent a time  */     
    
    //add video icon if we display a video
    if (hasVideoExt(g_path_get_basename(fullPath))){
        gchar *fullPathVideoIcon =g_strdup_printf ("%s/%s",resDir,videoIconFile);
        GdkPixbuf *bufferVid = gdk_pixbuf_new_from_file_at_size(fullPathVideoIcon,PHOTO_SIZE,PHOTO_SIZE,NULL);
        GtkWidget *imgVid = gtk_image_new_from_pixbuf (bufferVid);
        g_free(fullPathVideoIcon);
        gtk_overlay_add_overlay (GTK_OVERLAY (pOverlay), imgVid);
        gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(pOverlay),imgVid,TRUE);        
    }
    
    //add a border component to render the focus of a photo
    pFocusBox=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_no_show_all (pFocusBox, TRUE); //set invisible
    GtkStyleContext *context2=gtk_widget_get_style_context (pFocusBox);
    #ifdef OSX
    gtk_style_context_add_class (context2,   "focusBoxOSX");
    #else
    gtk_style_context_add_class (context2,   "focusBox");
    #endif
    pLabel4=gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(pFocusBox), pLabel4, TRUE, TRUE, 0); 
    gtk_overlay_add_overlay (GTK_OVERLAY (pOverlay), pFocusBox);
    gtk_overlay_set_overlay_pass_through (GTK_OVERLAY(pOverlay),pFocusBox,TRUE);

    //event handler on the image 
    g_signal_connect (G_OBJECT (pEventBox),"button-press-event",G_CALLBACK (clickPhotoCB), NULL); //click on image   

    //register the new component in the array
    gchar *value = g_strdup_printf("photo %i,%i",col,row);
    PhotoObj *pPhotoObj=malloc(sizeof(PhotoObj));
    pPhotoObj->iDir=iDir;
    pPhotoObj->col=col;
    pPhotoObj->row=row;
    pPhotoObj->hasFocus=FALSE;
    pPhotoObj->selected=FALSE;
    pPhotoObj->deleted=FALSE;
    pPhotoObj->pFocusBox=pFocusBox;
    pPhotoObj->pEventBox=pEventBox;
    pPhotoObj->fullPath=fullPath; //no free TODO free à faire quand on supprimera le photoArray
    pPhotoObj->height=-1;
    pPhotoObj->width=-1; 
    pPhotoObj->lastModDate=time; 

    photoArray->pdata[_index]=pPhotoObj; //update the photoArray with the new data
    
    IntObj *arrayIndex=malloc(sizeof(IntObj));
    arrayIndex->value=_index;
    g_object_set_data (G_OBJECT(pEventBox), "arrayIndex", arrayIndex);     //set data for event handling

    //TODO il faudrait peut-être assigner les box à NULL pour permettre un free parfait des composants. ceci dit je crois que le removeall fait le nécessaire    
    
    //TODO il faudra penser au free de cette variable normalement pas sûr que ce soit fait avec le destroy    
}

/*
If the thumbnails is not ready we postpone the insertion of the image in the photowall.
We update it later with this function
*/
static void updatePhotoWithImage(GtkWidget *image, int col, int row){
    int index=getPhotoIndex(col, row);
    if  (index!=-1){ 
        PhotoObj *photoObj=g_ptr_array_index(photoArray,index);
        g_print("updatePhotoWithImage %i %i %i %p\n",col, row, index, photoObj);
        gtk_container_add (GTK_CONTAINER (photoObj->pEventBox), image);
    } else g_print("error in updating the photo (col %i,row %i)",col,row);
}

/*
click or doubleclick on pEventBox of Photo
*/
static gboolean  clickPhotoCB (GtkWidget *event_box, GdkEventButton *event, gpointer data)  {
    activeWindow=ORGANIZER;
    static int select=FALSE;
    static gint64 lastTime=0;
    static int lastIndex=-1;
    static int counter=0;
    counter++;
    IntObj *arrayIndex=g_object_get_data (G_OBJECT(event_box), "arrayIndex");
    g_print("click on photoindex %i",arrayIndex->value);
    PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,arrayIndex->value);
    if (event->type==GDK_2BUTTON_PRESS) {
        g_print("double master window");
        return TRUE; //the double click is already handled by the last click    
    }
    
    if (event->type == GDK_BUTTON_PRESS){        
        if (event->button == GDK_BUTTON_SECONDARY )  {
            //right click for pc
            #if defined(LINUX) || defined(WIN)
            g_print("clickrightorganizor"); // ctrl click for mac support
            gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, NULL, NULL, event->button, event->time);//gtk-3.18
            //gtk_menu_popup_at_pointer(GTK_MENU(pMenuPopup), NULL);//gtk-3.22
            if (pPhotoObj->selected){ 
                updateStatusMessageWithDetails(arrayIndex->value); //workaround to release op messages
                return FALSE;//if already selected don't run a changefocus
            } 
            #endif
        } else if (event->state & GDK_CONTROL_MASK){
            //ctrl left click
            #ifdef LINUX
            multiSelectPhoto(arrayIndex->value,CTRL);
            return FALSE; //we don't run the changefocus
            #elif OSX
            // ctrl click for mac support
            gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, NULL, NULL, event->button, event->time);  //gtk-3.18
            //gtk_menu_popup_at_pointer(GTK_MENU(pMenuPopup), NULL); //gtk-3.22
            return FALSE;
            #elif WIN
            //TODO
            #endif            
        }  
        #ifdef OSX
        else if (event->state == META_KEY){
            //apple click only OSX
            g_print("meta\n");
            multiSelectPhoto(arrayIndex->value,CTRL);
            return FALSE; //we don't run the changefocus
        } 
        #endif 
        else if (event->state & GDK_SHIFT_MASK){
            multiSelectPhoto(arrayIndex->value,SHIFT);
            changeFocus(arrayIndex->value,TOP,FALSE);
            return FALSE;
        }       
    }
    
    gint64 now=g_get_monotonic_time ();
    if (((now - lastTime) < DOUBLE_CLICK_TIME_DIF) && (arrayIndex->value==lastIndex)) {
        //char *machin=g_strdup_printf("-double clicked%i",counter);
        g_print ("%s",g_strdup_printf("-double clicked%i",counter));
        lastTime=now;
        lastIndex=arrayIndex->value;
        
        if (!pPhotoObj->hasFocus) changeFocus(arrayIndex->value, TOP,TRUE);   // a double click leave the photo focused
        
        GdkWindow *gdkWindow=gtk_widget_get_window(pWindow);
        GdkScreen *gdkScreen=gdk_window_get_screen(gdkWindow);
        int monitor=gdk_screen_get_monitor_at_window(gdkScreen,gdkWindow);
        multiViewerInit(GTK_WINDOW(pWindow), arrayIndex->value, monitor);   //double click callback : launch new screen with the photo 
        
        return TRUE; //if double click do nothing, action has already been managed in the single click and stop the propagation of the event
        
    }

    
    /* before, we've unfocus it was focused, I think needed it multiselection is on. Otherwise it is confusing
     if (!pPhotoObj->hasFocus) {
        changeFocus(pPhotoIndex->value,TOP);   
    }else {
        changeFocus(-1,TOP); //means hide all put hasFocus to FALSE   
    }*/
    changeFocus(arrayIndex->value,TOP,TRUE);   
    g_print ("-clicked%i",counter);
    lastTime=now;
    lastIndex=arrayIndex->value;
    return FALSE;
}

/*
 Press the keyboard on the photowall panel.
 The event is on the global window so we also handle search in the treeview.
 The treeview changes are handled in treeSelectionChangedCB
*/
static gboolean keyPressCallBack(GtkWidget *widget,  GdkEventKey *event) {
    activeWindow=ORGANIZER; //used in the popupmenu functions
    g_print("-key %s", gdk_keyval_name (event->keyval));
    if (gtk_widget_is_focus(searchEntry)) {
        if (event->keyval == GDK_KEY_Return) {
            const char *searchingText=gtk_entry_get_text(GTK_ENTRY(searchEntry));
            g_print("searching %s", searchingText);
            searchNodes(searchingText, NULL); //find all the nodes containg the text
            searchNext(); //select the first one
            if (searchRes->len==0) updateStatusMessage(g_strdup_printf("Nothing found for \"%s\"!",searchingText));
        }
        // handle specific ctrl F
        if ((event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F) &&
            ((event->state & GDK_CONTROL_MASK)||(event->state & GDK_META_MASK))) showSearchBarCB(searchBar,NULL);
        return FALSE; //to propagate the key
    } 
    if ((gtk_widget_is_focus(btnUp)||gtk_widget_is_focus(btnDown)) && (event->keyval == GDK_KEY_Return)) return FALSE;
    
    //if the focus is on the tree we don't handle keys, workaround for a bug on left and right key
    if (gtk_widget_is_focus (pTree)) g_print("focus on pTree");
    if (gtk_widget_is_focus (pTree) && event->keyval!=GDK_KEY_Left && event->keyval!=GDK_KEY_Right && event->keyval!=GDK_KEY_Return ) return FALSE; 
    //if (photoArray->len == 0 ) return FALSE;  //do nothing if no photos
    int newFocus=-1;
    static gint64 lastKey=0L; //for key down and up only to workaround the repeat key with the scrollingwindow
    gint64 now=0;
    GArray *_array;
    ScrollType sType;
    int _true;    
    switch (event->keyval) {
    case  GDK_KEY_Menu:
        g_print("-key menu");
        newFocus=whichPhotoHasTheFocus();
        if (newFocus!=-1){
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,newFocus); 
            gtk_menu_popup (GTK_MENU(pMenuPopup), NULL, NULL, (GtkMenuPositionFunc)setMenuPopupPosition, GTK_WIDGET(pPhotoObj->pFocusBox), NULL, event->time);//gtk-3.18
            //gtk_menu_popup_at_widget  (GTK_MENU(pMenuPopup), GTK_WIDGET(pPhotoObj->pFocusBox), GDK_GRAVITY_CENTER, GDK_GRAVITY_NORTH_WEST, NULL);//3.22
        }
        break;
    case GDK_KEY_Return: 
        if ( photoArray->len == 0 ) break;
        if (event->state & GDK_MOD1_MASK) {
            g_print("-alt return");
            showPropertiesDialog();
            //detailBoxEnabled=!detailBoxEnabled;
            //showHideDetailedBox();
            break;
        }
        g_print("-return");
        newFocus=whichPhotoHasTheFocus();
        GdkWindow *gdkWindow=gtk_widget_get_window(pWindow);
        GdkScreen *gdkScreen=gdk_window_get_screen(gdkWindow);
        int monitor=gdk_screen_get_monitor_at_window(gdkScreen,gdkWindow);
        if (newFocus!=-1) multiViewerInit(GTK_WINDOW(pWindow), newFocus, monitor);   //double click callback : launch new screen with the photo 
        break; //1er return pas très fiable puis je sais pas pourquoi ok
    case GDK_KEY_i:
    case GDK_KEY_I: //OSX shortcut
    	g_print("state %i GDK_META_MASK %i\n",event->state,GDK_META_MASK);
        if (event->state & GDK_META_MASK) {
            g_print("-alt return");
            showPropertiesDialog();
        }
        break;
    case GDK_KEY_Left:
        g_print("-left");
        if (event->state & GDK_CONTROL_MASK) {
            g_print("-ctrl left");
            gtk_widget_grab_focus(pTree); 
            return FALSE;
        }        
        newFocus=whichPhotoHasTheFocus();
        if (event->state & GDK_SHIFT_MASK){ //select left
            if (--newFocus>=0){
                multiSelectPhoto(newFocus,SHIFT);
                changeFocus(newFocus,TOP,FALSE);
            }
            return FALSE;
        }    
        if (--newFocus>=0) changeFocus(newFocus,TOP,TRUE); //we move
        else {gtk_widget_grab_focus(pTree); return FALSE; } //we go on treeview
        break;
    case GDK_KEY_Up: 
        g_print("-up");
        now=g_get_monotonic_time (); //in microsec
        if (((now - lastKey)/1000) < 2000)  g_print("%ld\n",(now-lastKey)/1000); //check the difference to debug        
        newFocus=whichPhotoHasTheFocus();
        if (newFocus==-1) {g_print("\nnewfocus==-1\n");break;}//workaround
        if (newFocus==-1 && photoArray->len>0 ) changeFocus(0,TOP,TRUE);
        else {
            int _row=getPreviousRow(newFocus);
            if (_row!=-1){
                newFocus=getPhotoIndex(getPhotoCol(newFocus),_row);
                if (newFocus>=0) {
                    if (event->state & GDK_SHIFT_MASK){ //select up
                        multiSelectPhoto(newFocus,SHIFT);
                        changeFocus(newFocus,TOP,FALSE);
                    } else {
                        if ((((now - lastKey)/1000) > 150)&& !_keyPostponed)
                            changeFocus(newFocus,TOP,TRUE); //move up directly
                        else {
                            if (!_keyPostponed){ //delay 150ms
                                _keyPostponed=TRUE;
                                _array=g_array_new(FALSE, FALSE, sizeof (gint));
                                g_array_append_val(_array,newFocus);
                                sType=TOP;
                                g_array_append_val(_array,sType);
                                _true=TRUE;
                                g_array_append_val(_array,_true);
                                int delay=150-((now - lastKey)/1000);
                                g_print("delaykeyUp%i\n",delay);
                                gdk_threads_add_timeout(delay,keyPostponed, _array);
                            }
                        }
                        lastKey=now;
                    }
                }
            } 
        }
        break;

    case GDK_KEY_Right: 
        g_print("-right");
        newFocus=whichPhotoHasTheFocus();   
        if (event->state & GDK_SHIFT_MASK){ //select right
            if (++newFocus<=(photoArray->len-1)){
                multiSelectPhoto(newFocus,SHIFT);
                changeFocus(newFocus,BOTTOM,FALSE);
            }
            return FALSE;
        }
        if (++newFocus<=(photoArray->len-1)) changeFocus(newFocus,BOTTOM,TRUE); //move
        else {gtk_widget_grab_focus(pTree); return FALSE;}//we go on treeview
        break;
    case GDK_KEY_Down:
        g_print("-down");
        now=g_get_monotonic_time (); //in microsec
        if (((now - lastKey)/1000) < 2000)  g_print("%ld\n",(now-lastKey)/1000); //check the difference to debug        
        newFocus=whichPhotoHasTheFocus();
        if (newFocus==-1) {g_print("\nnewfocus==-1\n");break;} //workaround
        if (newFocus==-1 && photoArray->len>0) changeFocus(0,TOP,TRUE);
        else {
            int _row=getNextRow(newFocus);
            if (_row!=-1){
                newFocus=getPhotoIndex(getPhotoCol(newFocus),_row);
                if (photoArray->len>0 && newFocus<=(photoArray->len-1)) {
                    if (event->state & GDK_SHIFT_MASK){ //select down
                            multiSelectPhoto(newFocus,SHIFT);
                            changeFocus(newFocus,BOTTOM,FALSE);
                    } else {
                        if ((((now - lastKey)/1000) > 150)&& !_keyPostponed)
                            changeFocus(newFocus,BOTTOM,TRUE); //move down directly
                        else {
                            if (!_keyPostponed){ //delay 150ms
                                _keyPostponed=TRUE;
                                _array=g_array_new(FALSE, FALSE, sizeof (gint));
                                g_array_append_val(_array,newFocus);
                                sType=BOTTOM;
                                g_array_append_val(_array,sType);
                                _true=TRUE;
                                g_array_append_val(_array,_true);
                                int delay=150-((now - lastKey)/1000);
                                g_print("delaykeyDown%i\n",delay);
                                gdk_threads_add_timeout(delay,keyPostponed, _array);
                            }
                        }
                        lastKey=now;
                    }
                }
            } 
        }
        break;
    case GDK_KEY_a: //select all
    case GDK_KEY_A:
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            g_print("-ctrl a");
            //not compliant with the lazy loading of the images
            // multiSelectPhoto(-1);
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
    case GDK_KEY_f:
    case GDK_KEY_F:
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            g_print("-ctrl f");
            showSearchBarCB(searchBar,NULL);
        }
        break; 
    case GDK_KEY_o:
    case GDK_KEY_O:
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {
            newFocus=whichPhotoHasTheFocus();
            GdkWindow *gdkWindow=gtk_widget_get_window(pWindow);
            GdkScreen *gdkScreen=gdk_window_get_screen(gdkWindow);
            int monitor=gdk_screen_get_monitor_at_window(gdkScreen, gdkWindow);
            if (newFocus!=-1) multiViewerInit(GTK_WINDOW(pWindow), newFocus, monitor);   //double click callback : launch new screen with the photo 
        }
        break;  
    case GDK_KEY_q:
    case GDK_KEY_Q: //let OSX quits the app
        if (event->state & GDK_CONTROL_MASK || event->state & GDK_META_MASK) {return FALSE;}
		break; 
    case GDK_KEY_BackSpace:
    case GDK_KEY_Delete: g_print("-delete");deleteDialog(); break;          
    case GDK_KEY_Home: g_print("-home");changeFocus(0,TOP,TRUE);break;
    case GDK_KEY_End: g_print("-end");changeFocus(photoArray->len-1,BOTTOM,TRUE); break;
    case GDK_KEY_Page_Up: g_print("-page_up");return FALSE;//pour laisser le scroll monter - event processed by 
    case GDK_KEY_Page_Down: g_print("-page_down");return FALSE;
    case GDK_KEY_F1: g_print("-F1"); helpCB(NULL,NULL,NULL); break;
    case GDK_KEY_F5: g_print("-F5"); refreshPhotoWall(NULL,NULL); break;
    }
    return TRUE; //to fix the focus switch from the tree to the photowall
    //TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
}

static void setMenuPopupPosition(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data){
    GtkWidget *focusBox = GTK_WIDGET(user_data);
    GdkWindow *win=gtk_widget_get_window (focusBox);
    gint _x=0,_y=0;
    //gdk_window_get_root_origin (win, &_x, &_y); //doesn't work 223,131
    //gdk_window_get_position (win, &_x, &_y); //doesn't work 96,282
    gdk_window_get_origin (win, &_x, &_y); //works
    g_print("\nposition focusbox %i %i\n",_x,_y);
    *x=_x+PHOTO_SIZE/2;
    *y=_y+PHOTO_SIZE/2;
    /*GtkAllocation rect;
    gtk_widget_get_allocation (focusBox,&rect); //doesn't work
    *x=rect.x;
    *y=rect.y;*/
}
/*
Used in the keyup or down in the photowall to delay the changefocus.
It is a workaroung to gain stability with the grabfocus gtk function.
*/
static int keyPostponed(gpointer user_data){
    GArray *_array=user_data;
    int index=g_array_index(_array,int,0);
    ScrollType sType=g_array_index(_array,ScrollType,1);
    int clearSelection=g_array_index(_array,int,2);
    changeFocus(index, sType, clearSelection);
    _keyPostponed=FALSE;
    return FALSE; //to stop repetition call

}

/*
window maximized/unmaximized callback
*/
static void windowMaximizeCallBack(GtkWidget* widget, GParamSpec* property,  gpointer data){
    //g_print("%s is changed, the new value is %i\n", property->name, gtk_window_is_maximized(GTK_WINDOW(widget)));
    int isMax=gtk_window_is_maximized(GTK_WINDOW(widget));
    if (isMax){
        g_print("maximized\n");
        int preferedWidth=screenWidth*0.75f; //75% de l'écran pour le photowall
        colMax=((float)preferedWidth -MARGIN)/(MARGIN+PHOTO_SIZE); //combien d'images par ligne
        preferedWidth=colMax*(MARGIN+PHOTO_SIZE) + MARGIN; //on ajust nickel la prefered size
        g_print("%i images/line\n",colMax);
        gtk_widget_set_size_request(scPhotoWall,preferedWidth,100); //the 100 has no effect, we only play on the width 
        int focus=whereIsTheFocus();
        refreshPhotoArray(FALSE);
        int focusRow=getPhotoRow(focus);
        scroll2Row(focusRow,TOP);
        focusIndexPending=focus;  //postponed the grab focus-> processed by the next run of scShowAllGtk
    } else {
        g_print("unmaximized\n");
        int preferedWidth=screenWidth*0.6f; //60% de l'écran pour le photowall
        colMax=((float)preferedWidth -MARGIN)/(MARGIN+PHOTO_SIZE); //combien d'images par ligne
        preferedWidth=colMax*(MARGIN+PHOTO_SIZE) + MARGIN; //on ajust nickel la prefered size
        g_print("%i images/line\n",colMax);
        gtk_widget_set_size_request(scPhotoWall, preferedWidth,100); //the 100 has no effect, we only play on the width 
        gtk_window_resize (GTK_WINDOW(pWindow), screenWidth*0.8f, getAdjustedHeight(screenHeight*0.86f));
        int focus=whereIsTheFocus();
        refreshPhotoArray(FALSE);
        int focusRow=getPhotoRow(focus);
        scroll2Row(focusRow,TOP);
        focusIndexPending=focus;  //postponed the grab focus-> processed by the next run of scShowAllGtk
    }
    
}

static void activateFocusCB(GtkWindow *window, gpointer   user_data){
    g_print("\nactivatefocus\n");
}

static void focusCB(GtkWindow *window, GtkWidget *widget, gpointer   user_data){
    g_print("\nfocusonWindowCB\n");
}
/*
    we run the changeFocusDelay to give robustness to refreshphotoarray function
    it is not good to drop and create many widgets and launch the grabFocus directly.
    Using a tempo give robustness
*/
int changeFocusDelay(gpointer user_data){
    g_print("\npassthrough changefocusdelay\n");
    GArray *_array=user_data;
    int index=g_array_index(_array,int,0);
    ScrollType type=g_array_index(_array,ScrollType,1);
    int clearSelection=g_array_index(_array,int,2);
    changeFocus(index, type, clearSelection);
    return FALSE; //to stop repetion call

}

/*
search for the focus in the photoArray
*/
static int whereIsTheFocus(void){
    if (photoArray==NULL) return -1;
    for (int i=0;i<photoArray->len;i++){
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i); 
        if (pPhotoObj!=NULL && pPhotoObj->hasFocus ) return i;
    }
}
/*
Set the focus to the element at index position,
change property hasFocus, show/hide the pFocusBox, reset all except the one in parameter and hide or show the visual component.
clearSelection is done when we reset the focus, except when we want to keep the selection for multiselection 
*/
void changeFocus(int index, ScrollType type, int clearSelection){
    if (index<0) g_print("error : index can't be negative in changefocus function");
    int length=photoArray->len;
    if (clearSelection) clearSelected(0,length -1); // clear all selected photos
    int previousFocus=-1;
    int newFocusFound=FALSE;
    g_print("changeFocus to photoArray index %i\n", index);
    for (int i=0;i<length;i++){
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i); 
        if (pPhotoObj!=NULL){
            //g_print("pPhotoObj%i",i);
            if (pPhotoObj->hasFocus) {
                g_print("clearfocus for index=%i",i);
                previousFocus=i;
                pPhotoObj->hasFocus=FALSE;
                if (clearSelection) {
                    pPhotoObj->selected=FALSE;
                    gtk_widget_hide(pPhotoObj->pFocusBox);
                }
            }
            if (i==index){
                //prevent the scroll to top impredicable error "see doc of grab_focus" don't run it just afterthe widget's creation
                while (gtk_events_pending ()) gtk_main_iteration(); //consume all the events before getting the focus
                if (!gtk_widget_get_mapped(pPhotoObj->pEventBox) || !gtk_widget_get_realized (pPhotoObj->pEventBox)) { 
                    g_print("not mapped to grab focus error index=%i, row=%i,mapped %i,realized %i",i,pPhotoObj->row,gtk_widget_get_mapped(pPhotoObj->pEventBox),gtk_widget_get_realized (pPhotoObj->pEventBox));
                    continue;
                }
                newFocusFound=TRUE;
                pPhotoObj->hasFocus=TRUE;
                pPhotoObj->selected=TRUE;
                gtk_widget_show(pPhotoObj->pFocusBox);
                
                /*scroll with changing the adjustment value*/
                if (!newFocusFromRefreshPhotoWall){ //exception when refreshphotowall() is pending
                    int position = gtk_adjustment_get_value (scAdjustment);
                    int topRow = position/(PHOTO_SIZE + MARGIN);
                    int bottomRow=topRow+scRowsInPage;// -1 + 1; (-1 because index start at 0 and +1 when half a thumbnail on top and bottom of the wall) = 0
                    if ( pPhotoObj->row<topRow || pPhotoObj->row>bottomRow){
                        g_print("changefocus_rowToScroll%i-top%i-bottom%i\n",pPhotoObj->row,topRow,bottomRow);
                        scroll2Row(pPhotoObj->row,type);
                    } else
                        g_print("changefocus_noscrollForrow%i-top%i-bottom%i\n",pPhotoObj->row,topRow,bottomRow); 
                    while (gtk_events_pending ()) gtk_main_iteration(); //consume all the events before getting the focus
                    gtk_widget_grab_focus(pPhotoObj->pEventBox);
                } else {newFocusFromRefreshPhotoWall=FALSE;}
                //select also the relative folder
                RowObj *rowObj=g_ptr_array_index(rowArray,pPhotoObj->row);
                //Bugfixing if the treeview selection is lost, reset one
                GtkTreeSelection *selection=gtk_tree_view_get_selection (GTK_TREE_VIEW(pTree));
                GtkTreeIter iter;
                GtkTreeModel *model;
                gboolean hasTreeSelection=gtk_tree_selection_get_selected (selection, &model, &iter);  
                if (!hasTreeSelection) g_print("Tree selection lost\n");
                if (rowObj->idNode!=treeIdNodeSelected || !hasTreeSelection) {
                    //first check if the folder treeIdNodeSelected is empty and idnode is a children
                    scSelectFolder(rowObj->idNode);
                }
                if (scSelectingFolderDisabled) scSelectingFolderDisabled=FALSE;  //set of this variable is done in the treeselection
                updateStatusMessageWithDetails(index);
                
            }   
        }
    }
    if (!newFocusFound){ //changeFocus has failed, reset the previous focus 
        g_print("error : new focus not found:%i -previous is%i",index, previousFocus);
        if (previousFocus==-1) { g_print("error : previous focus can't be found");return;}
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,previousFocus); 
        pPhotoObj->hasFocus=TRUE;
        pPhotoObj->selected=TRUE;
        gtk_widget_show(pPhotoObj->pFocusBox);
    }
} 

/*
change the scroll of the photowall to show one row
TOP & BOTTOM mean that the row must be on the top or the bottom of the page 
*/
void scroll2Row(int row, ScrollType type){
        int newPosition;
        if (type==TOP) newPosition=row * (PHOTO_SIZE+ MARGIN);
        if (type==BOTTOM) newPosition=row * (PHOTO_SIZE+ MARGIN)-(scPage-PHOTO_SIZE);
        gtk_adjustment_set_value (scAdjustment, newPosition); 
}
/*
Add or remove a selection in the photowall
(index ==-1) for select all photos
*/
static void multiSelectPhoto(int index, int mode){
    if (index<=-1) return;
    //select/unselect 1 photo CTRL mode    
    if (mode == CTRL && index>-1 && index<photoArray->len){    
        g_print("multiSelectPhoto in CTRL mode");
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,index); 
        if (pPhotoObj!=NULL){
            if (howManySelected()==1 && pPhotoObj->selected ){//to avoid to unselect the only select and have no more focus
                pPhotoObj->hasFocus=TRUE; //we force the focus if it comes from a previous multisel
                return;
            }
            GtkWidget *pFocusBox=pPhotoObj->pFocusBox;
            pPhotoObj->selected=!pPhotoObj->selected;
            if (pPhotoObj->selected){
                gtk_widget_show(pFocusBox);    
            } else {
                gtk_widget_hide(pFocusBox);
            }
        }
    }
    //select block of photos
    if (mode == SHIFT){
        int first=firstSelected();
        int last=lastSelected();
        if (index<first) {setSelected(index,first);}            
        else if (index>=first && index<last) {clearSelected(index+1, last);setSelected(first,index);}              
        else if (index>last) {setSelected(last,index);}              
        g_print("multiSelectPhoto in shift mode first=%i,last=%i,index=%i\n",first,last,index);
    }
    //select all the photos
 /*   if (index==-1){
        //TODO can be too big so limit to the current Dir
        for (int i=0;i<photoArray->len;i++){
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i); 
            if (pPhotoObj!=NULL ){
                pPhotoObj->selected=TRUE;
                gtk_widget_show(pPhotoObj->pFocusBox);
            }
        }
    }*/
}

/*
Return the index of the photo which has the focus
return -1 if no photo has the focus
*/
int whichPhotoHasTheFocus(void){
    for (int i=0;i<photoArray->len;i++){
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);    
        if (pPhotoObj!=NULL && pPhotoObj->hasFocus) {
            return i;
        }   
    }
    return -1;
}

/*
Return the row index of the photo which has the focus
*/
int whichRowHasTheFocus(void){
    for (int i=0;i<photoArray->len;i++){
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);    
        if (pPhotoObj!=NULL && pPhotoObj->hasFocus) {
            return pPhotoObj->row;
        }   
    }
    return -1;
}

/*
Update the message on the bottom-right with basics attributs of the found photo
*/
static void updateStatusMessageWithDetails(int index){
    g_print("updateStatusMessageWithDetails");
    static int opMessage=FALSE; //used to keep operation message otherwise it must be hidden by the property of the focused photo
    static gint64 lastCallTime=0;
    gint64 now =g_get_monotonic_time();
    int inter=(now-lastCallTime) / 1000000 ; //in s
    lastCallTime=now;
    if (inter<1 && opMessage) opMessage=FALSE; //if several requests in the delay, we check if we need to keep the operational message
    if (index <0) return;
    if (photoArray->len==0) return;
    const gchar *txt=gtk_label_get_text(GTK_LABEL(pStatusMessage));
    //exception - these messages are more important, they mustn't be hidden
    if (!opMessage && g_str_has_prefix(txt,"Loading completed")) {opMessage=TRUE;return;}
    if (!opMessage && g_str_has_prefix(txt,"Data has")) {opMessage=TRUE;return;}
    if (!opMessage && g_str_has_prefix(txt,"Folder ")) {opMessage=TRUE;return;}
    if (!opMessage && g_str_has_prefix(txt,"error ")) {opMessage=TRUE;return;}
    if (!opMessage && g_str_has_prefix(txt,"Please install ")) {opMessage=TRUE;return;}
    if (!opMessage && g_str_has_prefix(txt,"Please wait")) {opMessage=TRUE;return;}
    if (!opMessage && g_str_has_prefix(txt,"File's ")) {opMessage=TRUE;return;} //
    if (!opMessage && g_str_has_prefix(txt,"The photo directory has")) {opMessage=TRUE;return;} //
    if (!opMessage && (strstr(txt, "copied") != NULL)) {g_print("copy\n");opMessage=TRUE;return;} //for copy
    if (!opMessage && (strstr(txt, "moved") != NULL)) {g_print("move\n");opMessage=TRUE;return;} //for move
    PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,index);
    //retrieve filename
    gchar *filename;
    char *pos=g_strrstr (pPhotoObj->fullPath, "/");
    int p = pos ? pos - pPhotoObj->fullPath : -1;
    if (p!=-1){
        filename=g_strdup(++pos);
    }
    //retrieve date
    long int _time=pPhotoObj->lastModDate;
    char _date[20];
    strftime(_date, 20, "%Y/%m/%d %H:%M:%S", localtime(&_time)); //localtime create a new tm struc which represent a time
    //lazy loading width height
    char *status;
    if (hasPhotoExt(filename)){
        if (pPhotoObj->width==-1) getPhotoSize(pPhotoObj->fullPath,&pPhotoObj->width,&pPhotoObj->height);
        //status=g_strdup_printf("(index%i) %s  %ix%i  %s",index, filename, pPhotoObj->width, pPhotoObj->height, _date);
        status=g_strdup_printf("%s  %ix%i  %s", filename, pPhotoObj->width, pPhotoObj->height, _date);
    } else  {
        float size=getFileSize(pPhotoObj->fullPath);
        status=g_strdup_printf("%s  %.1fMB  %s",filename, size, _date);
    }
    updateStatusMessage(status);
    if (filename!=NULL) g_free(filename);
    g_free(status);
    opMessage=FALSE;
}

void updateStatusMessage(char *value){
    if (activeWindow == VIEWER) {updateStatusMessageViewer(value);return;}
    if (value==NULL || value[0]=='\0'){g_print("HideStatusMessageWithValue%s\n",value);gtk_widget_hide (pStatusMessage);return;} //hide the status message
    gtk_label_set_text(GTK_LABEL(pStatusMessage),value);
    gtk_widget_show(pStatusMessage);
}

/*
We show the date of the first photo of the current page to give an idea of the timeline to the user especialy when he is scrolling in a big directory
*/
void updateScrollingDate(char *value){
    if (value==NULL || value[0]=='\0'){gtk_widget_hide (pScrollingDate);return;} //hide the scrolling message
    gtk_label_set_text(GTK_LABEL(pScrollingDate),value);
    gtk_widget_show(pScrollingDate);
}

/* 
We take the data from photoArray.
But even we load/unload this array dynamically, this function works not only for visible photos.

*/
int getPhotoCol(int index){
    if (index > photoArray->len -1 || index <0) return -1;
    PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,index);
    if (pPhotoObj!=NULL) return pPhotoObj->col;
    //look in RowObj to get the calculated row and after the col
    RowObj *rowObj=NULL;
    for (int i=0;i<rowArray->len;i++){
        rowObj=g_ptr_array_index(rowArray,i);
        if (rowObj->index!=-1 && rowObj->indexInPhotoArray<=index && (rowObj->len -1 + (int)rowObj->indexInPhotoArray)>=index){
            //i is the row of the photo, rowObj->indexInPhotoArray is the first index in the row
            return index - rowObj->indexInPhotoArray;
        }            
    }
    g_print("error getPhotoCol index%i\n",index);
    return -1; //fails, should not happen
}

/* 
We take the data from photoArray.
But even if we load/unload this array dynamically, this function works not only for visible photos.
*/
int getPhotoRow(int index){
    if (index > photoArray->len -1 || index <0) return -1; 
    PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,index);
    if (pPhotoObj!=NULL) return pPhotoObj->row;
    //look in RowObj to get the calculated row
    RowObj *rowObj=NULL;
    for (int i=0;i<rowArray->len;i++){
        rowObj=g_ptr_array_index(rowArray,i);
        //chg colmax par rowObj->len
        if (rowObj->index!=-1 && rowObj->indexInPhotoArray<=index && (rowObj->len -1 + (int)rowObj->indexInPhotoArray)>=index)
            return i;
    }
    g_print("error getPhotoCol index%i\n",index);
    return -1; //fails should not happen
}

/*
 public function
 row must exist otherwise we return -1
 if col is too big we take the max col of the row 
*/
int getPhotoIndex(int col, int row){
    int _index=0; 
        RowObj *rowObj=NULL;
        for (int j=0;j<row;j++){
            rowObj=g_ptr_array_index(rowArray,j);
            if (rowObj->index>=0) _index=_index+rowObj->len;
        }
        rowObj=g_ptr_array_index(rowArray,row);
        if (rowObj==NULL) return -1;
        if (col > rowObj->len-1) _index=_index+rowObj->len-1; else _index=_index+col;
        if (_index>(photoArray->len-1)) _index=photoArray->len-1;
        return _index;
}

/*
Get the position of a photo in the photowall (x coordinate)
*/
static int getPhotoX(int index){
    return getPhotoCol(index) * (PHOTO_SIZE + MARGIN) + MARGIN ;
}

/*
Get the position of a photo in the photowall (y coordinate)
*/
static int getPhotoY(int index){
    return getPhotoRow(index) * (PHOTO_SIZE + MARGIN) + MARGIN;
}

/*
Get the previous row containing photos, index is a photoArray one.
The row must be loaded (widgets created in GTK).
If not it returns the photorow of the index
*/
static int getPreviousRow(int index){
    int photoRow=getPhotoRow(index);
    int row=photoRow-1;
    while (TRUE){
        if (row==-1) break; //out of range
        RowObj *rowObj =g_ptr_array_index(rowArray,row);
        if (rowObj->index>=0){
            //check if loaded
            return (rowObj->loaded==TRUE)?row:photoRow;
        }
        row--;
    }
    return photoRow;
}
/*
Get the next row containing photos, index is a photoArray one
The row must be loaded (widgets created in GTK)
*/
static int getNextRow(int index){
    int photoRow=getPhotoRow(index);
    int row=photoRow+1;
    while (TRUE){
        if (row==rowArray->len) break; //out of range
        RowObj *rowObj =g_ptr_array_index(rowArray,row);
        if (rowObj->index>=0){
            //check if loaded
            return (rowObj->loaded==TRUE)?row:photoRow;
        }
        row++;
    }
    return photoRow;
}

/*
Find the first row of the folder in the photoWall. It should be the title of the folder.
*/
static int getRowFromFolder(int idNode){
    int row=0;
    while (TRUE){
            if (row==rowArray->len) return -1; //out of range
            RowObj *rowObj =g_ptr_array_index(rowArray,row);
            //if (rowObj->idNode==idNode && rowObj->index>=0){  //1st version only image
            if (rowObj->idNode==idNode){
                //check if loaded
                return row;
            }
            row++;
        }
}

/*
Find the next folder idnode which is not empty.
We scan the rowarray. We update the row with the row of the first found photo.
*/
int getNextDir(int *row){
    int _row=*row;
    RowObj *rowObj =g_ptr_array_index(rowArray,_row);
    _row++;
    while (TRUE){
        if (_row==rowArray->len) return -1; //out of range
        RowObj *_rowObj =g_ptr_array_index(rowArray,_row);
        if (rowObj->idNode!=_rowObj->idNode && _rowObj->index!=-1){
            int newRow=_row;
            *row=newRow;
            return _rowObj->idNode;
        }
        _row++;
    }    
}

/*
Find the previous folder idnode which is not empty.
We scan the rowarray. We update the row with the row of the first found photo.
*/
int getPreviousDir(int *row){
    int _row=*row;
    RowObj *rowObj =g_ptr_array_index(rowArray,_row);
    _row--;
    while (TRUE){
        if (_row==-1) return -1; //out of range
        RowObj *_rowObj =g_ptr_array_index(rowArray,_row);
        if (rowObj->idNode!=_rowObj->idNode && _rowObj->index!=-1){
            int newRow=_row;
            *row=newRow;
            return _rowObj->idNode;
        }
        _row--;
    }    
}

/*
We adjust the height of the photowall to fit with the size of the thumbnails
*/
static int getAdjustedHeight(int height){
    int i=height/(PHOTO_SIZE + MARGIN);
    return i*(PHOTO_SIZE + MARGIN) - MARGIN;  //+MARGIN
}

/*
Scroll to the photo with changing the position of the Adjustment.
We also use the grabfocus to change the position of the scrolling in the photowall.
*/
static void scrollToPhoto(int index,ScrollType type){
    g_print("scrollToPhoto");
    GtkAdjustment *pAdjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(scPhotoWall));    
    int y=getPhotoY(index)-MARGIN; 
    int scrollPositionTop= gtk_adjustment_get_value (pAdjustment);
    int page=gtk_adjustment_get_page_size (pAdjustment);
    int scrollPositionBottom = scrollPositionTop + page;
    if (y>=scrollPositionTop && (y+PHOTO_SIZE)<=scrollPositionBottom) {}
    else {
        int value = 0;        
        if (type==TOP) value=y;
        if (type==BOTTOM) value=y-(page-PHOTO_SIZE-MARGIN);//-MARGIN 
        gtk_adjustment_set_value (pAdjustment, value);
        g_print("-adjustmentscroll to %i ",value);
    }
} 

/*
This function is used to reset or start the loading of the photowall.
It first reset the treemodel of folders. Then calculate the size of the photowall and the content of all the rows in it.
The scThread will manage the loading and unloading of the photos.
refreshTree TRUE to include the run of refreshTree
*/
void refreshPhotoArray(int _refreshTree){
    g_print("refreshPhotoArray launched\n");
    if (scThread!=NULL){
        //to finish the current activity of the scrolling thread. Exception is for the functions postponed by gdk_threads_add_idle() 
        //scShowAllGtk,scShowMessageDate, scShowMessageWait, scUpdateNewThumbnailInGtk
        scInterrupt=TRUE; 
    }
    G_LOCK(scMutex); //we lock the scrolling thread
    scCleanArrays(); //we clean tempo arrays used by the scrolling thread so the posponed function will end
    if (_refreshTree){
        treeIdNodeSelected=-1;treeDirSelected=NULL; //reset folder selection
        refreshTree();//reset treemodel, reset photosRootDirCounter and counters by node if dirs and files have changed in the file system
        loadDirectories2Monitor(); //refresh the monitoring
    }
    g_ptr_array_unref(photoArray);
    removeAllWidgets(GTK_CONTAINER(photoWall));
    photoArray = g_ptr_array_new_with_free_func (g_free);     //we recreate the photoArray
    //We define a big static photoarray with the space for all the photos of the photoWall.
    //to gain some memory we will free invisible photos depending on the scrolling position
    //so pay attention that g_ptr_array_index can be NULL
    g_ptr_array_set_size (photoArray,photosRootDirCounter); //every element is NULL at that time

    calculateRows(); //calculate the number of rows needed to place all the photos

    
    //calculate height of the photoWall scrolling pannel. The photoWall must have the space for all the photos of your picture directory.
    preferedHeight=(rowArray->len)*(PHOTO_SIZE+MARGIN) -MARGIN;
    gtk_widget_set_size_request(photoWall,preferedWidth,preferedHeight);
    g_print ("RowArray Length %i \n",rowArray->len);

    scPage=gtk_adjustment_get_page_size (scAdjustment);
    scRowsInPage=ceilf(((float)scPage+MARGIN)/(PHOTO_SIZE+MARGIN)); //we add 1 margin to numerator because last row has no bottom margin
    g_print("rowsinPage%i",scRowsInPage);
    scLastPosition=-(PHOTO_SIZE+MARGIN)/2; //to refresh the photowall at the Last position
    resetCurrentFilesInFolder(); //reset the current reading of files
    scShowRows(-1); //to reset static var lastShowRowTop
    //Start the thread which handles the filling and unfilling of the photowall (only when the gtk UI is loaded)
    if (scThread==NULL){
        if( (scThread = g_thread_new("scrolling thread",(GThreadFunc)scThreadStart, NULL)) == NULL)
             g_print("Thread create failed!!");
    } 
    updateStatusMessage(NULL); //set invisible
    gdk_threads_add_idle(scReleaseThread, NULL); //it will unlock the mutex
}

/*
Refresh the treeview data and select the folder idnode (exception if idNode==-1). 
The treeview contains all the sub directories of your picture directory. 
*/
static void refreshTree(){
        g_print("refreshTree\n");
        /*clear pStore */
        GtkTreeSelection *pSelect = gtk_tree_view_get_selection (GTK_TREE_VIEW (pTree));
        g_signal_handlers_disconnect_by_func(G_OBJECT (pSelect), G_CALLBACK (treeSelectionChangedCB),NULL);
        gtk_tree_store_clear(pStore);// before clearing we get rid of the the handling of changeevent

        loadTreeModel();
    
        g_signal_connect (G_OBJECT (pSelect), "changed", G_CALLBACK (treeSelectionChangedCB),NULL);
}

/*
Callback function called when the selection on the treeview changes.
We use a temporization to improve the repeat up or down key experience.
*/
static void treeSelectionChangedCB (GtkTreeSelection *selection, gpointer data){
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *name;    
    int counter;
    if (scSelectingFolder){ //we don't run the change event if triggered by scroll mechanism
        scSelectingFolder=FALSE;
        return;
    }
    //retrieve the content of the selection
    if (gtk_tree_selection_get_selected (selection, &model, &iter)){
            gtk_tree_model_get (model, &iter, BASE_NAME_COL, &name, ID_NODE, &treeIdNodeSelected, FULL_PATH_COL, &treeDirSelected, COUNTER, &counter, -1); //treeIdNodeSelected is global
            g_print ("You selected the folder %s -idNode%i - %s - %i \n", name, treeIdNodeSelected, treeDirSelected, counter);
            //attention ne pas relancer si timer function est active
            if (treeTimerFunctionOn) interruptLoading=TRUE; //to terminate the pending refresh
            if (!treeTimerFunctionOn) {
                treeTimerFunctionOn=TRUE;
                gdk_threads_add_timeout(300,treeNewSelectionCB, NULL);
            }
            g_free (name);
    }
}

/*
Manage a new selection in the treeview.
*/
static int treeNewSelectionCB (gpointer user_data){
    //timeIn();
    if (rowArray->len ==0) return -1; //no photos in photosrootdir
    static int lastSelection=-1;
    if (lastSelection == treeIdNodeSelected) {
        //Bug fixing we no more expand the treeview directly because we have side effect when we collapse a folder
        //expand the treeview to the first children to be more direct and give more visibility
        /*GtkTreePath *treePath=getTreePathFromidNode(treeIdNodeSelected,NULL);
        if (treePath) {
            gtk_tree_view_expand_row (GTK_TREE_VIEW(pTree), treePath, FALSE);
            hideSearchBtn(treePath); //disable search btn if the user click another folder
            gtk_tree_path_free(treePath);
        }*/
        
        if (!interruptLoading) {
            //scrollTo the right location in the photowall
            int row2Scroll=getRowFromFolder(treeIdNodeSelected);
            if (row2Scroll==-1) row2Scroll=getRowFromFolder(getNextIdNodeNotEmpty(treeIdNodeSelected,NULL));
            g_print("row2Scroll%i\n",row2Scroll);
            scroll2Row(row2Scroll,TOP);
            RowObj *rowObj=g_ptr_array_index(rowArray,row2Scroll+1);
            scSelectingFolderDisabled=TRUE;
            if (!rowObj->loaded==TRUE)
                focusIndexPending=getPhotoIndex(0,row2Scroll+1); //postponed the grab focus-> processed by the next run of scShowAllGtk
            else {
                changeFocus(getPhotoIndex(0,row2Scroll+1),TOP,TRUE);
            }
            treeTimerFunctionOn=FALSE;
            return FALSE; //to stop the timer
        } else {
            g_print(" interruptLoading ");
            interruptLoading = FALSE;
            lastSelection=treeIdNodeSelected;
            return TRUE; //we continue with the new selection which did the interruption
        } 
    } else {
        lastSelection=treeIdNodeSelected;
        g_print("new selection for folderid %i", treeIdNodeSelected );
        return TRUE; //to continue
    }
}

static gint sortIterCompareCB (GtkTreeModel *model, GtkTreeIter  *a, GtkTreeIter  *b, gpointer userdata)  {
    gint ret = 0;
    gchar *name1, *name2;
    gtk_tree_model_get(model, a, BASE_NAME_COL, &name1, -1);
    gtk_tree_model_get(model, b, BASE_NAME_COL, &name2, -1);

    if (name1 == NULL || name2 == NULL)  {
      if (name1 == NULL && name2 == NULL)  ret=0;
      else   ret = (name1 == NULL) ? -1 : 1;
    } else  {
      ret = g_utf8_collate(name1,name2);
    }
    g_free(name1);
    g_free(name2);
    return ret;
}

/*
Parent is the starting point of the search.
If parent is null, we start from the beginning of the model.
Be carefull, the function is recursive.
The return object must be freed by gtk_tree_path_free.
*/
GtkTreePath *getTreePathFromidNode(int idNode, GtkTreeIter *parent){
    GtkTreePath *ret=NULL;
    GtkTreeIter iter;
    int _idNode;
    int _counter=0;    
    gboolean found=gtk_tree_model_iter_children (pSortedModel, &iter, parent); 
    while (found){
        gtk_tree_model_get(pSortedModel, &iter, ID_NODE, &_idNode,-1);
        //gtk_tree_model_get(pSortedModel, &iter, BASE_NAME_COL, &_name, ID_NODE, &_idNode, FULL_PATH_COL, &_fullPathName,-1);
        //g_print("treepath%s",_name); for debug
        if (_idNode==idNode) {
            ret=gtk_tree_model_get_path(pSortedModel, &iter); 
            return ret; //get out of the loop
        } else if (gtk_tree_model_iter_has_child (pSortedModel, &iter)){
            ret = getTreePathFromidNode(idNode,&iter);
            if (ret!=NULL) return ret;
        }
        found = gtk_tree_model_iter_next(pSortedModel, &iter); 
    }
    return NULL;
}

/*
Get the next treeview element containing photos. idNode is the idNode of a folder in your picture directory.
If parent is null, we start from the beginning of the model.
*/
static int getNextIdNodeNotEmpty(int idNode, GtkTreeIter *parent){
     //we don't have to declare a pointer to this struct, it is not typedef struct like all the others gtk object
    int ret=-1;
    GtkTreeIter iter;
    int _idNode;
    int _counter=0;
    static int search;
    if (parent==NULL) search=FALSE;
    gboolean iter_valid=gtk_tree_model_iter_children (pSortedModel, &iter, parent); 
    while (iter_valid){
        gtk_tree_model_get(pSortedModel, &iter, ID_NODE, &_idNode, COUNTER, &_counter,-1);
        //g_print("treepath%s",_name);
        if (_idNode==idNode) search=TRUE;
        if (search && _counter!=0) return _idNode;            
        if (gtk_tree_model_iter_has_child (pSortedModel, &iter)){
                ret = getNextIdNodeNotEmpty(idNode,&iter);
                if (ret!=-1) return ret;
        }
        iter_valid = gtk_tree_model_iter_next(pSortedModel, &iter); 
    }
    return -1;
}

/*
Get the fullPath from a folder id. We seek it in the treeview's model.
If parent is null, we start from the beginning of the model.
*/
char *getFullPathFromidNode(int idNode, GtkTreeIter *parent){
     //we don't have to declare a pointer to this struct, it is not typedef struct like all the others gtk object
    char *ret=NULL;
    GtkTreeIter iter;
    gchar *_name;    
    int _idNode;
    int _counter=0;
    gchar *_fullPathName;
    //gboolean iter_valid = gtk_tree_model_get_iter_first(pSortedModel, iter); //GTK_TREE_MODEL(pStore)
    gboolean iter_valid=gtk_tree_model_iter_children (pSortedModel, &iter, parent); 
    while (iter_valid){
        gtk_tree_model_get(pSortedModel, &iter, BASE_NAME_COL, &_name, ID_NODE, &_idNode, FULL_PATH_COL, &_fullPathName,-1);//GTK_TREE_MODEL(pStore) , COUNTER,&_counter
        //g_print("treepath%s",_name);
        if (_idNode==idNode) {
            //g_print("loop OK %s",_name);
            //gtk_tree_store_set(pStore, &iter,BASE_NAME_COL, "lptlpt", ID_NODE, &_idNode, FULL_PATH_COL, "lptlpt", COUNTER, &_counter, -1); //for testing
            return _fullPathName; //get out of the loop
        } else if (gtk_tree_model_iter_has_child (pSortedModel, &iter)){
            ret = getFullPathFromidNode(idNode,&iter);
            if (ret!=NULL) return ret;
        }
        iter_valid = gtk_tree_model_iter_next(pSortedModel, &iter); //GTK_TREE_MODEL(pStore)
    }
    return NULL;
}

/*
This function fills the array searchRes with GtkTreePath containing the searchingText.
Run it with parent == NULL to start the search. This function is recursive.
*/
static void searchNodes(const char *searchingText, GtkTreeIter *parent){
    const char *translatedSearchingText;
    if (parent==NULL){ //init searchRes
        if (searchRes!=NULL) g_ptr_array_free(searchRes,FALSE);
        searchRes = g_ptr_array_new_with_free_func (NULL);
        searchIndex=-1;        
        translatedSearchingText=lower(g_str_to_ascii(searchingText,NULL)); 
    } else {
        translatedSearchingText=searchingText;
    }
    GtkTreePath *ret=NULL;
    GtkTreeIter iter;
    gchar *_name;    
    int _idNode;
    int _counter=0;
    gchar *_fullPathName;
    gboolean iter_valid=gtk_tree_model_iter_children (pSortedModel, &iter, parent); 
    while (iter_valid){
        gtk_tree_model_get(pSortedModel, &iter, BASE_NAME_COL, &_name, ID_NODE, &_idNode, FULL_PATH_COL, &_fullPathName,-1);//GTK_TREE_MODEL(pStore) , COUNTER,&_counter
        //g_print("treepath%s",_name);
        char *name=g_strdup(_name);
        name=lower(g_str_to_ascii(name,NULL));
        if (strstr(name, translatedSearchingText) != NULL) { //case insensitive comparison
            g_ptr_array_add(searchRes,gtk_tree_model_get_path(pSortedModel, &iter)); //add the found treePath to the searchRes
        } //before I got a else here 
        if (gtk_tree_model_iter_has_child (pSortedModel, &iter)){
            searchNodes(translatedSearchingText,&iter);
        }
        iter_valid = gtk_tree_model_iter_next(pSortedModel, &iter); //GTK_TREE_MODEL(pStore)
    }
}

/*
SearchNext will find the next element in searchRes array and select the node in the TreeView.
*/
static void searchNext(void){
    searchIndex++;
    if (searchRes->len>searchIndex){
        GtkTreePath *treePath=g_ptr_array_index(searchRes,searchIndex);
        //expand the treeview if necessary
        if (gtk_tree_path_get_depth (treePath) > 1) gtk_tree_view_expand_to_path (GTK_TREE_VIEW(pTree), treePath); 
        //select the node in the treeView
        GtkTreeSelection *selection =gtk_tree_view_get_selection (GTK_TREE_VIEW(pTree));
        gtk_tree_selection_select_path(selection,treePath);
        //scroll to the selected cell if not displayed
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW(pTree),treePath,NULL,TRUE,1,1); 
        showHideSearchBtn();
    }
    else {
        g_print("Next not found");    
        searchIndex--; //restore the index
    }
    
}

/*
SearchPrevious will find the previous element in searchRes and select the node in the TreeView
*/
static void searchPrevious(void){
    searchIndex--;
    if (searchIndex>=0){
        GtkTreePath *treePath=g_ptr_array_index(searchRes,searchIndex);
        //expand the treeview if necessary
        if (gtk_tree_path_get_depth (treePath) > 1) gtk_tree_view_expand_to_path (GTK_TREE_VIEW(pTree), treePath); 
        //select the node in the treeView
        GtkTreeSelection *selection =gtk_tree_view_get_selection (GTK_TREE_VIEW(pTree));
        gtk_tree_selection_select_path(selection,treePath);
        //scroll to the selected cell if not displayed
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW(pTree),treePath,NULL,TRUE,1,1);
        showHideSearchBtn();
    }
    else {
        g_print("previous not found");  
        searchIndex++; //restore the index
    }
}

static void nextCB(GtkWidget* widget, gpointer data){
    searchNext();
}

static void previousCB(GtkWidget* widget, gpointer data){
    searchPrevious();
}

/*
Enable or disable btnUp and btnDown depending on rearch results and on the last position.
*/
static void showHideSearchBtn(void){
    if (searchIndex>=(searchRes->len-1)) gtk_widget_set_sensitive(btnDown,FALSE); else gtk_widget_set_sensitive(btnDown,TRUE);
    if (searchIndex<=0) gtk_widget_set_sensitive(btnUp,FALSE); else gtk_widget_set_sensitive(btnUp,TRUE);
}

/*
Disable the search btn when the user selects a new directory in the tree.
In this context the user must run his search request again.
*/
static void hideSearchBtn(GtkTreePath *treePath){
    if (searchRes!=NULL){
        for (int i=0;i<searchRes->len;i++){
                    GtkTreePath *_treePath=g_ptr_array_index(searchRes,i);
            if (gtk_tree_path_compare(_treePath,treePath)==0) return;
        }
        gtk_widget_set_sensitive(btnUp,FALSE);
        gtk_widget_set_sensitive(btnDown,FALSE);
    }
}
/*
Callback function to show the searchbar.
*/
static void showSearchBarCB(GtkWidget* widget, gpointer data){
    if (!gtk_widget_is_visible(searchBar)){
        gtk_widget_show(searchBar);
        gtk_widget_grab_focus(searchEntry);
        gtk_widget_set_sensitive(btnUp,FALSE);
        gtk_widget_set_sensitive(btnDown,FALSE);
    }else {
        gtk_entry_set_text(GTK_ENTRY(searchEntry),"");
        gtk_widget_hide(searchBar);
    }
}

/*
Delete the selection flag & hide the selection box of a range of photos
*/
static void clearSelected(int first, int last){
    if (first<0) return;
    if (last>photoArray->len-1) return;
    for (int i=first;i<=last;i++){    
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
        if (pPhotoObj!=NULL && pPhotoObj->selected) {
            pPhotoObj->selected=FALSE;
            gtk_widget_hide(pPhotoObj->pFocusBox);
        }       
    }    
}

static void setSelected(int first, int last){
    if (first<0) return;
    if (last>photoArray->len-1) return;
    for (int i=first;i<=last;i++){    
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
        if (pPhotoObj!=NULL && !pPhotoObj->selected) {
            pPhotoObj->selected=TRUE;
            gtk_widget_show(pPhotoObj->pFocusBox);
        }       
    }    
}

/*
Are there selected photos?
*/
int howManySelected(void){
    int count=0;
    for (int i=0;i<photoArray->len;i++){    
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
        if (pPhotoObj!=NULL &&pPhotoObj->selected) count++;
    }    
    return count;
}

/*
check the content type of the selection (photos or video)
return NOTHING,ONLY_VIDEOS,ONLY_PHOTOS, MIX_PHOTOS_VIDEOS
*/
static int whatTypeIsSelected(void){
    int ret =NOTHING; //default value
    if (activeWindow == VIEWER) {
        if (hasVideoExt(viewedFullPath)) return ONLY_VIDEOS;
        else return ONLY_PHOTOS;
    } 
    if (activeWindow == ORGANIZER) { 
        for (int i=0;i<photoArray->len;i++){    
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
            if (pPhotoObj!=NULL &&pPhotoObj->selected) {
                if (hasVideoExt(pPhotoObj->fullPath)){  //it's a video
                    switch (ret) {
                        case NOTHING:
                            ret=ONLY_VIDEOS;
                            break;
                        case ONLY_VIDEOS:
                            break;
                        case ONLY_PHOTOS:
                            ret=MIX_PHOTOS_VIDEOS;
                            break;
                        case MIX_PHOTOS_VIDEOS:
                            break;
                    }
                } else{ //it's a photo
                    switch (ret) {
                        case NOTHING:
                            ret=ONLY_PHOTOS;
                            break;
                        case ONLY_VIDEOS:
                            ret=MIX_PHOTOS_VIDEOS;
                            break;
                        case ONLY_PHOTOS:                        
                            break;
                        case MIX_PHOTOS_VIDEOS:
                            break;
                    }
                }            
            }
        }
        return ret;
    }
}

/*
first selected photo found
*/
static int firstSelected(void){
    for (int i=0;i<photoArray->len;i++){    
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
        if (pPhotoObj!=NULL &&pPhotoObj->selected) return i;
    }    
    return -1;
}

/*
last selected photo found 
*/
static int lastSelected(void){
    int ret=-1;
    for (int i=0;i<photoArray->len;i++){    
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
        if (pPhotoObj!=NULL &&pPhotoObj->selected) ret=i;
    }    
    return ret;
}

/*
Get all the rows where we can find minimum one photo selected
the calling function must free the return GArray to avoid memleak
*/
static GArray *rowsSelected(void){
    GArray *rows = g_array_new(FALSE, FALSE, sizeof (gint));
    int rowAdded=-1;
    int row=-1;
    for (int i=0;i<photoArray->len;i++){
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
        if (pPhotoObj!=NULL &&pPhotoObj->selected) {
            row=getPhotoRow(i);
            if (row!=-1 && rowAdded!=row){
                g_array_append_val(rows,row);
                rowAdded=row;
            }
        }
    }
    return rows;
}


/*
Public function
Get the index of a photo in the photoArray if you give its path. 
*/
int findPhotoFromFullPath(char *fullPath){
    if (fullPath==NULL) return -1;
    for (int i=0;i<photoArray->len;i++){    
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
        if (pPhotoObj!=NULL && g_strcmp0(pPhotoObj->fullPath,fullPath) == 0) return i;       
    }
    char *_path=g_path_get_dirname(fullPath);
    char *_file=g_path_get_basename(fullPath);
    GPtrArray *_fileSet=getDirSortedByDate(_path);
    int  i=0;
    gboolean found=FALSE;
    while (i<_fileSet->len){
        FileObj *fileObj=g_ptr_array_index(_fileSet,i);
        if (g_strcmp0(fileObj->name,_file) == 0) {found=TRUE;break;}
        i++;
    }
    //i is the counter where the file is found in his dir
    if (!found) {g_ptr_array_unref(_fileSet);return -1;} //exit if not found
    int idFolder= getFileNode(_path);
    int j=0,index=0;
    while (j<rowArray->len){
        RowObj *rowObj=g_ptr_array_index(rowArray,j);
        if (rowObj->idNode==idFolder) break;
        if (rowObj->index>=0) index=index+rowObj->len;
        j++;
    }
    index=index+i;
    g_ptr_array_unref(_fileSet);
    return index;
}

/*
When UI has just finished to show the photoOrganizer window.
We use it to start the filling of the photoWall.
*/
static gboolean  windowMapCB (GtkWidget *widget, GdkEvent *event, gpointer data)  {
    g_print("photo organizer ready\n");
    focusIndexPending=0;  //postponed the grab focus-> processed by the next run of scShowAllGtk      
    refreshPhotoArray(FALSE); //init the filling of the PhotoWall
    int thumbnailsCounter=countFilesInDir(thumbnailDir,FALSE,TRUE);
    if (((float)thumbnailsCounter/photosRootDirCounter)<0.25) thumbnailDialogInit(GTK_WINDOW(pWindow));//show the thumbnail dialogbox for less 25% of thumbnails of the picture directory
    loadDirectories2Monitor();
    startMonitor(); //start the Monitor Thread
    gtk_widget_hide(GTK_WIDGET(data)); //hide the waiting screen
}


static void windowDestroyCB(GtkWidget *pWidget, gpointer pData){
    scStopThread=TRUE; //it will stop the thread
    stopMonitor();//it will stop the monitor
    g_print("organizer destroyed\n");
    gtk_window_close(GTK_WINDOW(_waitingScreen));
    g_application_quit(G_APPLICATION(app));
}
/*
Lists all the directories from the photosRootDir and fill the treestore.
We also use this function to count the numbers of files in the photosRootDir.
*/
int readRecursiveDir(const gchar *dirPath, GtkTreeIter *parent, int reset){
    static int counterTotal=0;
    if (reset) counterTotal=0;
    int counterByDir=0;
    DIR *directory = opendir(dirPath); //open the current dir
    struct dirent *pFileEntry;
    GtkTreeIter child;
    if(directory != NULL) {
      while((pFileEntry = readdir(directory)) != NULL)   {
        if (g_strcmp0(pFileEntry -> d_name,"..") == 0 || g_strcmp0(pFileEntry -> d_name,".") == 0) continue;
        if (pFileEntry->d_type==DT_DIR) {            
            //g_print(" %s %ld -", pFileEntry -> d_name , pFileEntry -> d_ino);
            gtk_tree_store_append (pStore, &child, parent); //create a new entry in the store
            int _counter=0;
            char *_dirPath=g_strdup_printf ("%s/%s",dirPath,pFileEntry -> d_name);
            //set the new entry with the folder name, path and idnode, the counter is preset to 0, it will be filled later
            gtk_tree_store_set (pStore, &child, BASE_NAME_COL,pFileEntry -> d_name, FULL_PATH_COL,_dirPath,ID_NODE,pFileEntry->d_ino,COUNTER,0,-1);           
            //recursivité
            readRecursiveDir(_dirPath, &child, FALSE); //the child becomes the parent of a lower level
            g_free (_dirPath);
        }  else {
            //only jpg and png and no hidden files for counting 
            if (!isHiddenFile(pFileEntry -> d_name) && (hasPhotoExt(pFileEntry -> d_name) || hasVideoExt(pFileEntry -> d_name))) { counterByDir++;  counterTotal++; }
        }
      }
      closedir(directory);
    }
    int idNode= getFileNode(dirPath);
    //g_print ("%s(%i)%i\n",dirPath,idNode,counterByDir);

    g_array_append_val(folderNodeArray,idNode);
    g_array_append_val(folderCounterArray,counterByDir);
    return counterTotal;
}

/* 
Recursive function, scan/update the Treeview model.
Parent=NULL to scan all the model to update the counters.
the 2 arrays are cleared at the end of the process
*/
void addCounters2Tree(GtkTreeIter *parent){
    //we don't have to declare a pointer to this struct, it is not typedef struct like all the others gtk object
    GtkTreeIter iter; //used in the sorted model
    GtkTreeIter store_iter; //used in the store model (what they call in the doc, the child model)
    gchar *_name;    
    int _idNode;
    int _counter=0;
    int newCounter=0;
    gchar *_fullPathName;
    GValue _newCounter = G_VALUE_INIT;
    g_value_init(&_newCounter,G_TYPE_INT);
    gboolean iter_valid=gtk_tree_model_iter_children (GTK_TREE_MODEL(pStore), &iter, parent); 
    while (iter_valid){
        gtk_tree_model_get(GTK_TREE_MODEL(pStore), &iter, ID_NODE, &_idNode,-1);
        newCounter=findCountersInArray(_idNode);
        g_value_set_int(&_newCounter, newCounter);
        gtk_tree_store_set_value (pStore,&iter,COUNTER,&_newCounter); //update the folder

        if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL(pStore), &iter)){
            addCounters2Tree(&iter);
        }
        iter_valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(pStore), &iter); //GTK_TREE_MODEL(pStore)
    }
    if (parent==NULL){
        g_print("addCounters2Tree finished\n");
        g_array_free (folderNodeArray,TRUE);
        g_array_free (folderCounterArray,TRUE);
    }
}

/*
The 2 arrays are complementary  : folderNodeArray contains all the sub directories of the photoroot and folderCounterArray the number of files contained in this dir. These 2 arrays are used in addCounters2Tree() and removed after.
*/
static int findCountersInArray(int idNode){
    int _idNode=-1;
    int ret=-1;
    int found =FALSE;
    int i;
    for (i=0;i<folderNodeArray->len;i++){
        _idNode = g_array_index (folderNodeArray, int, i);
        if (_idNode == idNode){
            found=TRUE;
            break;
        }
    }
    if (found) ret = g_array_index (folderCounterArray, int, i);
    return ret; 
}

static int counter4PhotoIndex=0; //used in calculateRowsEach
/*
The photowall is a scrolling window containing the space for all the photos of your picture directory.
This function calculate the rows needed to display all the photos.
And fill the array rowArray - number of rows = rowArray->len
*/
static void calculateRows(){
    counter4PhotoIndex=0;
    if (rowArray!=NULL) g_ptr_array_unref(rowArray);
    rowArray = g_ptr_array_new_with_free_func (g_free); 
    gtk_tree_model_foreach (pSortedModel,calculateRowsEach,NULL);
}

/*
We fill the rowArray with rowObj(s).
1 for name of the dir, 1 for each row in the photowall
*/
static gboolean calculateRowsEach (GtkTreeModel *model,GtkTreePath *path,GtkTreeIter *iter,gpointer data){
    int counter,idNode,rows;
    gtk_tree_model_get(model, iter, ID_NODE, &idNode, COUNTER, &counter,-1);
    int remainder=counter%colMax;
    if (remainder != 0)
        rows=counter/colMax +1;
    else
        rows=counter/colMax;
    if (counter==0) return FALSE;  // if the folder is empty we skip it from the photowall
    
    RowObj *rowTitle=malloc(sizeof(RowObj));
    rowTitle->idNode=idNode;
    rowTitle->index=-1; //we use -1 to know that the row doesn't contains photos but the title of the directory
    rowTitle->len=-1;
    rowTitle->hasMore=FALSE; //no meaning in this context
    rowTitle->loaded=FALSE;
    rowTitle->indexInPhotoArray=-1; //no meaning 
    g_ptr_array_add(rowArray,rowTitle);
    for (int i=0; i<rows; i++){
        RowObj *rowObj=malloc(sizeof(RowObj));
        rowObj->idNode=idNode;
        rowObj->index=i*colMax; //index in the directory of the first photo of the row
        rowObj->indexInPhotoArray=counter4PhotoIndex+rowObj->index;
        if (i==(rows-1)) {
            rowObj->len=(remainder==0)?colMax:remainder;
            rowObj->hasMore=FALSE; //hasmore in the directory?
        }else {
            rowObj->len=colMax;
            rowObj->hasMore=TRUE;
        }
        rowObj->loaded=FALSE;
        g_ptr_array_add(rowArray,rowObj);
    }
    counter4PhotoIndex+=counter; 
    return FALSE; //to continue the each call;
}

/*
Scroll configuration changed. For example the height of the panel to be scrolled.
*/
static void scrollChanged (GtkAdjustment *adjustment,  gpointer user_data){
        scPage=gtk_adjustment_get_page_size (scAdjustment);
        scRowsInPage=ceilf(((float)scPage+MARGIN)/(PHOTO_SIZE+MARGIN));   //we add 1 margin to numerator because last row has no bottom margin
}

/*
Photowall scrolling handler.
It checks every 25ms if the photowall scrolling has changed.
If a scroll happens, it refresh the visible part of the photowall

*/
static void scThreadStart(void *pointer){
     g_usleep(150000); //gtk pas completement ready - ça marche en décalant de 50ms le demarrage. On met 150ms pour ne pas prendre de risque
    //position is the position of the top of the viewport in the scrollpanel 
    int i=0; int j=0;
    int topRow=-1;                      
    scLastPosition=-(PHOTO_SIZE+MARGIN)/2; //46 is the row size/2 to display rows the first time 
    int position=-1;
    while (TRUE){
        g_usleep(25000); //temporization 25ms

        G_LOCK(scMutex); //we lock the scrolling thread

        position = gtk_adjustment_get_value (scAdjustment);
        if (focusIndexPending!=-1){
            scFocusIndexPending=focusIndexPending;
            focusIndexPending=-1;
        }
        //test range >46 to minimize the number of check
        if (abs(position-scLastPosition)>=((PHOTO_SIZE+MARGIN)/2) && !scWait){ //changes occured 46 is the row size/2
            if (scCheckMove(position)){ //check if it is a temporary, progressive or final scroll position
                g_print("calcul rows to check,%i,%i\n",scLastPosition,position);
                topRow = position/(PHOTO_SIZE + MARGIN);
                scShowRows(topRow); //load and show the needed thumbnails
                scLastPosition=position;  //set last value
                j++;
            }
        }
        
        G_UNLOCK(scMutex); //we lock the scrolling thread
        
        i++;
        if (i==1000000) i=0; //reinit after au dela du milion pour éviter les overflow
        if (scStopThread){ //stop the thread
            scStopThread=FALSE;
            scShowRows(-1); //reset static var lastShowRowTop
            return;
        }
    }
}

/*
Check if it scrolling behaviour is pending or completed.
*/
static int scCheckMove(int position){
    g_usleep(25000); //wait 25ms to see if the position is final or a progression 
    int newPosition = gtk_adjustment_get_value (scAdjustment);
    return (fabs(newPosition - position)<((PHOTO_SIZE+MARGIN)/2))?TRUE:FALSE;   //we can't check only equality because of the progressive very slow 2 fingers scroll
}

/*
Calculates the rows to show and inserts the thumbnails in the array scWaitingImages.
Directories titles are managed. These widgets will be attached to their GTK parents later in a GTK thread as required.
If thumbnails need to be created, their creation are postponed to avoid grabfocus errors. 
*/
static void scShowRows(int top){
    g_print("scShowRows called");
    static int lastShowRowTop=-1;
    int error=FALSE;
    if (top==-1) {lastShowRowTop=-1;g_print("scShowRows reset top=-1\n");return;} //reset the static var
    if (top==lastShowRowTop) return;
    if (rowArray->len==0)    { updateStatusMessage(g_strdup_printf("No Photos in %s !",photosRootDir)); return; }//no photos in photosrootdir
    //check already loaded
    //g_print("scShowRows%i\n", top  );
    int bottom=top+scRowsInPage;  
    int _top=(top<=1)?0:top-2; //+2 to get enough rows for the keyup gesture through grabfocus
    int _bottom=(bottom>=(int)(rowArray->len-3))?rowArray->len:bottom+2;//+2 to get enough rows for the keydown gesture through grabfocus
    g_print("bottom%i_bottom%i,rowArray->len%i\n",bottom,_bottom,rowArray->len);
    for (int i=_top; i<_bottom;i++){
        RowObj *rowObj=g_ptr_array_index(rowArray,i); 
        if (rowObj->loaded==FALSE){
            g_print("row to insert is %i, index is %i\n",i,rowObj->index);
            if (rowObj->index==-1)     {g_array_append_val(scWaitingLabels,i);
                                        g_print("insert row %i in waiting labels\n",i);}
            if (rowObj->index>=0) error=!insertImagesInRow(i,rowObj->idNode,rowObj->index,rowObj->hasMore);
            
            rowObj->loaded=WAITING;
            if (error || scInterrupt) break;
        }
    }
    g_print("inserts row in waiting array till row %i\n",_bottom);
    if (error) {
        g_print("error in insertImagesInRow(), the dir content has changed"); 
        return;
    }
    
    lastShowRowTop=top;
    if (scWaitingImages->len!=0 || scWaitingLabels->len!=0){
        scWait=TRUE;//prevent the scroll thread from playing
        gdk_threads_add_idle(scShowAllGtk,NULL);//Trigger the GTK widgets update in the gtk main thread
    }
    
    //show the date in the status bar when we scroll the photowall
    RowObj *rowObj=g_ptr_array_index(rowArray,lastShowRowTop);
    int rowIndexDate=(rowObj->index<0)?lastShowRowTop+1:lastShowRowTop;    
    gdk_threads_add_idle(scShowMessageDate,GINT_TO_POINTER(rowIndexDate)); //Trigger the scShowMessageDate in the gtk main thread

    //show Message Wait for new thumbnails
    if (scNewThumbnails2Create!=NULL && scNewThumbnails2Create->len>=12) { //1line of photos in basic screen
                gdk_threads_add_idle(scShowMessageWait,NULL);
    }
    //create the new thumbnails in this thread
    if (scNewThumbnails2Create!=NULL) scCreateNewThumbnails();
    //Trigger the scUpdateNewThumbnailInGtk in the gtk main thread
    gdk_threads_add_idle(scUpdateNewThumbnailInGtk,NULL);    
}

/*
Insert existing thumbnails in scWaitingImages to be added in Gtk later. New thumbnails will be added to scNewThumbnails2Create to be processed differently. Index is the index of the first photo of the row in its directory.
Return FALSE if error in loading
*/
static int insertImagesInRow(int row,int idNode,int index,int hasMore){
    int ret=TRUE;
    if (scNewThumbnails2Create==NULL) scNewThumbnails2Create = g_ptr_array_new_with_free_func (g_free);
    static int lastIdNode=-1;
    static char *fullPath;
    static GPtrArray *_fileSet=NULL;
    int col=0;
    //next call every static variable will be reset
    if (idNode==-1){
        lastIdNode=-1;
        if (_fileSet != NULL) {
            _fileSet=(GPtrArray *)(g_ptr_array_free(_fileSet,TRUE));
        }
        g_print("reset fileset\n");
        return ret;
    } 
    
    g_print("insert row %i in waiting images",row);
    //get the dir
    if (lastIdNode!=idNode){
        lastIdNode=idNode;
        fullPath=getFullPathFromidNode(idNode, NULL);
        if (_fileSet != NULL) _fileSet=(GPtrArray *)g_ptr_array_free(_fileSet,TRUE);        
        _fileSet=getDirSortedByDate(fullPath); 
    }
    if (_fileSet!=NULL){ //null means getDirSortedByDate crashed because the dir has changed
        for (int i=index;i<(index+colMax);i++){
            if (i>=(_fileSet->len)) break;
            FileObj *fileObj=g_ptr_array_index(_fileSet,i);
            gchar *fullPathFile= g_strdup_printf("%s/%s",fullPath,fileObj->name);
            GtkWidget *pImage = loadThumbnail(fullPathFile, idNode);
            int nullImage=FALSE;
            if (pImage==NULL) {nullImage=TRUE; pImage=gtk_label_new("");} //empty image, we use gtkLabel as a wrapper
            IntObj *_nullImage=malloc(sizeof(IntObj)); _nullImage->value=nullImage;
            g_object_set_data_full (G_OBJECT(pImage), "nullImage", _nullImage,(GDestroyNotify) g_free);
            IntObj *rowIndex=malloc(sizeof(IntObj)); rowIndex->value=row; 
            g_object_set_data_full (G_OBJECT(pImage), "row", rowIndex,(GDestroyNotify) g_free);
            IntObj *iDir=malloc(sizeof(IntObj)); iDir->value=idNode; 
            g_object_set_data_full (G_OBJECT(pImage), "iDir", iDir,(GDestroyNotify) g_free);
            LongIntObj *time=malloc(sizeof(LongIntObj)); time->value=fileObj->time; 
            g_object_set_data_full (G_OBJECT(pImage), "time", time,(GDestroyNotify) g_free);
            g_object_set_data_full (G_OBJECT (pImage),"fullPath",fullPathFile,(GDestroyNotify) g_free);
            g_ptr_array_add (scWaitingImages, pImage);
            if (nullImage) {
                //store the data to create the thumbnail later
                PhotoObj *newThumbnail=malloc(sizeof(PhotoObj));
                newThumbnail->row=row;
                newThumbnail->col=col;
                newThumbnail->iDir=idNode;
                newThumbnail->lastModDate=fileObj->time;
                newThumbnail->fullPath=g_strdup_printf("%s/%s",fullPath,fileObj->name);
                //other fields are useless for this process
                g_ptr_array_add (scNewThumbnails2Create, newThumbnail);
            }
            col++;
        }
        if (!hasMore) {
            if (_fileSet != NULL) _fileSet=(GPtrArray *)g_ptr_array_free(_fileSet,TRUE);
            lastIdNode=-1; //to clean the function
        }
        ret=TRUE;
    } else {
        //clear scNewThumbnails2Create, scWaitingImages
        if (scWaitingImages->len>0){
            g_ptr_array_free (scWaitingImages,FALSE); //don't remove the elements otherwise the previously loaded pImage will freed and can't be used by gtk
            scWaitingImages = g_ptr_array_new_with_free_func (NULL);
        }
        scNewThumbnails2Create=(GPtrArray *)g_ptr_array_free(scNewThumbnails2Create,TRUE);
        
        ret=FALSE; //means an error happens, so break scShowRows()
    }
    
    return ret;
}

//in insertImagesInRow, we use a statics variable which need to be reset when we refresh the photoarray
static void resetCurrentFilesInFolder(void){
    insertImagesInRow(0,-1,0,FALSE); //-1 will reset the static var_fileSet
}

/*
Attach the waiting widgets to the photowall at the correct place.
It removes also the widgets which have disappeared from the visible part of the photowall.
Runs in the gtk thread. 
*/
static int scShowAllGtk(gpointer user_data){
    g_print("scShowAllGtk called\n");
    //add image to rows
    int row=-1;
    int col=-1;
    int lastRowIndex=-1;
    if (scWaitingImages->len==0) g_print("no image in waiting images!\n");
    for (int i=0;i<scWaitingImages->len;i++){
        GtkWidget *pImage=g_ptr_array_index(scWaitingImages,i);    
        IntObj *_row=g_object_get_data (G_OBJECT(pImage), "row");
        row=_row->value;
        IntObj *_iDir=g_object_get_data (G_OBJECT(pImage), "iDir");
        int iDir=_iDir->value;
        LongIntObj *_time=g_object_get_data (G_OBJECT(pImage), "time");
        int time=_time->value;
        char *fullPath=g_object_get_data (G_OBJECT(pImage), "fullPath");
        if (row!=lastRowIndex) {
            col=0;lastRowIndex=row;g_print("insert images in gtk for row %i\n" ,row);
            RowObj *rowObj=g_ptr_array_index(rowArray,row);
            rowObj->loaded=TRUE;
        } else col++;
        IntObj *_nullImage=g_object_get_data (G_OBJECT(pImage), "nullImage");
        if (_nullImage->value) {//g_free(pImage);
                                pImage=NULL;}
        insertPhotoWithImage(pImage, col, row, iDir, fullPath, time);
    }
    //clean scWaitingImages
    if (scWaitingImages->len>0){
        g_ptr_array_free (scWaitingImages,FALSE); //don't remove the elements otherwise the previous pImage will be freed and can't be used by gtk
        scWaitingImages = g_ptr_array_new_with_free_func (NULL);
    }
    for (int j=0;j<scWaitingLabels->len;j++){
        row = g_array_index (scWaitingLabels, int, j);
        RowObj *rowObj=g_ptr_array_index(rowArray,row); 
        if (rowObj->index==-1) { //directory full name
            char *folder=getFullPathFromidNode(rowObj->idNode,NULL);
            GtkWidget *label=gtk_label_new(folder);
            int y= row * (PHOTO_SIZE + MARGIN)+PHOTO_SIZE/1.5;//+MARGIN //pas de margin pour la 1ere row
            GtkStyleContext *context=gtk_widget_get_style_context (label);
            gtk_style_context_add_class (context,   "labelInWall");
            gtk_fixed_put(GTK_FIXED(photoWall),label, PHOTO_SIZE/4 , y);
        }
        lastRowIndex=row;
    }
    //clean scWaitingLabels
    if (scWaitingLabels->len>0){
        g_array_free(scWaitingLabels, TRUE);
        scWaitingLabels = g_array_new (FALSE, FALSE, sizeof (gint));
    }
    
    //clean the photoArray and the photoWall
    int rowKeepFirst, rowKeepLast;//max 5 pages we don't care about limits. it's only for a test 
    rowKeepFirst=lastRowIndex-scRowsInPage*2.5;
    rowKeepLast=lastRowIndex+scRowsInPage*2.5;
    g_print("lastRowIndex %i\n",lastRowIndex);
    if (lastRowIndex!=-1){
        int k=0, w=-1;
        int focusRow=whichRowHasTheFocus();
        int focusRowPending=getPhotoRow(scFocusIndexPending);
        /*Exclude selection rows from cleaning*/
        GArray *_rowsSelected=rowsSelected();    
        while (TRUE){
            if (k > (photoArray->len-1) || photoArray->len == 0 ) break;
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray, k);
            //exclude focusRow and rows where we can find a selection
            //for focusRow we also exclude a range of 2up and 2 down to don't break with key up and down features 
            if (pPhotoObj!=NULL && (pPhotoObj->row < rowKeepFirst || pPhotoObj->row > rowKeepLast) && ( pPhotoObj->row < focusRow-2 || pPhotoObj->row > focusRow+2 ) && pPhotoObj->row!=focusRowPending && !isInIntegerArray(_rowsSelected,pPhotoObj->row) ){
                if (w!=pPhotoObj->row) g_print("clean row%i-",pPhotoObj->row);
                w=pPhotoObj->row;
                if (GTK_IS_WIDGET(pPhotoObj->pEventBox)){
                    GtkWidget *pOverlay=gtk_widget_get_parent(pPhotoObj->pEventBox);
                    removeAllWidgets(GTK_CONTAINER(pOverlay)); //remove the children of the overlay 
                    gtk_widget_destroy(pOverlay); //remove le container itself
                    if (pPhotoObj->col==0) { //unload the rowArray (only once by row, the first col) 
                        RowObj *rowObj=g_ptr_array_index(rowArray,pPhotoObj->row); 
                        rowObj->loaded=FALSE;
                    }
                }else{
                    //troubleshooting
                    g_print("error with eventBox k=%i\n",k);
                    g_print("photoarray len %i", photoArray->len);
                    break; 
                }
                //we don't remove the element from the photoArray but we free it and set it to NULL
                g_free(pPhotoObj);
                photoArray->pdata[k]=NULL;
                //TODO remove Labels dir et no photo???
            } else k++;
        }
        g_array_free(_rowsSelected,TRUE);
    }
    
    gtk_widget_show_all(photoWall); //set the new widget visibles
    
    if (scFocusIndexPending!=-1){
        g_print("changeFocus requested by scShowAllGtk\n");
        GArray *_array=g_array_new(FALSE, FALSE, sizeof (gint));
        g_array_append_val(_array,scFocusIndexPending);
        ScrollType _top=TOP;
        g_array_append_val(_array,_top);
        int _true=TRUE;
        g_array_append_val(_array,_true);
        //We run the changeFocusDelay to give robustness to refreshphotoarray function.
        //It is not good to drop and create many widgets and launch the grabfocus directly. Using a tempo gives robustness
        gdk_threads_add_timeout(50,changeFocusDelay, _array);
        scFocusIndexPending=-1;
    }
    return FALSE; //to stop the looped function
}

/*
If thumbnails don't exist or are out of date, we must build them on the fly
and we load the new thumbnails in the array scWaitingNewThumbnails. They will be attached to GTK container later
*/
static void scCreateNewThumbnails(void){
    GdkPixbuf *pBuffer1;
    for(int i=0;i<scNewThumbnails2Create->len;i++){
        if (scInterrupt) break;
        pBuffer1=NULL;
        PhotoObj *newThumbnail=g_ptr_array_index(scNewThumbnails2Create,i);
        #if defined(LINUX) || defined(WIN)
    	int _size=PHOTO_SIZE;
    	#else 
    	int _size=PHOTO_SIZE*2;
    	#endif
        int resCreate=createThumbnail(newThumbnail->fullPath,newThumbnail->iDir, thumbnailDir, _size, newThumbnail->lastModDate);
        
        gchar *fullPathThumbnail =getThumbnailPath (thumbnailDir,newThumbnail->iDir,newThumbnail->fullPath);
        
        if (resCreate == PASSED_CREATED){
            pBuffer1 = gdk_pixbuf_new_from_file_at_size(fullPathThumbnail,PHOTO_SIZE,PHOTO_SIZE,NULL);
            g_free(fullPathThumbnail);       
        } else {
            if (resCreate == ERR_FFMPEGTHUMBNAILER_DOESNT_EXIST) { 
                updateStatusMessage("Please install ffmpeg with \"sudo apt-get install ffmpeg\" to get video thumbnails.");
                //load ERROR image
                g_print("load thumbnail ERROR");
                gchar *fullPathNoVideo =g_strdup_printf ("%s/%s",resDir,noVideoPng);
                pBuffer1 = gdk_pixbuf_new_from_file_at_size(fullPathNoVideo,PHOTO_SIZE,PHOTO_SIZE,NULL);
                g_free(fullPathNoVideo);
            } else {
                //load ERROR image
                g_print("load thumbnail ERROR");
                gchar *fullPathNoImage =g_strdup_printf ("%s/%s",resDir,noImagePng);
                pBuffer1 = gdk_pixbuf_new_from_file_at_size(fullPathNoImage,PHOTO_SIZE,PHOTO_SIZE,NULL);
                g_free(fullPathNoImage);
                g_print("/fin/");
            }
        }
        if (pBuffer1){
            GtkWidget *pImage = gtk_image_new_from_pixbuf (pBuffer1);
            IntObj *rowIndex=malloc(sizeof(IntObj)); rowIndex->value=newThumbnail->row; 
            g_object_set_data_full (G_OBJECT(pImage), "row", rowIndex,(GDestroyNotify) g_free);
            IntObj *colIndex=malloc(sizeof(IntObj)); colIndex->value=newThumbnail->col; 
            g_object_set_data_full (G_OBJECT(pImage), "col", colIndex,(GDestroyNotify) g_free);
            g_ptr_array_add (scWaitingNewThumbnails, pImage);
            g_object_unref(pBuffer1);
        }
    }
    g_ptr_array_free(scNewThumbnails2Create,TRUE); scNewThumbnails2Create=NULL;
}

/*
Here we attach the newly created thumbnails to the photoWall. We must do that in the main gtk thread.
*/
static int scUpdateNewThumbnailInGtk(gpointer user_data){
    //update widget with some scWaitingNewThumbnails
    if (scWaitingNewThumbnails->len==0) g_print("no new Thumbnails waiting !\n"); else g_print("New Thumbnails are waiting !\n");
    int lastRowIndex=-1;
    for (int i=0;i<scWaitingNewThumbnails->len;i++){
            GtkWidget *pImage=g_ptr_array_index(scWaitingNewThumbnails,i);    
            IntObj *_row=g_object_get_data (G_OBJECT(pImage), "row");
            int row=_row->value;
            IntObj *_col=g_object_get_data (G_OBJECT(pImage), "col");
            int col=_col->value;
            updatePhotoWithImage(pImage, col, row);
            if (row!=lastRowIndex) { //update loaded
                lastRowIndex=row;
                RowObj *rowObj=g_ptr_array_index(rowArray,row);
                rowObj->loaded=TRUE;
            }
            //not working if (i%10==0) {gtk_widget_show_all(photoWall); while (gtk_events_pending ()) gtk_main_iteration(); } ////consume all the events before getting the focus 
    }
    //show the updates
    gtk_widget_show_all(photoWall);
    
    //free scWaitingNewThumbnails
    if (scWaitingNewThumbnails->len>0){
        g_print("finished scUpdateNewThumbnailInGtk %i\n",scWaitingNewThumbnails->len);
        g_ptr_array_free (scWaitingNewThumbnails,FALSE); //don't remove the elements otherwise the previous pImage will freed and can't be used by gtk
        scWaitingNewThumbnails = g_ptr_array_new_with_free_func (NULL);
    }
    
    //release Thread loop
    if (scInterrupt) scInterrupt=FALSE; //permit to the postponed func in the scrolling thread to play again
    scWait=FALSE;   //permit the scrolling loop thread to play again
    scHideMessageWait(NULL); //hide "waiting for thumbnails creation"
    return FALSE;
}

static int scRefreshPhotoWall(gpointer user_data){
    int k=-1;
    refreshPhotoWall(NULL,&k); //with -1 we don't need to reset the focus 
    return FALSE;
}
/*
Show the date in the status bar when we scroll the photowall
parameter is the row which we want to show the date
*/
static int scShowMessageDate(gpointer user_data){
    if (scInterrupt) return FALSE;
    int row=GPOINTER_TO_INT(user_data);
    //g_print("scShowMessageDate%i",row);
    int index=getPhotoIndex(0, row);
    PhotoObj *photo=g_ptr_array_index(photoArray,index);
    if (photo==NULL) return FALSE; //it can happen when we remove all the files of a directory
    char _date[20];
    strftime(_date, 20, "%Y/%m/%d %H:%M:%S", localtime(&(photo->lastModDate))); 
    updateScrollingDate(_date);
    return FALSE;
}

/*
Show to the UI that the process of creation thumbnails is pending.
*/
static int scShowMessageWait(gpointer user_data){
    if (scInterrupt) return FALSE;
    const gchar *txt=gtk_label_get_text(GTK_LABEL(pStatusMessage));
    g_print("scShowMessageWait%s\n",txt);
    scBeforeMessageWait=g_strdup(txt);
    updateStatusMessage("Please wait for the creation of thumbnails from images!");
    while (gtk_events_pending ()) gtk_main_iteration(); //consume all the events before doing the select
    return FALSE; //to stop the looped function
}

/*
Clear the message when the process is completed. 
*/
static int scHideMessageWait(gpointer user_data){
    const char *text=gtk_label_get_text(GTK_LABEL(pStatusMessage));
    if (g_strcmp0(text,"Please wait for the creation of thumbnails from images!") == 0) {updateStatusMessage(scBeforeMessageWait); g_print("scHideMessageWait\n");}
    return FALSE;
}

/*
unlock the mutex to release the scrolling thread loop
*/
static int scReleaseThread(gpointer user_data){
    scInterrupt=FALSE;
    G_UNLOCK(scMutex);
    g_print("G_UNLOCK(scMutex)\n");
    return FALSE;
}



/*
In the changefocus function we check if the directory of the focused photo has changed. In this case, we select the new directory in the treeview. 
*/
static void scSelectFolder(int idNode){
    while (gtk_events_pending ()) gtk_main_iteration(); //consume all the events before doing the select
    if (scSelectingFolderDisabled) return; //allows selection of empty folders
    g_print("scSelectFolder called");
    scSelectingFolder=TRUE;
    //select the idnode
    if (idNode!=-1){
        //GtkTreeIter iter;
        GtkTreePath *treePath=getTreePathFromidNode(idNode,NULL);//&iter);
        if (treePath) {
            g_print("treepath:%s",gtk_tree_path_to_string(treePath) );
            //expand the parent if needed 
            if (gtk_tree_path_get_depth (treePath) > 1) gtk_tree_view_expand_to_path (GTK_TREE_VIEW(pTree), treePath); 
            while (gtk_events_pending ()) gtk_main_iteration(); //consume all the events before doing the scroll
            //select the cell
            GtkTreeSelection *selection =gtk_tree_view_get_selection (GTK_TREE_VIEW(pTree));
            gtk_tree_selection_select_path(selection,treePath);
            //gtk_tree_view_set_cursor (GTK_TREE_VIEW(pTree),treePath,NULL,FALSE); //set cursor does a select and scroll but we have more control with scroll to cell
            //scroll to the selected cell if cell is not visible
            gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW(pTree),treePath,NULL,TRUE,1,1);             
            treeIdNodeSelected=idNode;
            treeDirSelected=getFullPathFromidNode(idNode, NULL); //update the treeDirSelected used in Diologs function
            gtk_tree_path_free(treePath); 
        }
    }
}

/*
We clean and remove the content of temporary scrolling arrays 
*/
static void scCleanArrays(void){
    
    if (scNewThumbnails2Create!=NULL && scNewThumbnails2Create->len>0) {
        g_ptr_array_free(scNewThumbnails2Create,TRUE);
        scNewThumbnails2Create=NULL;
    }
    //usually we don't remove the elements otherwise the previously loaded pImage will freed and can't be used by gtk but we use this function in an interruptions process so we need to free them
    if (scWaitingImages->len>0){
        g_ptr_array_set_free_func(scWaitingImages,(GDestroyNotify)gtk_widget_destroy);
        g_ptr_array_free (scWaitingImages,TRUE);     
        scWaitingImages = g_ptr_array_new_with_free_func (NULL);
    }
    if (scWaitingLabels->len>0){
        g_array_free(scWaitingLabels, TRUE);
        scWaitingLabels = g_array_new (FALSE, FALSE, sizeof (gint));
    }
    //usually we don't remove the elements otherwise the previously loaded pImage will freed and can't be used by gtk but we use this function in an interruptions process so we need to free them
    if (scWaitingNewThumbnails->len>0){
        g_ptr_array_set_free_func(scWaitingNewThumbnails,(GDestroyNotify)gtk_widget_destroy);
        g_ptr_array_free (scWaitingNewThumbnails,TRUE); //don't remove the elements otherwise the previous pImage will freed and can't be used by gtk
        scWaitingNewThumbnails = g_ptr_array_new_with_free_func (NULL);
    }


}

static void createMenuTree(void){
    pMenuTree = gtk_menu_new();
    GtkWidget *pMenuItem;

    pMenuItem = gtk_menu_item_new_with_label("Search Tree");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(showSearchBarCB),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuTree), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Refresh Tree");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(refreshPhotoWall),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuTree), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Rename Folder");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(renameFolderCB),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuTree), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Move Folder");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(moveFolderCB),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuTree), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Delete Folder");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(deleteFolderCB),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuTree), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("New Folder");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(newFolderCB),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuTree), pMenuItem);

    gtk_widget_show_all(pMenuTree);
    
    g_signal_connect (G_OBJECT (pTree),"button-press-event",G_CALLBACK (clickTreeCB), NULL);
    g_signal_connect (G_OBJECT (pTree),"test-expand-row",G_CALLBACK (expandCollapseTreeCB), NULL);
    g_signal_connect (G_OBJECT (pTree),"test-collapse-row",G_CALLBACK (expandCollapseTreeCB), NULL);

}


static gboolean clickTreeCB(GtkWidget *event_box, GdkEventButton *event, gpointer data)  {
    if (event->type == GDK_BUTTON_PRESS){        
        if (event->button == GDK_BUTTON_SECONDARY )  {//right click
            #if defined(LINUX) || defined(WIN)
            gtk_menu_popup (GTK_MENU(pMenuTree), NULL, NULL, NULL, NULL, event->button, event->time);//gtk-3.18
            //gtk_menu_popup_at_pointer(GTK_MENU(pMenuTree), NULL);//gtk-3.22
            #endif
        } else if (event->state & GDK_CONTROL_MASK){//ctrl left click for mac support
            #ifdef OSX
            gtk_menu_popup (GTK_MENU(pMenuTree), NULL, NULL, NULL, NULL, event->button, event->time); //gtk-3.18 
            //gtk_menu_popup_at_pointer(GTK_MENU(pMenuTree), NULL);//gtk-3.22

            #endif            
        } else {
            //scroll to the first photo of the folder if we re-click on a selected folder (not handled by treeSelectionChangedCB)
            //used to issue bug when we click expand or reduce icon but fixed with expandCollapseTreeCB
            expandCollapseTreePending=FALSE;
            GtkTreeSelection *selection=gtk_tree_view_get_selection (GTK_TREE_VIEW(pTree));
            GtkTreeIter iter;
            GtkTreeModel *model;
            int _treeIdNodeSelected;
            //retrieve the content of the selection
            if (gtk_tree_selection_get_selected (selection, &model, &iter)){
                    gtk_tree_model_get (model, &iter, ID_NODE, &_treeIdNodeSelected, -1); 
                    if (treeIdNodeSelected==_treeIdNodeSelected){
                        gdk_threads_add_timeout(400,reClickSameFolder, NULL);                        
                    }
            }
        }
    }
    return FALSE; // continue event handling
}

/*
check expand collapse context
*/
static gboolean expandCollapseTreeCB (GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data){
    g_print("expand-collapse");
    expandCollapseTreePending=TRUE;
    return FALSE;
}

/*
delayed to wait for expandCollapse event
*/
static int reClickSameFolder(gpointer user_data){
    if (!expandCollapseTreePending){
        expandCollapseTreePending=FALSE;
        g_print("reclick on the same folder");
        treeNewSelectionCB(NULL); //we force the selection to change
    }
    return FALSE; //stop repetition
}

void refreshPhotoWall(GtkWidget* widget, gpointer data){
    
    int lastFocus=whichPhotoHasTheFocus();
    char *fullPath=NULL;
    if (lastFocus!=-1){
        PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,lastFocus);
        fullPath=g_strdup(pPhotoObj->fullPath);
    }
    refreshPhotoArray(TRUE); //refresh tree and photowall
    if (data !=NULL){
        int *i= data;
        g_print("\n refreshData%i\n",*i);
        int idNode=*i;
        if (idNode!=-1){
            //GtkTreeIter iter;
            GtkTreePath *treePath=getTreePathFromidNode(idNode,NULL);//&iter);
            if (treePath) {
                g_print("treepath:%s",gtk_tree_path_to_string(treePath) );
                //expand the parent if needed 
                if (gtk_tree_path_get_depth (treePath) > 1) gtk_tree_view_expand_to_path (GTK_TREE_VIEW(pTree), treePath); 
                //select the cell
                GtkTreeSelection *selection =gtk_tree_view_get_selection (GTK_TREE_VIEW(pTree));
                gtk_tree_selection_select_path(selection,treePath);
                //gtk_tree_view_set_cursor (GTK_TREE_VIEW(pTree),treePath,NULL,FALSE); //set cursor does a select and scroll but more control with scroll to cell
                //scroll to the selected cell if not displayed
                gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW(pTree),treePath,NULL,TRUE,1,1); 
                if (treePath) gtk_tree_path_free(treePath); 
            }
        }
    } else {
        int newFocus=findPhotoFromFullPath(fullPath);
        if (newFocus!=-1){
            /*to be sure to have something in the new focus we force an empty create that will be refreshed later*/
            int col=getPhotoCol(newFocus);
            int row=getPhotoRow(newFocus);
            gchar *dirName=g_path_get_dirname (fullPath);
            int iDir=getFileNode(dirName);
            if (dirName) g_free(dirName);
            int time=getFileTime(fullPath);
            insertPhotoWithImage(NULL, col, row, iDir, fullPath, time);
            g_print("\nrefreshPhotoWall newfocus %i col %i,row%i\n", newFocus, col, row);
            focusIndexPending=newFocus;
            newFocusFromRefreshPhotoWall=TRUE;
        }
    }
    updateStatusMessage("Data has been refreshed!");
}

static void renameFolderCB(GtkWidget* widget, gpointer data){
    renameFolderDialog();
}
static void moveFolderCB(GtkWidget* widget, gpointer data){
    moveFolderDialog();
}
static void deleteFolderCB(GtkWidget* widget, gpointer data){
    deleteFolderDialog();
}
static void newFolderCB(GtkWidget* widget, gpointer data){
    newFolderDialog();
}


static void createPopupMenu(void){
    pMenuPopup = gtk_menu_new();
    GtkWidget *pSubMenu;
    GtkWidget *pMenuItem;
    GFile *pFile;
    GList *listApp, *l,  *listAppVideo;
    GAppInfo *appInfo;
    
    pMenuItem = gtk_menu_item_new_with_label("Refresh");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(refreshPhotoWall),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuPopup), pMenuItem);
    
    //get the app that can use mime type and create submenu with them
    pSubMenu = gtk_menu_new();
    
    #ifdef LINUX
    //LINUX uses mime types
    listApp=g_app_info_get_all_for_type("image/jpeg"); //photos
    listAppVideo=g_app_info_get_all_for_type( "video/mp4"); //videos
    listApp=g_list_concat(listApp,listAppVideo);
    //add open a new terminal with echo photoname and cd in the dir of the photo
    #ifndef FLATPAK
    appInfo = g_app_info_create_from_commandline("echo toto",
                                             "Terminal",
                                             G_APP_INFO_CREATE_NEEDS_TERMINAL,
                                             NULL);
    listApp=g_list_insert (listApp, appInfo, 0);
    #endif
    appInfo = g_app_info_create_from_commandline("Google Maps",
                                             "Google Maps",
                                             G_APP_INFO_CREATE_NEEDS_TERMINAL,
                                             NULL);
    listApp=g_list_insert (listApp, appInfo, 1);
    
    #elif OSX    
    //OSX uses UTI names
    listApp=g_app_info_get_all_for_type("public.jpeg"); // photos
    listAppVideo=g_app_info_get_all_for_type( "public.mpeg-4"); //videos
    listApp=g_list_concat(listApp,listAppVideo);
    
    char *appExe=g_strdup_printf("%s","Terminal");
    pMenuItem = gtk_menu_item_new_with_label(appExe);
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), pMenuItem);
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(openWithCB),appExe);
    
    appExe=g_strdup_printf("%s","Google Maps");
    pMenuItem = gtk_menu_item_new_with_label(appExe);
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), pMenuItem);
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(openWithCB),appExe);
        
    #elif WIN
    //TODO
    #endif
    
    for (l = listApp; l != NULL; l = l->next){
        appInfo = l->data;
        //g_print("%s",g_app_info_get_name(appInfo));
        pMenuItem = gtk_menu_item_new_with_label(g_app_info_get_name(appInfo));
        gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), pMenuItem);
        
        #ifdef LINUX
        g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(openWithCB),appInfo);
        #elif OSX
        g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(openWithCB),g_app_info_get_name(appInfo));
        #elif WIN
        //TODO
        #endif
    }

    pMenuItem = gtk_menu_item_new_with_label("Open With");  
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuPopup), pMenuItem);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubMenu);   
    
    pSubMenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Mail");
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), pMenuItem);
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(mailToCB),NULL);

    pMenuItem = gtk_menu_item_new_with_label("Web Whatsapp");
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), pMenuItem);
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(whatsappCB),NULL);

    pMenuItem = gtk_menu_item_new_with_label("Facebook");
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), pMenuItem);
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(facebookCB),NULL);

    pMenuItem = gtk_menu_item_new_with_label("Share");
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuPopup), pMenuItem);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubMenu);   
    
    pMenuItem = gtk_menu_item_new_with_label("Properties");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(showPropertiesDialog),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuPopup), pMenuItem);

    pSubMenu = gtk_menu_new();
    pMenuItem = gtk_menu_item_new_with_label("Change file's date with Exif's date");
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), pMenuItem);
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(changeFileDateWithExif),NULL);

    pMenuItem = gtk_menu_item_new_with_label("Entry a new file's date");
    gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), pMenuItem);
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(changeDateDialog),NULL); //from photoDialogs

    pMenuItem = gtk_menu_item_new_with_label("Date Operations");
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuPopup), pMenuItem);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(pMenuItem), pSubMenu);   
        
    pMenuItem = gtk_menu_item_new_with_label("Copy To");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(copyToDialog),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuPopup), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Move To");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(moveToDialog),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuPopup), pMenuItem);

    pMenuItem = gtk_menu_item_new_with_label("Delete");
    g_signal_connect(G_OBJECT(pMenuItem), "activate", G_CALLBACK(deleteMenu),NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(pMenuPopup), pMenuItem);

    gtk_widget_show_all(pMenuPopup);
}



static void openWithCB(GtkWidget* widget, gpointer data){
    
    g_print("openWithCB");
    #ifdef LINUX
    GAppInfo *appInfo=data;
    const char *appName=g_app_info_get_name(appInfo);     
    const char **mimeTypes=g_app_info_get_supported_types(appInfo);
    int i=0;
    int appType=PHOTO_APP; //default option Type
    while (mimeTypes && mimeTypes[i] )  {
        //is it a mp4 option?
        if (g_str_has_prefix(mimeTypes[i], "video/mp4"))  {
            appType = VIDEO_APP;
            break;
        }
        i++;
    } 
    #elif OSX
    char *appName=data;
    //TODO handle the difference between photo and video apps
    #elif WIN
    //TODO
    #endif
    
    GList *l=NULL;
    if (activeWindow == VIEWER) {
        GFile *pFile=g_file_new_for_path (viewedFullPath);
        l=g_list_append(NULL,pFile); 
    }
    if (activeWindow == ORGANIZER) {
        for (int i=0;i<photoArray->len;i++){    
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
            if (pPhotoObj!=NULL && pPhotoObj->selected) {
                GFile *pFile=g_file_new_for_path (pPhotoObj->fullPath);
                l=g_list_append(l,pFile);
            }
        }
    }
    if (g_strcmp0(appName,"Terminal") == 0) {
        //copy the filename in the clipboard
        GFile *pFile = g_list_first(l)->data;
        char *dirName=g_path_get_dirname(g_file_get_path(pFile));
        //concat all the file names.
        GString *fileList= g_string_new(NULL);
        while (l != NULL)   {
            pFile=l->data;
            g_string_append_printf(fileList," \"%s\"", g_file_get_basename(pFile));
            l = l->next;
        }
        //put the filename list in the clipboard, very usefull to launch cmd on the photos (ctrl shift v)
        GtkClipboard *clipBoard = gtk_clipboard_get (gdk_atom_intern ("CLIPBOARD",TRUE));
        gtk_clipboard_set_text (clipBoard, fileList->str, -1);
        //launch a terminal in the photo dir
        #ifdef LINUX
        //TODO can we check that gnome-terminal exists???
        char *cmd=g_strdup_printf("gnome-terminal --working-directory=\"%s\"",dirName);
        #elif OSX
        char *cmd=g_strdup_printf("open -a Terminal \"%s\"",dirName); //lance un terminal dans le current Dir 
        #elif WIN
        //TODO write the command
        #endif
        system(cmd);
        g_free(dirName);
        g_string_free(fileList, TRUE  );
    }   else if (g_strcmp0(appName,"Google Maps") == 0){
        g_print("google maps\n");
        char *_fullPath=NULL;
        if (activeWindow == VIEWER)         
            _fullPath=viewedFullPath;
        if (activeWindow == ORGANIZER) {
            int i=whichPhotoHasTheFocus();
            if (i!=-1) {
                PhotoObj *_pPhotoObj=g_ptr_array_index(photoArray,i); 
                _fullPath=_pPhotoObj->fullPath;
            }       
        } 
        if (hasVideoExt(_fullPath)){
            updateStatusMessage("just photos can run the google map option!");            
        } else {
            char *gps=getGPSData(_fullPath); //gps data are reformatted to fit to google maps
            if (gps!=NULL){ 
            #ifdef LINUX
                char *cmd = g_strdup_printf("xdg-open \"http://www.google.com/maps/place/%s\"",g_strescape(gps,"°")); 
            #elif OSX
                char *cmd =g_strdup_printf ("open \"http://www.google.com/maps/place/%s\"",g_strescape(gps,"°"));
            #elif WIN
                char *cmd =g_strdup_printf ("echo google maps not supported on windows.");
            #endif
                //g_print("\ncmd %s",cmd);
                g_spawn_command_line_async (cmd, NULL);
                g_free(cmd);
            } else {
                updateStatusMessage("No GPS data in the photo!");
            }        
        }
    }   else {
        #ifdef LINUX
        //check NOTHING,ONLY_VIDEOS,ONLY_PHOTOS, MIX_PHOTOS_VIDEOS before running the option
        int selectionType=whatTypeIsSelected();
        if ((selectionType==ONLY_PHOTOS && appType == PHOTO_APP) ||
            (selectionType==ONLY_VIDEOS && appType == VIDEO_APP)){
            g_app_info_launch (appInfo,l, NULL, NULL);

        } else
            if (appType==PHOTO_APP)
                updateStatusMessage("just photos can run these options!");
            else
                updateStatusMessage("just videos can run these options!");

        #elif OSX
        GFile *pFile = g_list_first(l)->data;
        GString *fileList= g_string_new(NULL);
        while (l != NULL)   {
            pFile=l->data;
            g_string_append_printf(fileList," \"%s\"", g_file_get_path(pFile));
            l = l->next;
        }
        char *cmd =g_strdup_printf ("open -a \"%s\" %s",appName, fileList->str); 
        g_spawn_command_line_async (cmd, NULL);
        g_free(cmd);

        #elif WIN

        #endif
    }
}
//Command_line_arguments_ compliant with geary or thunderbird in LINUX
//xdg-email --attach "/home/leon/Cloud Pictures/2003-08equihen/DSCN0282.JPG"
//in OSX 
//open -a Mail "/Users/leon/Pictures/BestOf 2016/20161118_135005.jpg"
static void mailToCB(GtkWidget* widget, gpointer data){
    g_print("mailToCB");
    gchar *cmd;
    GString *fileList=g_string_new(NULL);
    #ifdef LINUX
    gchar * prefix= "--attach ";
	#elif OSX  
    gchar * prefix= "";
    #endif
    if (activeWindow == VIEWER) { 
        g_string_append_printf(fileList," %s\"%s\"",prefix, viewedFullPath);
    }
    if (activeWindow == ORGANIZER) {
        for (int i=0;i<photoArray->len; i++){
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
            if (pPhotoObj!=NULL && pPhotoObj->selected) g_string_append_printf(fileList," %s\"%s\"",prefix,pPhotoObj->fullPath);
        }
    }
    if (fileList->str!=NULL){     //min 1 selected
    #ifdef LINUX
        //tested with geary and thunderbird
        cmd = g_strdup_printf("xdg-email %s",fileList->str); // xdg-email --attach \"%s\" 
    #elif OSX
        cmd =g_strdup_printf ("open -a Mail %s",fileList->str); 
    #elif WIN
        cmd =g_strdup_printf ("echo mailto no supported on windows.");
    //TODO
    #endif
        g_spawn_command_line_async (cmd, NULL);
        g_free(cmd);
        g_string_free(fileList,TRUE);
    }
}

/* 
Replace Update Date by exif date 
*/
static void changeFileDateWithExif(void){
    gchar *firstFullPath=NULL;
    GError *err = NULL;
    gchar *stdOut = NULL;
    gchar *stdErr = NULL;
    int status;
    GString *fileList= g_string_new(NULL);
    char *cmd=NULL;
    int count=0;
    if (activeWindow == VIEWER){
        g_string_append_printf(fileList," \"%s\"", viewedFullPath);
        count=1;
    }
    if (activeWindow == ORGANIZER){
        for (int i=0;i<photoArray->len; i++){
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
            if (pPhotoObj!=NULL && pPhotoObj->selected) {
                if (count==0) firstFullPath=g_strdup(pPhotoObj->fullPath); //for focusreset
                if (hasExifExt){  //only for jpeg
                    g_string_append_printf(fileList," \"%s\"",pPhotoObj->fullPath);
                    count++;
                }               
            }
        }
    }
    updateStatusMessage("Please wait, change date is pending...");
    while (gtk_events_pending ()) gtk_main_iteration(); //consume all the events 
    
    if (fileList->str!=NULL){     //min 1 selected
        cmd = g_strdup_printf("jhead -ft %s",fileList->str);
        g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err); //run jhead cmd
       if (err){
            g_print ("-error: %s\n", err->message);
            updateStatusMessage("Please install jhead with \"sudo apt-get install jhead\" to get this option.");
            g_clear_error(&err);
        } else if (stdErr[0]!='\0' && !strstr(stdErr,"Nonfatal")){ 
            updateStatusMessage(g_strdup_printf("error: %s.",stdErr));        
        } 
        else {//done
            if (activeWindow == VIEWER){
                GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(winViewer),GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_INFO, GTK_BUTTONS_OK,"%s", "File's date changed for the current file.");
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
                organizerNeed2BeRefreshed=TRUE; //refresh will occurs after leaving the viewer
            }
            if (activeWindow == ORGANIZER){
                int first=findPhotoFromFullPath(firstFullPath);
                focusIndexPending=first;  //postponed the grab focus-> processed by the next run of scShowAllGtk      
                removeThumbnails(NULL);//is needed to avoid the mess with idnode
                refreshPhotoArray(FALSE);
                updateStatusMessage(g_strdup_printf("File's date changed for %i file(s).",count));  
            }
        }
    }
    g_string_free(fileList,TRUE);
    g_free(cmd);
    //TODO WIN SPECIAL
}

static void deleteMenu(void){
    if (activeWindow==VIEWER) deleteInViewer();
    if (activeWindow==ORGANIZER) deleteDialog();
}

/*
Run web.whatsapp.com
copy in the clipboard the the photo fullpath
To send the current photo via whatsapp, when you attach the photo, use ctrl V to get the current photo
*/
static void whatsappCB(GtkWidget* widget, gpointer data){
    g_print("whatsappCB");
    gchar *cmd;
    char * fullPath=NULL;
    if (activeWindow == VIEWER) { 
        fullPath=viewedFullPath;
    }
    if (activeWindow == ORGANIZER) {
        for (int i=0;i<photoArray->len; i++){
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
            if (pPhotoObj!=NULL && pPhotoObj->selected) fullPath=pPhotoObj->fullPath;
        }
    }
    
    if (fullPath!=NULL){ 
    //put the fullPath in the clipboard, very usefull to attach the photo to a new message (ctrl V in the Upload Dialog)
    GtkClipboard *clipBoard = gtk_clipboard_get (gdk_atom_intern ("CLIPBOARD",TRUE));
    #ifdef OSX
    gtk_clipboard_set_text (clipBoard, g_path_get_basename(fullPath), -1);  //in OSX only the filename can be used in the search input
    #else
    gtk_clipboard_set_text (clipBoard, fullPath, -1);
    #endif
    #ifdef LINUX
        cmd = g_strdup_printf("xdg-open http://web.whatsapp.com"); 
    #elif OSX
        cmd =g_strdup_printf ("open http://web.whatsapp.com");
    #elif WIN
        cmd =g_strdup_printf ("echo whatsapp not supported on windows.");
    #endif
        g_spawn_command_line_async (cmd, NULL);
        g_free(cmd);
    }
}

/*
Run www.facebook.com
copy in the clipboard the the photo fullpath
To send the current photo via facebook, when you attach the photo, use ctrl V to get the current photo
*/
static void facebookCB(GtkWidget* widget, gpointer data){
    g_print("facebookCB");
    gchar *cmd;
    char * fullPath=NULL;
    if (activeWindow == VIEWER) { 
        fullPath=viewedFullPath;
    }
    if (activeWindow == ORGANIZER) {
        for (int i=0;i<photoArray->len; i++){
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i);
            if (pPhotoObj!=NULL && pPhotoObj->selected) fullPath=pPhotoObj->fullPath;
        }
    }
    
    if (fullPath!=NULL){ 
    //put the fullPath in the clipboard, very usefull to attach the photo to a new message (ctrl V in the Upload Dialog)
    GtkClipboard *clipBoard = gtk_clipboard_get (gdk_atom_intern ("CLIPBOARD",TRUE));
    #ifdef OSX
    gtk_clipboard_set_text (clipBoard, g_path_get_basename(fullPath), -1);  //in OSX only the filename can be used in the search input
    #else
    gtk_clipboard_set_text (clipBoard, fullPath, -1);
    #endif
	#ifdef LINUX
        cmd = g_strdup_printf("xdg-open http://www.facebook.com"); 
    #elif OSX
        cmd =g_strdup_printf ("open http://www.facebook.com");
    #elif WIN
        cmd =g_strdup_printf ("echo facebook not supported on windows.");
    #endif
        g_spawn_command_line_async (cmd, NULL);
        g_free(cmd);
    }
}
