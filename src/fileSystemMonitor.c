/*
fileSystemMonitor.c
use GFileMonitor to check if something has changed in the photos we are browsingfile
We have to register all the subdir of photosRootDir with g_file_monitor_directory
as multiple events are generated by gfilemonitor we use listener and a thread to marshall which event needs to trigger an action.
The listener works in the gtk thread
Action is a refreshPhotoWall when activeWindow == ORGANIZER
this monitorint must be in waiting mode when acting on files directly with the pwall. In this case, the refresh is managed by pwall 
The registration must be updated if subdirs are changing names
*/

#include <stdlib.h>
#include <gtk/gtk.h>
#include <tsoft.h>
#include <photoOrganizer.h>
#include <multiViewer.h>
#include <fileSystemMonitor.h>

#define RUN_AFTER_DELAY 1000000//run after last signal emitted in micros
#define SLEEP_DELAY 500000 //loop delay in the fsMonitoring thread
#define TWO_RUN_DELAY_MIN 5000000 //minimum time between 2 runs can happen
#define UNLOCK_DELAY 500 //in ms to delay the unlock

static gint64 lastSignalTime=0; //last emitted signal time
static gint64 lastRunTime=0;  //last time we run something because of the monitorinf
static gboolean lock=FALSE; //used to block the monitorLoop
static gboolean unlockWaiting=FALSE; //used to cancel the unlock if a locks arrives in between
static gboolean stop=FALSE; //used to finish monitorLoop


GPtrArray *arrayMonitorDir=NULL; //store all the monitor objects

static void monitorEventListener (GFileMonitor *monitor,GFile *file,GFile *other_file, GFileMonitorEvent event_type, gpointer data);
static void addChildrenDirs2Monitor(GtkTreeIter *parent);
static void monitorLoop(void *pointer);
static gboolean monitorAction(gpointer user_data);
static void free_on_closure_notify(gpointer data, G_GNUC_UNUSED GClosure *closure);
static gboolean unlockMonitorDelayed(gpointer data);



/*
    refresh all the subdirectories and the photosRootDir directory which we want to monitor
*/
void loadDirectories2Monitor(void){
    g_print("\nloadDirectories2Monitor\n");
    lockMonitor();
    lastSignalTime=0;
    //it will unref each dirMonitor, disconnect the handler and remove the data sent to the handler (free_on_closure_notify)
    if (arrayMonitorDir) g_ptr_array_unref(arrayMonitorDir); 
    arrayMonitorDir= g_ptr_array_new_with_free_func (g_object_unref); //doesn't work with g_free
    addChildrenDirs2Monitor(NULL);
    unlockMonitor();
}



/*
scan the TreeModel to register the subdirs with g_file_monitor_directory
recursive function
parent=NULL for a scan of all the tree model
*/
static void addChildrenDirs2Monitor(GtkTreeIter *parent){
    GtkTreeIter iter;
    gchar *_fullPathName;
    gboolean iter_valid=gtk_tree_model_iter_children (pSortedModel, &iter, parent); 
    while (iter_valid){
        gtk_tree_model_get(pSortedModel, &iter, FULL_PATH_COL, &_fullPathName,-1);
        GFile *file = g_file_new_for_path (_fullPathName);
        GFileMonitor *dirMonitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, NULL);
        g_object_unref (file);
        if (dirMonitor){
            gchar *data=g_strdup(_fullPathName);
            g_signal_connect_data (G_OBJECT (dirMonitor),"changed",G_CALLBACK (monitorEventListener), data, free_on_closure_notify, 0);
            g_ptr_array_add(arrayMonitorDir,dirMonitor);
            //g_print("\ng_file_monitor %s\n",data);
        }
        if (gtk_tree_model_iter_has_child (pSortedModel, &iter)){
            addChildrenDirs2Monitor(&iter);
        }
        iter_valid = gtk_tree_model_iter_next(pSortedModel, &iter); 
    }
}

static void free_on_closure_notify(gpointer data, G_GNUC_UNUSED GClosure *closure){
	//g_print("free_on_closure_notify %s",(gchar *)data);
	g_free(data);
}


static void monitorEventListener (GFileMonitor *monitor,GFile *file,GFile *other_file, GFileMonitorEvent event_type, gpointer data){
	static int i=0;
    if (lock) return; 
	gchar *listenerDir=(gchar *)data;
	gchar *text=g_strdup_printf("changed listener for Dir=%s, file updated=%s, eventtype=%i, compteur =%i", listenerDir, g_file_get_path(file),event_type, i );
	g_print("%s\n",text);	
	gchar *changedPath=g_file_get_path(file);

	//we only consider the event 1 and 2 which are the last one of a move, copy, and the delete event	
	//we don't consider signal if a subdir is moved or copy and is already registered as a listener, there would be 2 events
	if (event_type==G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT && (g_strcmp0(listenerDir,changedPath)!=0)) { 
		lastSignalTime=g_get_monotonic_time ();
	}
	if (event_type==G_FILE_MONITOR_EVENT_DELETED){
		lastSignalTime=g_get_monotonic_time ();
	}

	g_free(changedPath);
	i++;
}

/*
run monitorLoop in a new thread
*/
void startMonitor(void){
    if ((g_thread_new("File system monitoring",(GThreadFunc)monitorLoop, NULL)) == NULL)
             g_print("Thread create failed!!");
}

void lockMonitor(void){
    if (unlockWaiting) unlockWaiting=FALSE; //it will cancel the unlock
    lock=TRUE;
}

void unlockMonitor(void){
    unlockWaiting=TRUE;
    gdk_threads_add_timeout(UNLOCK_DELAY,unlockMonitorDelayed, NULL);
}

static gboolean unlockMonitorDelayed(gpointer data){
    if (unlockWaiting) {
        lock=FALSE;
        unlockWaiting=FALSE;
    }
    return FALSE;
}   


void stopMonitor(void){
    stop=TRUE;
}

static void monitorLoop(void *pointer){
    while (TRUE){
        if (stop) return;
		if (!lock && lastSignalTime!=0){		
			gint64 now=g_get_monotonic_time ();
			if ((now - lastSignalTime)>RUN_AFTER_DELAY){ //tempo to enable the signal
				if ((now -lastRunTime)>TWO_RUN_DELAY_MIN){ //tempo between 2 runs
					lastSignalTime=0;
					lastRunTime=now;
					gdk_threads_add_idle(monitorAction,NULL);
				} else { // we don't respect the delay bewteen 2 run, we disable the signal
					lastSignalTime=0;
				} 
			} 
		}
		g_usleep(SLEEP_DELAY); //run every 0,5s	
	}
}

static gboolean monitorAction(gpointer user_data){
    static int i=0;
    if (activeWindow == ORGANIZER){   
        g_print("monitor Action %i\n",i); 
        refreshPhotoWall(NULL,NULL);
        i++;
    }
    if (activeWindow == VIEWER){
        g_print("monitor Action in viewer %i\n",i); 
        organizerNeed2BeRefreshed=TRUE;
    }
    return FALSE;
}