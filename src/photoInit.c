/*
photoInit.c
read input variables
read/ask settings (ask the first run)
show waiting screen
run the icon creation of all the photos (asked when not enough thumbnails are created)
start the photoOrganizer
*/
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <math.h>
#include <gtk/gtk.h>
#include <tsoft.h>
#include <photoInit.h>
#include <photoOrganizer.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <thumbnail.h>
#include <fileSystemMonitor.h>



enum {RIGHT, LEFT};

void onDestroy(GtkWidget *pWidget, gpointer pData);
static gboolean fillProgressBar (gpointer   user_data);
static gboolean  buttonPressCB (GtkWidget *event_box, GdkEventButton *event, gpointer data);
static void *_loadTreeModel(void *pointer);
static void startThread(void);
static gboolean  windowMapCallBack (GtkWidget *widget, GdkEvent *event, gpointer data) ;
static int startOrganizer (gpointer user_data);
static int processInitFile(void);
static gboolean btnValidateCB (GtkWidget *event_box, GdkEventButton *event, gpointer data);
static void initFileChooser(gboolean chgRootDir);
static void initProgressBar(void);
static void savePhotosRootDirToIniFile(void);
static void appActivated (GtkApplication *app, gpointer user_data) ; //used for menu
static void appStarted (GtkApplication *app, gpointer user_data);
static void appOpened (GApplication *application, GFile **files, gint n_files, const gchar *hint);
static void menuButtonClickCB (GtkButton *button, gpointer data);


//public variable
char *thumbnailDir; //initialized in the main
char *appDir, *resDir; //init in the main
int photosRootDirCounter=0; //number of files of the root of your photo library
GtkApplication *app;


//private global
static char *iniFileName;//init in the main
static GtkWidget *pVBox;
static GtkWidget *pProgressBar, *pLabelStatus;
static int cancelProgress=FALSE;
static GThread  *pThread; 
static GtkWidget *pWindow;
static GtkWidget *fileChooser;
static int dirInCmd =FALSE; //did the root photo dir initialized in the command ?
static GtkWidget *popover=NULL;


int main(int argc, char *argv[]){
    
    if (argc>1) {
        char *help="You can start the app with no argument. The UI will ask you where is the root of your photos. \nIf you want to view a specific folder, just add it to the command as an argument.\nexample: pwall ~/my\\ photos, pwall \"/home/peter/Pictures/2016 summer\"\n";
        if (g_strcmp0(argv[1],"--help")==0) {g_print("%s",help);return EXIT_SUCCESS;}
        if (g_strcmp0(argv[1],"-h")==0) {g_print("%s",help);return EXIT_SUCCESS;}
        //check if it is a directory
        DIR *directory = opendir(argv[1]); 
        if ( directory!=NULL) {
            dirInCmd=TRUE;
            photosRootDir=argv[1]; //set the public variable
            //remove trailing / if needed
            if (g_str_has_suffix (photosRootDir, "/")) {
                int l=(int)strlen(photosRootDir) -1;
                photosRootDir=g_strndup (photosRootDir,l);
            }
            closedir(directory);
        } else {
            g_print("The argument of the command should be a valid and accessible folder %s . Please check the folder or check the syntaxe with pwall --help .\n",argv[1]);
            return EXIT_FAILURE;
        }
    }

    //init app dir variables    
    appDir =g_strdup_printf ("%s/%s",g_get_home_dir(),".pwall");
    thumbnailDir =g_strdup_printf ("%s/%s",appDir,"mini");
    
    //check dir exists
    struct stat st = {0};
    if (stat(appDir, &st) == -1) {
        mkdir(appDir, 0775); // read, no write for others
    }
    if (stat(thumbnailDir, &st) == -1) {
        mkdir(thumbnailDir, 0775);
    }
    
    // load preferences from .ini file
    iniFileName="pwall.ini";
    int ret = processInitFile();
    if (!ret) return EXIT_FAILURE;
        
    //res - where the resources are installed (icons, help ...)
    //For example, on linux, it is /usr/share/pwall
    const char *APP_RES = g_getenv ("PWALL_RES");  //for OSX support
	if (APP_RES == NULL) APP_RES="/usr/share"; //example /usr/share/pwall	
	resDir = g_strdup_printf("%s/%s",APP_RES,"pwall"); 
    #ifdef FLATPAK
    //resDir="/app/extra/export/share";
    resDir="/app/share";
    #endif 	
    g_print("new application - resources in %s\n",resDir);
    int status;
    //char *locale = setlocale (LC_ALL, ""); //check locale for glib sorting : issue in flatpak runtime Gtk-WARNING **: Locale not supported by C library
    //g_print("local%s",locale);
    app = gtk_application_new ("com.tsoft.pwall", G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN);//G_APPLICATION_HANDLES_OPEN to get the command line
    g_signal_connect (app, "activate", G_CALLBACK (appActivated), NULL);
    g_signal_connect (app, "startup", G_CALLBACK (appStarted), NULL);
    g_signal_connect (app, "open", G_CALLBACK (appOpened), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);
	
	
	return status; 
}

/*
gapplication support with command line
we pass through this function if there is parameters in the command line
*/
void appOpened (GApplication *application, GFile **files, gint n_files, const gchar *hint){
    g_print("app opened");
    appActivated(app,NULL);
}

static void quitCB (GSimpleAction *action, GVariant *parameter, gpointer user_data){
    //scStopThread=TRUE; //it will stop the thread but not in the scope
    g_application_quit(G_APPLICATION(app));
}

static void refreshThumbnailsCB (GSimpleAction *action, GVariant *parameter, gpointer user_data){
     if (pWindowOrganizer !=NULL) thumbnailDialogInit(GTK_WINDOW(pWindowOrganizer));
}

static void chgPhotoDirCB (GSimpleAction *action, GVariant *parameter, gpointer user_data){
    initFileChooser(TRUE);
}

void helpCB (GSimpleAction *action, GVariant *parameter, gpointer user_data){    
    #ifdef OSX
        char *helpFile="helposx.html";
    #else
        char *helpFile="help.html";
    #endif

    const char *helpFilePath =g_strdup_printf ("file://%s/%s", resDir, helpFile);
    GError *err = NULL;
    gtk_show_uri_on_window (GTK_WINDOW(pWindow), helpFilePath, GDK_CURRENT_TIME, &err); //compliant with flatpak
    if (err){ 
        g_print ("-error: %s\n", err->message);
    }    
}

const GActionEntry app_actions[] = {
    { "refreshThumbnails", refreshThumbnailsCB },{ "chgPhotoDir", chgPhotoDirCB },{ "help", helpCB }, { "quit", quitCB }
};

/*
gnome 3.34 and + support for menus , we settle it in the title menu
*/
void initPrimaryMenu(GtkWidget *menuButton){
    GMenu *menu;
    g_action_map_add_action_entries (G_ACTION_MAP (app), app_actions, G_N_ELEMENTS (app_actions), app);
    menu = g_menu_new ();
    g_menu_append (menu,"Refresh Thumbnails","app.refreshThumbnails");
    g_menu_append (menu,"Change photos directory","app.chgPhotoDir");
    g_menu_append (menu, "Help", "app.help");
    g_menu_append (menu, "Quit", "app.quit");
    popover=gtk_popover_new_from_model (menuButton, G_MENU_MODEL (menu));
    g_signal_connect (G_OBJECT (menuButton),"button-press-event",G_CALLBACK (menuButtonClickCB), NULL); //click
    g_signal_connect (G_OBJECT (menuButton),"key-press-event",G_CALLBACK (menuButtonClickCB), NULL); //click
    g_object_unref (menu);
}

static void menuButtonClickCB (GtkButton *button, gpointer data){
    gtk_widget_show(popover); 
    //gtk_popover_popup(GTK_POPOVER(popover)); //not supported in ubuntu 16.04
}
/*
The place where we create the main menu
*/
static void appStarted (GtkApplication *app, gpointer user_data) {
    g_print("app started\n");
    //     gnome 3.34+ don't support any more gtk_application_set_app_menu to put menu in the primarymenu//
   /* GMenu *menu;
    g_action_map_add_action_entries (G_ACTION_MAP (app), app_actions, G_N_ELEMENTS (app_actions), app);
    menu = g_menu_new ();
    g_menu_append (menu,"Refresh Thumbnails","app.refreshThumbnails");
    g_menu_append (menu,"Change photos directory","app.chgPhotoDir");
    g_menu_append (menu, "Help", "app.help");
    g_menu_append (menu, "Quit", "app.quit");
    gtk_application_set_app_menu (GTK_APPLICATION (app),G_MENU_MODEL (menu));
    g_object_unref (menu);*/
}

static void appActivated (GtkApplication *app, gpointer user_data) {
    g_print("app activated\n");
    pWindow = gtk_application_window_new(app);
	gtk_window_set_position(GTK_WINDOW(pWindow), GTK_WIN_POS_CENTER); //default position
    
    //try to set specific font bugfix for mac support
    #ifdef OSX
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);
  	GtkSettings *settings=gtk_settings_get_for_screen(screen);
   	gchar *fontName;
   	g_object_get (G_OBJECT(settings),"gtk-font-name", &fontName,NULL);
   	g_print("-old font %s-", fontName);
   	//g_object_set (G_OBJECT(settings), "gtk-font-name", "Sans 6",NULL);
    #endif
    
    pVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(pWindow), pVBox);

    if (photosRootDir==NULL){
        initFileChooser(FALSE); //to select the photosDir
    } else {
        initProgressBar(); //to show a waiting screen during the load of treeview
    }
    
	g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(onDestroy), NULL);
    g_signal_connect(G_OBJECT(pWindow),"map-event",G_CALLBACK(windowMapCallBack), NULL); //UI ready 
	
	gtk_widget_show_all(pWindow);    	
}



static GtkWidget *_pWindow; //used in initFileChooser & btnValidateCB
/*
Select the photo root directory
chgRootDir is used for the case we are not in a new usage of the app but a change of the photorootdir with the option 
*/


static void initFileChooser(gboolean chgRootDir){
    _pWindow=pWindow;
    GtkWidget *_pVBox=pVBox;
    if (chgRootDir) {
        _pWindow = gtk_application_window_new(app);
        gtk_window_set_position(GTK_WINDOW(_pWindow), GTK_WIN_POS_CENTER);
        _pVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(_pWindow), _pVBox);
    }
    gtk_window_set_title (GTK_WINDOW (_pWindow), "Select the photos directory");	
    /*GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);
    int screenWidth = gdk_screen_get_width (screen);
    int screenHeight = gdk_screen_get_height (screen);*/

    fileChooser= gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    if (chgRootDir) gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(fileChooser),photosRootDir); //set the current photoroot

    gtk_box_pack_start(GTK_BOX(_pVBox), fileChooser, TRUE, TRUE, 0);

    GtkWidget *comment=gtk_label_new("This will be the root of your browsing. This can be changed in the preferences!");
    g_object_set(comment, "margin", 10, NULL);
    gtk_widget_set_halign(comment,GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(_pVBox), comment, FALSE, FALSE, 0); 
    // load application to load the menu
    /*app = gtk_application_new ("com.tsoft.pwall", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (appActivated), NULL);
    g_signal_connect (app,"startup",G_CALLBACK (appStarted), NULL);
    g_application_run (G_APPLICATION (app), 0, NULL);*/

    GtkWidget *btnValidate = gtk_button_new_with_label("Validate");
    g_object_set(btnValidate, "margin", 10, NULL);
    gtk_widget_set_halign(btnValidate,GTK_ALIGN_END);

    gtk_box_pack_start(GTK_BOX(_pVBox), btnValidate, FALSE, FALSE, 0);    
    if (chgRootDir) { //we come from the chg photo root option
        IntObj *fake=malloc(sizeof(IntObj));
        g_signal_connect (G_OBJECT (btnValidate),"button-press-event",G_CALLBACK (btnValidateCB), fake); //click
    } else 
        g_signal_connect (G_OBJECT (btnValidate),"button-press-event",G_CALLBACK (btnValidateCB), NULL); //click
    
    gtk_widget_show_all(_pWindow);    	
}

static void initProgressBar(void){
    gtk_window_set_title (GTK_WINDOW (pWindow), "pwall");	
    gtk_window_set_default_size(GTK_WINDOW(pWindow), 200, 100); //default size

    GtkWidget *pLabelTitle=gtk_label_new("Please wait for the initialization of the application...");
    g_object_set(pLabelTitle, "margin-top", 20, NULL);
    g_object_set(pLabelTitle, "margin-bottom", 10, NULL);
    g_object_set(pLabelTitle, "margin-left", 10, NULL);
    g_object_set(pLabelTitle, "margin-right", 10, NULL);

    gtk_box_pack_start(GTK_BOX(pVBox), pLabelTitle, TRUE, TRUE, 0); 

    /*Create a progressbar and add it to the window*/
    pProgressBar = gtk_progress_bar_new ();
    g_object_set(pProgressBar, "margin-left", 10, NULL);
    g_object_set(pProgressBar, "margin-right", 10, NULL);
    g_object_set(pProgressBar, "margin-bottom", 20, NULL);

    gtk_box_pack_start(GTK_BOX(pVBox), pProgressBar, FALSE, FALSE, 0);    

    /*Fill in the given fraction of the bar. Has to be between 0.0-1.0 inclusive*/
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pProgressBar), 0.0);
}

void onDestroy(GtkWidget *pWidget, gpointer pData){
    g_print("\nwaitingscreen ondestroy\n");
	//gtk_main_quit();
}

/*
Triggered after the selection of the photosRootDir
*/
static gboolean  btnValidateCB (GtkWidget *event_box, GdkEventButton *event, gpointer data)  {
    gboolean chgRootDirOpt=FALSE; //we come from the change photoDir option
    if (data) chgRootDirOpt=TRUE;
    char *resultUri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(fileChooser));
    char *result= g_filename_from_uri(resultUri,NULL,NULL);
    GtkDialogFlags    flags = GTK_DIALOG_DESTROY_WITH_PARENT;
    GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(pWindow),flags,
                            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,"Please confirm the folder root selection \"%s\"", result);
    int dialogConfirm = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog); 
    if (dialogConfirm==GTK_RESPONSE_OK) {
        photosRootDir = result;
        savePhotosRootDirToIniFile();
        gtk_widget_destroy(fileChooser);//to fix a bug in the removeAllWidgets 
        if (!chgRootDirOpt) {
            removeAllWidgets(GTK_CONTAINER(pVBox));
            initProgressBar(); //show the waiting screen 
            gtk_widget_show_all(pWindow);
            while (gtk_events_pending ())
                gtk_main_iteration ();
            windowMapCallBack(NULL,NULL,NULL); //start the initialization
        } else {
            gtk_window_close(GTK_WINDOW(_pWindow));
            refreshPhotoArray(TRUE);
            scroll2Row(0,TOP);
            focusIndexPending=0;
            updateStatusMessage(g_strdup_printf("The photo directory has changed to %s.",photosRootDir));
        }
    } else if (chgRootDirOpt) gtk_window_close(GTK_WINDOW(_pWindow));

}


/*
Render the progressbar during the loading of the sub folders of the photoRootDir
*/
static gboolean fillProgressBar (gpointer   user_data){
    static int direction=RIGHT; //init à droite
    
    if (cancelProgress) return FALSE; //will stop repetition
    
    /*Get the current progress*/
    gdouble fraction = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (pProgressBar));

    /*Increase the bar by 10% each time this function is called*/
    if (direction==RIGHT)  fraction += 0.05;
    else fraction -= 0.05;

    /*Fill in the bar with the new fraction*/
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pProgressBar), fraction);

    //reset the direction  //test comparaison double fraction == 0 et fraction == 1  
    if (fraction  < 0.001 ) direction = RIGHT;   
    if (fabs(fraction-1) < 0.001 ) {direction = LEFT;   }
    return TRUE;
}

/*
Start loading the left tree view content (all the folders)
*/
static void startThread(void){
    if( (pThread = g_thread_new("loadTreeModel",(GThreadFunc)_loadTreeModel, NULL)) == NULL)
         g_print("Thread create failed!!");
}

/*
Load the photo directory tree to attach to the treeview later
*/
static void *_loadTreeModel(void *pointer){
    //we create the store to host the treeview data	
    //each row of the tree  has 4 fields : dirname, fullpath, idnode, counter
    pStore=gtk_tree_store_new(N_COL,G_TYPE_STRING,G_TYPE_STRING, G_TYPE_INT,G_TYPE_INT); 

    loadTreeModel(); //do the job
    //terminate the work - go back to main thread
    gdk_threads_add_idle(startOrganizer, NULL);
    
    //process finished
    cancelProgress=TRUE; //for the animation of the progressbar to stop at the end
}

void loadTreeModel(void){
    g_print("loadTreeModel");
    //init folder counter arrays to be filled and injected in the pStore
    folderNodeArray = g_array_new (FALSE, FALSE, sizeof (gint)); //array of all the idnodes folder
    folderCounterArray = g_array_new (FALSE, FALSE, sizeof (gint));//array of number of files by folder
    //we feed the store with the subdirs of the photosRootDir 
    photosRootDirCounter = readRecursiveDir(photosRootDir, NULL, TRUE);  //function is coming from photoOrganizer
    addCounters2Tree(NULL); //update the number of files in each dir
    g_print( "%i files in photoRootDir and subdirs\n", photosRootDirCounter);
    int children=gtk_tree_model_iter_n_children (GTK_TREE_MODEL(pStore), NULL); //read the number of top level node
    //g_print("pstore count %i-",children);
    int rootFilesCounter=countFilesInDir(photosRootDir,TRUE,FALSE); //calculate numbers of file in the photoroot dir not in subdir
    
    /*the model for the tree starts with subdirs of photosRootDir but in a few cases it's better to have the photosRootDir, the root node of the tree */    
    if (children==0 || dirInCmd || rootFilesCounter !=0 ||photosRootDirCounter==0) { 
        //if children ==0 we need at least one entry in the tree       
        //for dir asked with command, if files exists in the root or if there is nothing at all, we show this dir as the root of the tree node 
        //rootFilesCounter !=0, if files exists directly under the photosRootDir, we have to show them so the root node of the tree is photosRootDir
        //photosRootDirCounter==0 if there is no photos neither videos in photosRootDir and subdirs, we need at least one entry in the tree
        gtk_tree_store_clear(pStore); //we remove all the rows of the store
        
        
        /*get the id node of the photorootdir */
        struct stat bufStat;
        int ret=stat(photosRootDir, &bufStat);
        long int inode;
        if (ret==0) inode= bufStat.st_ino; else return;
        
        photosRootDir=g_strdup(getCanonicalPath (photosRootDir)); //resolve relative path if needed especialy when it is transmitted in a command

        char *pos=g_strrstr (photosRootDir, "/");
        int p = pos ? pos - photosRootDir : -1;

        if (p!=-1){
            char *right=++pos; //basename for the dir
            //g_print("right %s", right);
            
            //we create the first element of the tree
            GtkTreeIter child;
            gtk_tree_store_append (pStore, &child, NULL); //create the child     
            gtk_tree_store_set (pStore, &child, BASE_NAME_COL,right, FULL_PATH_COL,photosRootDir,ID_NODE,inode,-1); //set content of the child 

            folderNodeArray = g_array_new (FALSE, FALSE, sizeof (gint));
            folderCounterArray = g_array_new (FALSE, FALSE, sizeof (gint));
            
            photosRootDirCounter=readRecursiveDir(photosRootDir, &child, TRUE); //fill the tree model
            
            addCounters2Tree(NULL); //update rows with the number of files of each dir
            
        }
    }
}
/*
Start the photo Organizer window
*/
static int startOrganizer (gpointer user_data){
    g_print("startOrganizer");
    
    //photoOrganizerInit(NULL);
    photoOrganizerInit(pWindow);
    
    // we run this command "gtk_widget_hide(pWindow)"after the photoorganizer is ready ;
    // destroy is not working. It closes the app
    // that is not the case with the hide
    //removeAllWidgets(GTK_CONTAINER(pWindow)); //we clean the content of the window
    //gtk_widget_destroy(pWindow);//we destroy the window 
    return FALSE;
}

/*
Run when the photoInit window is ready
*/
static gboolean  windowMapCallBack (GtkWidget *widget, GdkEvent *event, gpointer data)  {
    g_print("waiting screen ready\n");
    if (photosRootDir==NULL){// launch the jfiledialog
        //set the size of the filechooser window
        int screenWidth=getMonitorWidth(pWindow);
        int screenHeight=getMonitorHeight(pWindow);
        g_print("screenWidth%i screenheight%i",screenWidth,screenHeight);
        gtk_window_resize(GTK_WINDOW(pWindow), screenWidth*0.5f, screenHeight*0.5f); //default size
        g_print("jfilechooser loaded");
    } else { //load the tree dir to show in the next window
        g_timeout_add (300, fillProgressBar, GTK_PROGRESS_BAR (pProgressBar));
        startThread();  //attention normalement il faudrait attendre le dernier thread
    }
}

/*
Create read and update the .ini file 
*/
static int processInitFile(void){
    GKeyFile *keyFile =g_key_file_new ();
    char *iniFilePath =g_strdup_printf ("%s/%s", appDir, iniFileName);
    //check file exists
    FILE *file=fopen(iniFilePath, "r"); //r for txt files rb for binary files
    if (!file) {
        file=fopen(iniFilePath, "w"); //create a new empty file
        if (file) fclose(file);
        else g_print("error in creating the file %s",iniFilePath);
    } else {
        fclose(file);
    }
    
    int loaded=g_key_file_load_from_file (keyFile, iniFilePath, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
    if (!loaded) {
        g_print("can't load the ini file %s", iniFilePath);
        return FALSE;
    }
    //photosRootDir ex "/home/francis/Cloud Pictures", be carefull there no " in the ini file for string keys
    //ie photosRootDir=/home/francis/Cloud Pictures (no ")
    if (photosRootDir==NULL){ //it might be initialized by the command line
        photosRootDir = g_key_file_get_string (keyFile,"default", "photosRootDir", NULL);
        if (photosRootDir == NULL) {
            g_print("photosRootDir needs a value!");
        } else {
            //test existence dir
            struct stat st = {0};
            if (stat(photosRootDir, &st) == -1) {
                g_print("photosRootDir %s from ini file doesn't exist",photosRootDir);
                photosRootDir=NULL;
            } else {
                g_print("photosRootDir is %s\n",photosRootDir);
            }
        }
    } else
        g_print("photosRootDir is %s\n",photosRootDir);
    //first use of the app
    gint64 first=g_key_file_get_int64 (keyFile,"default","first",NULL);
    if (first==0){
        first=g_get_real_time ();
        g_key_file_set_int64 (keyFile,"default","first", first);
    }

    //last use of the app
    gint64 last=g_get_real_time ();
    g_key_file_set_int64 (keyFile,"default","last", last);

    //counter of app launching 
    gint counter=g_key_file_get_integer (keyFile,"default","counter",NULL);

    counter++;
    g_key_file_set_integer (keyFile,"default","counter", counter);
    
    g_key_file_save_to_file (keyFile,iniFilePath, NULL);   
    g_key_file_unref (keyFile);
    return TRUE; //OK
}

static void savePhotosRootDirToIniFile(void){
    GKeyFile *keyFile =g_key_file_new ();
    char *iniFilePath =g_strdup_printf ("%s/%s", appDir, iniFileName);
    g_key_file_load_from_file (keyFile, iniFilePath, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
    g_key_file_set_string (keyFile,"default","photosRootDir", photosRootDir);
    g_key_file_save_to_file (keyFile,iniFilePath, NULL); 
    g_print("saved ini with photoRootDir %s",photosRootDir);
}
