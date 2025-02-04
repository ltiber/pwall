//thumbnail.c
//to compile see gcc in photoOrganizer.c

//This source is the batch process to create all the square thumbnails needed by the photowall.
//It shows a dialog box with real time progression of the process.
//It is a multithreading program.
//This process is asked to be run when you first use the application.
//We can run it again with the option "refresh thumbnails" 

#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include <tsoft.h>
#include <photoInit.h>
#include <photoOrganizer.h>

enum {RIGHT, LEFT};
enum {INIT, RUNNING, CANCELED, FINISHED, DESTROYED};
typedef struct ThreadParamObj {
    char *fullPath;
    void (* completedThreadCB) ();
} ThreadParamObj;

static void windowDestroyCB(GtkWidget *pWidget, gpointer pData);
static gboolean fillProgressBar (gpointer   user_data);
static gboolean  buttonPressCB (GtkWidget *event_box, GdkEventButton *event, gpointer data);
static gboolean  checkOneFolderCB (GtkWidget *event_box, GdkEventButton *event, gpointer data);
static void *heavyWork(void *pointer);
static void startThread(void);
static int followupCB(char *value);
static int resetStatusCB (gpointer user_data);
static int resetButtonLabel(gpointer user_data);
static int followup2CB(char *value);
static int completedThreadLauncherCB (gpointer user_data);
static gboolean  windowMapCB (GtkWidget *widget, GdkEvent *event, gpointer data);
static void removeThumbnails(const gchar *dirPath);


//global data 
static GtkWidget *pProgressBar, *pLabelStatus, *pButton, *pWindow, *checkForceClean, *checkOneFolder;
static int cancelRunning=FALSE;
static int state=INIT;
static GThread  *pThread; 

void thumbnailDialogInit(GtkWindow *parent){
    //reset
    state=INIT;
    cancelRunning=FALSE;
    
    GtkWidget *pVBox, *pLabelTitle;

	pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);	
    g_signal_connect(G_OBJECT(pWindow),"map-event",G_CALLBACK(windowMapCB), NULL); 

	gtk_window_set_position(GTK_WINDOW(pWindow), GTK_WIN_POS_CENTER); //default position
    gtk_window_set_default_size(GTK_WINDOW(pWindow), 450, 100); //default size

    pVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(pWindow), pVBox);
    gtk_box_set_homogeneous (GTK_BOX(pVBox), FALSE);

    pLabelTitle=gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL (pLabelTitle),"This process will create <span foreground=\"#EC8156\">thumbnails</span> for all <span foreground=\"#EC8156\">your photos</span>. It can be long!!!\n\nThe app uses thumbnails to display photo walls and it can take time to create one for each photo. On basic laptop, it can take 7mn to generate thumbnails for 10000 photos. Each thumbnail costs about 10ko and is stored in you home dir in the .pwall subdir. The best thing is to run once at first time or when you renew massively your photos. Updates will be done based on file's last modified date.\n\nNevertheless feel free to cancel this operation if it is not the right moment.The app will create thumbnails <span foreground=\"#EC8156\">on the fly.</span> But the UI might <span foreground=\"#EC8156\">freeze</span> some time to time.");
    gtk_widget_set_size_request(pLabelTitle,400,100);
    gtk_label_set_line_wrap(GTK_LABEL(pLabelTitle),TRUE);
    gtk_label_set_justify(GTK_LABEL(pLabelTitle),GTK_JUSTIFY_FILL);
    gtk_widget_set_name(pLabelTitle,"thumbnailExplanation");
    
    g_object_set(pLabelTitle, "margin-top", 20, NULL);
    g_object_set(pLabelTitle, "margin-bottom", 20, NULL);
    g_object_set(pLabelTitle, "margin-left", 10, NULL);
    g_object_set(pLabelTitle, "margin-right", 10, NULL);

    gtk_box_pack_start(GTK_BOX(pVBox), pLabelTitle, FALSE, FALSE, 0); 

    // checkbox for force clean
    checkForceClean= gtk_check_button_new_with_label("Force Clean");
    g_object_set(checkForceClean, "margin-top", 20, NULL);
    g_object_set(checkForceClean, "margin-bottom", 20, NULL);
    g_object_set(checkForceClean, "margin-left", 20, NULL);
    gtk_box_pack_start(GTK_BOX(pVBox), checkForceClean, FALSE, FALSE, 0);
    gtk_widget_set_name(checkForceClean,"thumbnailExplanation"); //to get larger font
    //gtk_widget_set_can_default (checkForceClean,FALSE);
    
    checkOneFolder=gtk_check_button_new_with_label("Limit the processing to one Folder");
    g_object_set(checkOneFolder, "margin-top", 20, NULL);
    g_object_set(checkOneFolder, "margin-bottom", 20, NULL);
    g_object_set(checkOneFolder, "margin-left", 20, NULL);
    gtk_box_pack_start(GTK_BOX(pVBox), checkOneFolder, FALSE, FALSE, 0);
    gtk_widget_set_name(checkOneFolder,"thumbnailExplanation"); //to get larger font
    g_signal_connect (G_OBJECT (checkOneFolder),"button-press-event",G_CALLBACK (checkOneFolderCB), NULL); //click
    

        // pButton
    pButton = gtk_button_new_with_label("Start/Stop MProgress");
    resetButtonLabel(NULL);
    g_object_set(pButton, "margin-top", 20, NULL);                                    
    g_object_set(pButton, "margin-left", 80, NULL);
    g_object_set(pButton, "margin-right", 80, NULL);
    g_object_set(pButton, "margin-bottom", 20, NULL);                                    
    gtk_box_pack_start(GTK_BOX(pVBox), pButton, FALSE, FALSE, 0);    
    g_signal_connect (G_OBJECT (pButton),"button-press-event",G_CALLBACK (buttonPressCB), NULL); //click
    //gtk_widget_set_can_default (pButton,TRUE);
    
    /*Create a progressbar and add it to the window*/
    pProgressBar = gtk_progress_bar_new ();
    g_object_set(pProgressBar, "margin-top", 20, NULL);
    g_object_set(pProgressBar, "margin-left", 10, NULL);
    g_object_set(pProgressBar, "margin-right", 10, NULL);

    gtk_box_pack_start(GTK_BOX(pVBox), pProgressBar, FALSE, FALSE, 0);    

    /*Fill in the given fraction of the bar. Has to be between 0.0-1.0 inclusive*/
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pProgressBar), 0.0);

    pLabelStatus=gtk_label_new("...");
    g_object_set(pLabelStatus, "margin-top", 20, NULL);
    gtk_box_pack_start(GTK_BOX(pVBox), pLabelStatus, FALSE, FALSE, 0); 

    
    // window
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pWindow),TRUE);  //to avoid multi windows in the app manager
    gtk_window_set_modal (GTK_WINDOW(pWindow),TRUE);
    gtk_window_set_transient_for (GTK_WINDOW(pWindow), parent);

	g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(windowDestroyCB), NULL); 
	gtk_widget_show_all(pWindow);
}

static gboolean  windowMapCB (GtkWidget *widget, GdkEvent *event, gpointer data)  {
gtk_widget_grab_focus(pButton);
}
    

static int resetButtonLabel(gpointer user_data){
    switch (state) { 
        case INIT: gtk_button_set_label(GTK_BUTTON(pButton),"Start!"); break;
        case RUNNING: gtk_button_set_label(GTK_BUTTON(pButton),"Cancel!"); break;
        case CANCELED: gtk_button_set_label(GTK_BUTTON(pButton),"Start again!"); break;
        case FINISHED: gtk_button_set_label(GTK_BUTTON(pButton),"Finished!"); break;
    }
    return FALSE;
}
 
static gboolean fillProgressBar (gpointer   user_data){
  static int direction=RIGHT; //progression direction from left to right
  if (cancelRunning) {
    g_print("fillProgressBar Stopped!!!!");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pProgressBar), 1);
    return FALSE; //will stop repetition
  }
  /*Get the current progress*/
  gdouble fraction = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (pProgressBar));

  /*Increase the bar by 10% each time this function is called*/
  if (direction==RIGHT)  fraction += 0.05;
  else fraction -= 0.05;

  /*Fill in the bar with the new fraction*/
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pProgressBar), fraction);
    
  //reset the direction  
  if (fraction  < 0.001 ) direction = RIGHT;   
  if (fabs(fraction-1) < 0.001 ) {direction = LEFT;   }
  return TRUE;
}

static gboolean  buttonPressCB (GtkWidget *event_box, GdkEventButton *event, gpointer data)  {
    switch (state) { 
        case INIT: 
        case CANCELED: //start or restart
            g_print("start or restart\n");
            cancelRunning=FALSE; //reset the value
            gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pProgressBar), 0.0); //needed for restart
            g_timeout_add (400, fillProgressBar, GTK_PROGRESS_BAR (pProgressBar));
            startThread();  //normaly we have to wait till the last thread is completed
            state = RUNNING; //not a problem for the switch
            resetButtonLabel(NULL);
            break;        
        case RUNNING: //cancel
            g_print("cancel\n");
            cancelRunning=TRUE;
            state = CANCELED; 
            resetButtonLabel(NULL);
            break;
        case FINISHED: //destroy action
            g_print("thumbnail dialog closed\n");
            state = DESTROYED;
            removeAllWidgets(GTK_CONTAINER(pWindow)); //we clean the content of the window
            gtk_widget_destroy(pWindow);//we destroy the window
            break;
    }
}

static gboolean  checkOneFolderCB (GtkWidget *event_box, GdkEventButton *event, gpointer data)  {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkOneFolder))) { //uncheck
        gtk_button_set_label(GTK_BUTTON(checkOneFolder),"Limit the processing to one Folder");
    } else { //check
        GtkWidget *dialog;
        GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        gint res;        
        dialog = gtk_file_chooser_dialog_new ("Select the folder",
                                          GTK_WINDOW(pWindow),
                                          action,
                                          "Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "Ok",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog),photosRootDir);
        res = gtk_dialog_run (GTK_DIALOG (dialog));
        if (res == GTK_RESPONSE_ACCEPT){
            char *folder;
            GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
            folder = gtk_file_chooser_get_filename (chooser);
            g_print("folder selected %s", folder);        
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkOneFolder), TRUE);            
            gtk_button_set_label(GTK_BUTTON(checkOneFolder),folder);
            g_free (folder);
          } else { //cancel
            gtk_button_set_label(GTK_BUTTON(checkOneFolder),"Limit the processing to one Folder");            
        }
        gtk_widget_destroy (dialog);
        
    }
    return FALSE; //propagate the event
}

/*
launch the thread function
*/
static void startThread(void){
    if( (pThread = g_thread_new("thumbnails creation",(GThreadFunc)heavyWork, NULL)) == NULL)
         g_print("Thread create failed!!");
}

/*
launch the creation of the thumbnails
be carefull with this function, it runs in a seperate thread, all UI calls need to use gdk_threads_add_idle
*/
static void *heavyWork(void *pointer){
    static int i=0;
    i++;
    int counter;
    gint64 initTime=g_get_monotonic_time ();        
    state = RUNNING;
    //g_print("just before thumbnails creating");
    #if defined(LINUX) || defined(WIN)
    int _size=PHOTO_COMPRESS_SIZE;
    #else 
    int _size=PHOTO_COMPRESS_SIZE*2;
    #endif
    const char *photos2process;

    //if checkForceClean == TRUE remove the content thumbnailDir
    //it is meaningfull if checkOneFolder is FALSE
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkForceClean)) && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkOneFolder))) {
        g_print("\nforce clean\n"); 
        gchar *cmd = g_strdup_printf("rm -rf %s/%s",thumbnailDir,"*");
        //launch the cmd
        GError *err = NULL; gchar *stdOut,*stdErr = NULL;int status;
        g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err);
        g_free(cmd);
    }
    //we have selected one folder to process
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkOneFolder))){
        photos2process=gtk_button_get_label(GTK_BUTTON(checkOneFolder));
        //we clean the thumbnails of the selected dir
        removeThumbnails(photos2process);
        
    }else{ //we process all the photos of the root dir
        photos2process=photosRootDir;
    }
    //we generate thumbnails if they don't exist or are fresher    
    counter=createThumbnail4Dir(photos2process,thumbnailDir, _size, followupCB);
    //finished
    if (state == RUNNING) state=FINISHED; 
    if (state != DESTROYED) {
        gdk_threads_add_idle(resetButtonLabel,NULL); //
        gint64 now=g_get_monotonic_time ();        
        int inter=(now-initTime) / 1000000 ; //en s
        gchar *status =g_strdup_printf ("Completed : %is - %i thumbnails processed of %i files in the photos directory.",inter + 1, counter, photosRootDirCounter); //round to the next second
        g_print("%s",status);
        g_usleep(500000);
        gdk_threads_add_idle(resetStatusCB, status);
        cancelRunning=TRUE; //for the animation of the progressbar to stop at the end
    }
}

static int followupCB(char *value){
    //The changes on the UI must be done in the GTK thread
    if (state != DESTROYED) gdk_threads_add_idle(resetStatusCB,value);
    return cancelRunning; //return TRUE will stop createThumbnail4Dir
    //the problem is that if return is TRUE the function is a neverending call
}

static int resetStatusCB (gpointer user_data){
    char *s1=user_data;
    //g_print("reset status%s\n",s1);
    gtk_label_set_text(GTK_LABEL(pLabelStatus),s1);
    g_free(s1); //free the string created in createThumbnail4Dir
    return FALSE;
}

//before destroying the window
static void windowDestroyCB(GtkWidget *pWidget, gpointer pData){
    if (state==RUNNING) {
       state=DESTROYED;
       cancelRunning = TRUE; // it will close the current running thread if it exists
    }   
}

static int followup2CB(char *value){
    return cancelRunning; //return TRUE will stop createThumbnail4Dir
}

static int completedThreadLauncherCB (gpointer user_data){
    void (* completedThreadCB) ()=user_data;
    completedThreadCB();
    return FALSE; //to stop the loop
}

/*
we remove the thumbnails of the dirpath
*/    
static void removeThumbnails(const gchar *dirPath){
    int iDir=getFileNode(dirPath);
    gchar *thumbnailPath =g_strdup_printf ("%s/%i",thumbnailDir,iDir);
    DIR *directory = opendir(thumbnailPath); //open the current dir
    struct dirent *pFileEntry;
    if(directory != NULL) {
        while((pFileEntry = readdir(directory)) != NULL)   {
            char *filePath = g_strdup_printf("%s/%s",thumbnailPath, pFileEntry -> d_name); 
            if (g_strcmp0(pFileEntry -> d_name,"..") == 0 || g_strcmp0(pFileEntry -> d_name,".") == 0) continue; //avoid the . and .. dirs
            if (pFileEntry->d_type!=DT_DIR) {
                    remove(filePath);                
            }
            g_free (filePath);        
        }
        closedir(directory);
    }
    g_free(thumbnailPath);
}

