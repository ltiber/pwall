/* 
photoDialogs 
photoOrganizer uses this module to handle the dialog boxes and options given to the user
example folderRename, photoDelete, changeDate...
It was too big to be hosted in the photoOrganizer source
*/

//gcc -o pepper thumbnail.c tsoft.c photoInit.c photoDialogs.c photoOrganizer.c photoViewer.c -I. `pkg-config --libs --cflags gtk+-3.0`

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <gtk/gtk.h>
#include <tsoft.h>
#include <photoInit.h>
#include <photoDialogs.h>
#include <photoOrganizer.h>
#include <photoViewer.h>

//private function
static gboolean  changeDateCB (GtkWidget *event_box, GdkEventButton *event, gpointer data);
static gboolean renameFolderCB (GtkWidget *event_box, GdkEvent *event, gpointer data);
static gboolean newFolderCB (GtkWidget *event_box, GdkEvent *event, gpointer data);
static char * extractGPS(char *text);

/*
change the file date with an input dialog
*/
void changeDateDialog(void){
	GtkWidget *pDialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);	
	gtk_window_set_position(GTK_WINDOW(pDialog), GTK_WIN_POS_CENTER); //default position
    gtk_window_set_default_size(GTK_WINDOW(pDialog), 400, 200); //default size
    // modal window
    gtk_window_set_title (GTK_WINDOW (pDialog), "change the file date (exif & last modified)");	
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pDialog),TRUE);  //to avoid multi windows in the app manager
    gtk_window_set_modal (GTK_WINDOW(pDialog),TRUE);
    gtk_window_set_transient_for (GTK_WINDOW(pDialog), GTK_WINDOW(pWindowOrganizer));

    GtkWidget *pVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(pDialog), pVBox);
    gtk_box_set_homogeneous (GTK_BOX(pVBox), FALSE);
    
    GtkWidget *pCalendar= gtk_calendar_new();
    g_object_set(pCalendar, "margin", 10, NULL);
    gtk_box_pack_start(GTK_BOX(pVBox), pCalendar, TRUE, TRUE, 0);

    GtkWidget *pHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);    
    GtkWidget *timeLabel=gtk_label_new("Time (hh:mm<24:00)");
    g_object_set(timeLabel, "margin", 10, NULL);
    gtk_box_pack_start(GTK_BOX(pHBox), timeLabel, FALSE, FALSE, 0); 
    GtkWidget *timeInput=gtk_entry_new();
    gtk_entry_set_text (GTK_ENTRY(timeInput),"10:00"); //default value
    gtk_box_pack_start(GTK_BOX(pHBox), timeInput, FALSE, FALSE, 0); 
    gtk_box_pack_start(GTK_BOX(pVBox), pHBox, FALSE, FALSE, 0);
    
    pHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);    
    GtkWidget *btnChange = gtk_button_new_with_label("Change");
    g_object_set(btnChange, "margin", 10, NULL);
    gtk_box_pack_start(GTK_BOX(pHBox), btnChange, FALSE, FALSE, 0); 
    gtk_widget_set_halign(pHBox,GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(pVBox), pHBox, FALSE, FALSE, 0);
    
    GPtrArray *widgetArray = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add(widgetArray, pCalendar);
    g_ptr_array_add(widgetArray, timeInput);
    g_signal_connect (G_OBJECT (btnChange),"button-press-event",G_CALLBACK (changeDateCB), widgetArray); //click
    
    gtk_widget_show_all(pDialog);

}
/*
process the changing date ( use of touch and jhead command ) 
*/
static gboolean  changeDateCB (GtkWidget *event_box, GdkEventButton *event, gpointer data)  {
    gchar *firstFullPath=NULL;
    GPtrArray *widgetArray =data;
    GtkCalendar *pCalendar=g_ptr_array_index(widgetArray,0);
    GtkWidget *timeInput=g_ptr_array_index(widgetArray,1);
    GError *err = NULL;
    gchar *stdOut = NULL;
    gchar *stdErr = NULL;
    int status;
    int y,m,d;
    char *message=NULL;
    int error=FALSE;
    gchar *cmd1, *cmd2;
    gtk_calendar_get_date(pCalendar,&y,&m,&d);
    m++;//the month start at 0
    g_print("change date %04d:%02d:%02d %s",y,m,d,gtk_entry_get_text(GTK_ENTRY(timeInput)));
    const gchar *_time=gtk_entry_get_text(GTK_ENTRY(timeInput));
    gchar *time=g_strdup(_time);
    trim(time);
    //g_print("time=%s",time);
    gchar **times = g_strsplit(time,":", 2);
//    gchar *mmm=times[0];
    gchar *hh, *mm;
    int ihh, imm;
    hh=*times;  //1er split
    if (times++) {mm=*times; times--;} //times-- for free
    if (hh && mm) {
        //g_print("split hh:mm=%s:%s",hh,mm);  //OK
    } else {error=TRUE; message="time format not valid";}
    if (!error){
        ihh=atoi(hh);
        imm=atoi(mm);
        g_print("converttoint hh:mm=%i:%i",ihh,imm);
        /*if (ihh && imm) g_print("converttoint hh:mm=%i:%i",ihh,imm);
        else {error=TRUE; message="time format not valid";}*/
    }
    if (!error){
        if (ihh>=24 || imm>=60) {error=TRUE; message="time format not valid";}
    }
    if (!error){ 
        cmd1= g_strdup_printf("touch -mt%04d%02d%02d%02d%02d.01",y,m,d,ihh,imm);
    }
    g_strfreev(times);
    if (!error){
        PhotoObj *pPhotoObj;
        int count=0;
        GString *fileList=g_string_new(NULL);
        if (activeWindow == VIEWER){
            g_string_append_printf(fileList," \"%s\"", viewedFullPath);
            count=1;
        }
        if (activeWindow == ORGANIZER){
            for (int i=0;i<photoArray->len;i++){    
                pPhotoObj=g_ptr_array_index(photoArray,i);
                if (pPhotoObj!=NULL && pPhotoObj->selected) {
                    if (count==0) firstFullPath=g_strdup(pPhotoObj->fullPath);
                    count++;
                    g_string_append_printf(fileList," \"%s\"", pPhotoObj->fullPath);
                }
            }
        }
        #ifdef LINUX
        cmd1=g_strconcat(cmd1,fileList->str,NULL);
        cmd2=g_strconcat("jhead -dsft ",fileList->str,NULL);
        g_string_free(fileList, TRUE);    
        g_print("cmd1%s",cmd1);
        g_print("cmd2%s",cmd2);
        //cmd1 touch -mtyyyyMMddhhmm.01 "file1" "file2" "file3"
        //cmd2 jhead -dsft "file1" "file2" "file3"
        g_spawn_command_line_sync (cmd1, &stdOut, &stdErr, &status, &err);
        g_spawn_command_line_sync (cmd2, &stdOut, &stdErr, &status, &err);
        #elif OSX
        //TODO
        #elif WIN
        //TODO
        #endif
        if (err){
            error=TRUE;
            g_print ("-error: %s\n", err->message);
            updateStatusMessage("Please install jhead with \"sudo apt-get install jhead\" to get this option.");
            g_clear_error(&err);
        } else if (stdErr[0]!='\0'){
            error=TRUE;
            updateStatusMessage(g_strdup_printf("error: %s.",stdErr));        
        } else {//done
            message=g_strdup_printf("File's date changed for %i file(s).",count);    
        }
    }
    //remove dialogbox
    GtkWidget *pWindow= gtk_widget_get_ancestor (timeInput, GTK_TYPE_WINDOW);
    removeAllWidgets(GTK_CONTAINER(pWindow)); //we clean the content of the window
    gtk_widget_destroy(pWindow);

    //remove the selected thumbnails to avoid mix mess
    removeThumbnails(NULL);    
    
    if (!error) refreshPhotoArray(FALSE);
    
    if (activeWindow == ORGANIZER){
        if (message) updateStatusMessage(message); //if message <>NULL
        refreshPhotoArray(FALSE);
        int first=findPhotoFromFullPath(firstFullPath);
        focusIndexPending=first;  //postponed the grab focus-> processed by the next run of scShowAllGtk      
    } 
    if (activeWindow == VIEWER){
        if (message)   {   
            GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(pWindowViewer),GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_MESSAGE_INFO, GTK_BUTTONS_OK,"%s", message);
            gtk_dialog_run (GTK_DIALOG (dialog));
            gtk_widget_destroy (dialog);
            organizerNeed2BeRefreshed=TRUE; //refresh will occurs after leaving the viewer
        }
    }
    //g_ptr_array_unref(widgetArray);
    //gtk_window_close(GTK_WINDOW( gtk_widget_get_ancestor (timeInput, GTK_TYPE_WINDOW)));
}

/*
copy one or several files to another directory (use of cp command)
*/
void copyToDialog(void){
    GError *err = NULL;
    gchar *stdOut = NULL;
    gchar *stdErr = NULL;
    gchar *cmd=NULL;
    gchar *cmd2=NULL;
    int status=0;
    static char *currentFolder = NULL;  //suggest the last copy destination to the user
    char *targetFolder=NULL;
    GtkWidget *dialog;
    
    //choose the destination dir
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    gint res;
    dialog = gtk_file_chooser_dialog_new ("Copy To",
                                          GTK_WINDOW(pWindowOrganizer),
                                          action,
                                          "Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "Ok",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
    if (currentFolder==NULL) currentFolder=photosRootDir;
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog),currentFolder);
    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT){
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        targetFolder = gtk_file_chooser_get_filename (chooser);
        currentFolder= targetFolder;
        g_print ("  %s",targetFolder);
        //g_free (targetFolder); //mem leak
    }
    if (res == GTK_RESPONSE_CANCEL){
        if (activeWindow == ORGANIZER) updateStatusMessage("Copy Canceled");
    }
    gtk_widget_destroy (dialog);
    if (activeWindow == ORGANIZER) updateStatusMessage("Please wait, copy is pending...");
    while (gtk_events_pending ()) gtk_main_iteration(); //consume all the events 
    
    //copy the selection
    if (targetFolder!=NULL){
        PhotoObj *pPhotoObj;
        int count=0;
        char *firstFullPath=NULL;
        GString *fileList=g_string_new(NULL);
        if (activeWindow == VIEWER){
            g_string_append_printf(fileList," \"%s\"", viewedFullPath);
        }
        if (activeWindow == ORGANIZER){
            for (int i=0;i<photoArray->len;i++){    
                pPhotoObj=g_ptr_array_index(photoArray,i);
                if (pPhotoObj!=NULL && pPhotoObj->selected) {
                    if (count==0) firstFullPath=g_strdup(pPhotoObj->fullPath);
                    count++;
                    g_string_append_printf(fileList," \"%s\"", pPhotoObj->fullPath);
                }
            }
            updateStatusMessage(g_strdup_printf("%i file(s) copied to %s",count, targetFolder));
        }
        #ifdef LINUX
        //if file exists in target dir, cp wil backup them with a suffix ~x~
        cmd = g_strdup_printf("cp --backup=numbered -p %s -t \"%s\"",fileList->str, targetFolder);
        g_print("cmd=%s",cmd);
        //change the suffix to prefix
        //we use a bash, we can't use cd, for directly with g_spawn
        //for file in *.~?~; do mv $file `echo $file |sed -e \"s/\\(.*\\)\\.\\(~[0-9]~$\\)/\\2\\1/\"`;done"
        cmd2= g_strdup_printf("\"%s/%s\" \"%s\"",resDir,"clearbackup.sh", targetFolder); //rename backup files
        #elif OSX
        //TODO
        #elif WIN
        //TODO
        #endif
        g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err);
        g_spawn_command_line_sync (cmd2, &stdOut, &stdErr, &status, &err);
        
        if (err){ 
            g_print ("-error: %s\n", err->message);
            if (activeWindow == VIEWER) {
                GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(pWindowViewer),GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_INFO, GTK_BUTTONS_OK,"error: %s", err->message);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
            } else { //organizer
                updateStatusMessage(g_strdup_printf("error: %s", err->message));
            }
            g_clear_error(&err);
        }  else {
            //remove thumbnails in the target dir to refresh them
            removeThumbnails(targetFolder);
            
            //refresh the view
            if (activeWindow == VIEWER) {
                GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(pWindowViewer),GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_INFO, GTK_BUTTONS_OK,"file copied to %s", targetFolder);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
                //we don't do a refreshPhotoSet because copying to the current dir will lead to a replacement of files not a new one
                organizerNeed2BeRefreshed=TRUE;
            } else { //organizer
                //refresh and set the focus
                refreshPhotoArray(TRUE);
                int first=findPhotoFromFullPath(firstFullPath);
                focusIndexPending=first;  //postponed the grab focus-> processed by the next run of scShowAllGtk      
                updateStatusMessage(g_strdup_printf("%i file(s) copied to %s",count, targetFolder));
            }
        }
        g_string_free(fileList, TRUE  );
    }    
}

/*
Move one or several files to another directory (use of cp command)
*/
void moveToDialog(void){
    GError *err = NULL;
    gchar *stdOut = NULL;
    gchar *stdErr = NULL;
    gchar *cmd=NULL;
    gchar *cmd2=NULL;
    int status=0;
    static char *currentFolder = NULL;  //suggest the last move destination to the user
    char *targetFolder=NULL;
    GtkWidget *dialog;
    
    //choose the destination dir
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    gint res;
    dialog = gtk_file_chooser_dialog_new ("Move To",
                                          GTK_WINDOW(pWindowOrganizer),
                                          action,
                                          "Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "Ok",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
    if (currentFolder==NULL) currentFolder=photosRootDir;
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog),currentFolder);
    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT){
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        targetFolder = gtk_file_chooser_get_filename (chooser);
        currentFolder= targetFolder;
        g_print ("  %s",targetFolder);
        //g_free (targetFolder); //mem leak
    }
    if (res == GTK_RESPONSE_CANCEL){
        if (activeWindow == ORGANIZER) updateStatusMessage("Move Canceled");
    }
    gtk_widget_destroy (dialog);
    if (activeWindow == ORGANIZER) updateStatusMessage("Please wait, move is pending...");
    while (gtk_events_pending ()) gtk_main_iteration(); //consume all the events 
    
    //move the selection
    if (targetFolder!=NULL){
        PhotoObj *pPhotoObj;
        int count=0;
        char *firstFullPath=NULL;
        GString *fileList=g_string_new(NULL);
        if (activeWindow == VIEWER){
            g_string_append_printf(fileList," \"%s\"", viewedFullPath);
        }
        if (activeWindow == ORGANIZER){
            for (int i=0;i<photoArray->len;i++){    
                pPhotoObj=g_ptr_array_index(photoArray,i);
                if (pPhotoObj!=NULL && pPhotoObj->selected) {
                    if (count==0) {
                        PhotoObj *firstPhoto;
                        if (i!=0) {
                            firstPhoto=g_ptr_array_index(photoArray,i-1);
                            firstFullPath= g_strdup(firstPhoto->fullPath)   ;
                        }
                    }
                    count++;
                    g_string_append_printf(fileList," \"%s\"", pPhotoObj->fullPath);
                }
            }
            updateStatusMessage(g_strdup_printf("%i file(s) moved to %s",count, targetFolder));
        }
        #ifdef LINUX
        //if file exists in target dir, mv will force the move
        cmd = g_strdup_printf("mv %s \"%s\"",fileList->str, targetFolder);
        g_print("cmd=%s",cmd);
        #elif OSX
        //TODO
        #elif WIN
        //TODO
        #endif
        g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err);
        
        if (err){ 
            g_print ("-error: %s\n", err->message);
            if (activeWindow == VIEWER) {
                GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(pWindowViewer),GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_INFO, GTK_BUTTONS_OK,"error: %s", err->message);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
            } else { //organizer
                updateStatusMessage(g_strdup_printf("error: %s", err->message));
            }
            g_clear_error(&err);
        }  else {
            //remove thumbnails in the target dir to refresh them
            removeThumbnails(targetFolder);
            
            //refresh the view
            if (activeWindow == VIEWER) {
                GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(pWindowViewer),GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_INFO, GTK_BUTTONS_OK,"file moved to %s", targetFolder);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
                moveInViewer();
                organizerNeed2BeRefreshed=TRUE;
            } else { //organizer
                //refresh and set the focus
                refreshPhotoArray(TRUE);
                int first=findPhotoFromFullPath(firstFullPath);
                focusIndexPending=first;  //postponed the grab focus-> processed by the next run of scShowAllGtk      
                updateStatusMessage(g_strdup_printf("%i file(s) moved to %s",count, targetFolder));
            }
        }
        g_string_free(fileList, TRUE  );
    }    
}

/*
remove thumbnails of the selected list in the target directory
if specialFolder is filled, the remove will occur from this folder on the selected photo list
specialFolder is used in the copyto to remove thumbnails in the targetfolder of the copy
*/
void removeThumbnails(char *specialFolder){
    PhotoObj *pPhotoObj;
    if (!specialFolder){
        g_print("no special folder");
        if (activeWindow == VIEWER){
            int idNode=getFileNode(viewedFullPath);
            gchar *thumbnailPath = g_strdup_printf("%s/%i",thumbnailDir,idNode);
            remove(thumbnailPath); //remove the thumbnail
            g_free(thumbnailPath);
        }
        if (activeWindow == ORGANIZER){
            for (int i=0;i<photoArray->len;i++){    
                pPhotoObj=g_ptr_array_index(photoArray,i);
                if (pPhotoObj!=NULL && pPhotoObj->selected) {
                    gchar *thumbnailPath = g_strdup_printf("%s/%i",thumbnailDir,pPhotoObj->idNode);
                    remove(thumbnailPath); //remove the thumbnail
                    g_free(thumbnailPath); 
                }
            }
        }
    }  else {
        if (activeWindow == VIEWER){
            gchar *fileName = g_path_get_basename (viewedFullPath);
            gchar *fullPathInTarget =  g_strdup_printf("%s/%s",specialFolder,fileName);
            int j=getFileNode(fullPathInTarget);
            gchar *thumbnailPath = g_strdup_printf("%s/%i",thumbnailDir,j);
            if (j!=-1) remove(thumbnailPath);
            g_free(fileName);
            g_free(fullPathInTarget);
            g_free(thumbnailPath); 
        }
        if (activeWindow == ORGANIZER){
            for (int i=0;i<photoArray->len;i++){    
                pPhotoObj=g_ptr_array_index(photoArray,i);
                if (pPhotoObj!=NULL && pPhotoObj->selected) {
                    gchar *fileName = g_path_get_basename (pPhotoObj->fullPath);
                    gchar *fullPathInTarget =  g_strdup_printf("%s/%s",specialFolder,fileName);
                    int j=getFileNode(fullPathInTarget);
                    gchar *thumbnailPath = g_strdup_printf("%s/%i",thumbnailDir,j);
                    if (j!=-1) remove(thumbnailPath);
                    g_free(fileName);
                    g_free(fullPathInTarget);
                    g_free(thumbnailPath);        
                }
            }
        }
    } 
}  

/*
show exif data in a dialog (use of jhead command)
if show is FALSE, we don't show the dialog but extract the gpsdata for later use
*/
char * showExifDialog(gboolean show){
    GError *err = NULL;
    gchar *stdOut = NULL;
    gchar *stdErr = NULL;
    gchar *cmd=NULL;
    char *gps=NULL;
    int status;
    char *_fullPath=NULL;
    if (activeWindow == VIEWER)         
        _fullPath=viewedFullPath;
    if (activeWindow == ORGANIZER) {
        int i=whichPhotoHasTheFocus();
        g_print("showExif focus %i\n",i);
        if (i!=-1) {
            PhotoObj *pPhotoObj=g_ptr_array_index(photoArray,i); 
            _fullPath=pPhotoObj->fullPath;
        }       
    }          
    if (_fullPath == NULL) return;
    if (g_str_has_suffix(_fullPath, ".png") || g_str_has_suffix(_fullPath, ".PNG")){
            updateStatusMessage("PNG files'properties are not supported! Use exiftool.");
        return;
    }
    
    #ifdef LINUX
    cmd = g_strdup_printf("jhead \'%s\'",_fullPath);
    //cmd = g_strdup_printf("exiftool \'%s\'",_fullPath); //test with exiftool utilities
    g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err); 
    if (err){
        g_print ("-error: %s\n", err->message);
        updateStatusMessage("Please install jhead with \"sudo apt-get install jhead\" to get this option.");
        g_clear_error(&err);
    } else if (stdErr[0]!='\0'){ 
        updateStatusMessage(g_strdup_printf("error: %s.",stdErr));        
    }  else {
        gps=extractGPS(stdOut);
        char *res;
        if (gps) res=g_strdup_printf("%sGPS (lat,lon): %s",stdOut, gps ); //add lat,lon in one line to ease "copy paste" in google map
        else res=stdOut;
        if (show){
            GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(pWindowOrganizer),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, res, NULL);

            //actions to give messagearea copy/paste skills
            GtkWidget *_messageArea=gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog)); //msgarea is a vbox
            GList *_list=gtk_container_get_children(_messageArea); 
            GtkWidget *_label=g_list_first(_list)->data; //the first child is the text labe
            if (GTK_IS_LABEL(_label)) gtk_label_set_selectable (GTK_LABEL(_label), TRUE); 

            gtk_dialog_run (GTK_DIALOG (dialog));
            gtk_widget_destroy (dialog);
        }
        g_free (stdOut);
    }
    #elif WIN
        // check the following url for windows support
        //https://hpdc-gitlab.eeecs.qub.ac.uk/binqbu2002/uniserver-vm-benchmarks/raw/master/parsec/pkgs/libs/glib/src/tests/spawn-test.c
      g_print ("doesn't support windows for the moment-\n");
    #elif OSX
        //TODO
    #endif
    g_free(cmd);
    if (!show) return gps;
}

/*
extract GPS data and format them for google maps
*/
static char * extractGPS(char *text){
    char *latitude= extractKey(text,"GPS Latitude");
    char *longitude= extractKey(text,"GPS Longitude");
    if (!latitude || !longitude) return NULL;
    //format gps data : chg N 21d  4m  6.2928s in 21°  4'  6.2928" N
    latitude=replaceString(latitude,"d","°");
    latitude=replaceString(latitude,"m","\'");
    latitude=replaceString(latitude,"s","\"");
    char *prefixLat=g_strndup(latitude,2);
    latitude=&latitude[2];// move 2 bytes to drop the N     
    longitude=replaceString(longitude,"d","°");
    longitude=replaceString(longitude,"m","\'");
    longitude=replaceString(longitude,"s","\"");
    char *prefixLon=g_strndup(longitude,2);
    longitude=&longitude[2];// move 2 bytes to drop the N 
    char *res=g_strdup_printf("%s%s,%s%s",latitude,prefixLat,longitude, prefixLon);
    //remove blanks in the result
    char **resSplit=g_strsplit(res," ", -1);
    gchar *_res= g_strjoinv(NULL,resSplit);
    return _res;
}
/*
delete selected photos dialog and processing
*/
void deleteDialog(void){
    GtkWidget *dialog;
    PhotoObj *pPhotoObj;
    int first=-1, count=-1,newFocus=-1,deleteConfirm,removeResult;
    char *message;
    count=howManySelected();
    if (count<= 0) return;
    else if (count==1) message= "Do you really want to delete this file ?";
    else message= "Do you really want to delete those files ?";
    dialog = gtk_message_dialog_new (GTK_WINDOW(pWindowOrganizer), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,"%s", message);
    gtk_widget_grab_focus(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK)); //set default button
    deleteConfirm = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (deleteConfirm==GTK_RESPONSE_OK) { 
        for (int i=0;i<photoArray->len;i++){    
            pPhotoObj=g_ptr_array_index(photoArray,i);
            if (pPhotoObj!=NULL && pPhotoObj->selected) {
                if (first==-1) first=i;
                removeResult=remove(pPhotoObj->fullPath);//remove each file
                gchar *thumbnailPath = g_strdup_printf("%s/%i",thumbnailDir,pPhotoObj->idNode);
                remove(thumbnailPath); //remove the thumbnail
                g_free(thumbnailPath); 
            }
        }    
        if(removeResult==0) {           
            g_print("%i file(s) deleted successfully",count);
            //we calculate the next focus but we set focusIndexPending later via first var update;        
            PhotoObj *firstPhoto = g_ptr_array_index(photoArray,first);
            int row2Scroll =firstPhoto->row-1;
            int col2Scroll= firstPhoto->col-1;
            RowObj *firstRow=g_ptr_array_index(rowArray,row2Scroll);
            if (firstRow->index==-1 && col2Scroll==-1){//if previous row is title and col is zero we try to put the focus in the same position 
                //we don't change the first position which will be the focus
            } else {
                first--; //
            }
            //we refresh all the data
            refreshPhotoArray(TRUE);// FALSE
            //scroll
            /*g_print("\nDeleteDialogrow2Scroll%i",row2Scroll);
            int position=row2Scroll * (PHOTO_SIZE+ MARGIN);
            gtk_adjustment_set_value (scAdjustment, position); */
            RowObj *rowObj=g_ptr_array_index(rowArray,row2Scroll);
            //setfocus and scroll done by changefocus function
            if (first<=photoArray->len-1){
                if (!rowObj->loaded==TRUE)
                    focusIndexPending=first;  //postponed the grab focus-> processed by the next run of scShowAllGtk      
                else {
                    g_print("\nDeleteDialogChangeFocus requested");
                    changeFocus(first,TOP,TRUE);
                }
            }
        } else { 
            if (count == 1)  updateStatusMessage(g_strdup_printf("Error: unable to delete the file %s",pPhotoObj->fullPath));
            else  updateStatusMessage("Error: unable to delete some file(s) ");
        }
    }
}

/*
rename directory dialog
*/
void renameFolderDialog(void){
   	GtkWidget *pDialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);	
	gtk_window_set_position(GTK_WINDOW(pDialog), GTK_WIN_POS_CENTER); //default position
    gtk_window_set_default_size(GTK_WINDOW(pDialog), 400, 100); //default size
    // modal window
    gtk_window_set_title (GTK_WINDOW (pDialog), "Rename Folder");	
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pDialog),TRUE);  //to avoid multi windows in the app manager
    gtk_window_set_modal (GTK_WINDOW(pDialog),TRUE);
    gtk_window_set_transient_for (GTK_WINDOW(pDialog), GTK_WINDOW(pWindowOrganizer));

    GtkWidget *pVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(pDialog), pVBox);
    gtk_box_set_homogeneous (GTK_BOX(pVBox), FALSE);
    
    GtkWidget *pHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);    
    GtkWidget *nameLabel=gtk_label_new("Name");
    g_object_set(nameLabel, "margin", 10, NULL);
    gtk_box_pack_start(GTK_BOX(pHBox), nameLabel, FALSE, FALSE, 0); 
    GtkWidget *nameInput=gtk_entry_new();
    g_object_set(nameInput, "margin", 10, NULL);
    //treeIdNodeSelected, treeDirSelected = last selected folder coming from photoorganizer
    gtk_entry_set_text (GTK_ENTRY(nameInput),g_path_get_basename(treeDirSelected)); //default value
    gtk_box_pack_start(GTK_BOX(pHBox), nameInput, TRUE, TRUE, 0); 
    gtk_box_pack_start(GTK_BOX(pVBox), pHBox, FALSE, FALSE, 0);
    
    pHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);    
    GtkWidget *btnRename = gtk_button_new_with_label("Rename");
    g_object_set(btnRename, "margin", 10, NULL);
    gtk_box_pack_start(GTK_BOX(pHBox), btnRename, FALSE, FALSE, 0); 
    gtk_widget_set_halign(pHBox,GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(pVBox), pHBox, FALSE, FALSE, 0);
    
    GPtrArray *widgetArray = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add(widgetArray, nameInput);

    g_signal_connect (G_OBJECT (btnRename),"button-press-event",G_CALLBACK (renameFolderCB), widgetArray); //click
    g_signal_connect (G_OBJECT (btnRename),"key-press-event",G_CALLBACK (renameFolderCB), widgetArray); //click
    
    gtk_widget_show_all(pDialog);
}

/*
rename directory processed with a mv command
*/
static gboolean renameFolderCB (GtkWidget *event_box, GdkEvent *event, gpointer data)  {
    //check click or enter on button
    if ( event->type == GDK_KEY_PRESS){
        GdkEventKey *eventKey=(GdkEventKey *)event;
        if (eventKey->keyval!=GDK_KEY_Return) return FALSE;
    } else if (event->type != GDK_BUTTON_PRESS ) {
        return FALSE; 
    }    

    //format the cmd
    GPtrArray *widgetArray =data;
    GtkWidget *folderInput=g_ptr_array_index(widgetArray,0);
    const gchar *_folder=gtk_entry_get_text(GTK_ENTRY(folderInput));
    gchar *folder=g_strdup(_folder);
    //mv "home/user/Pictures/divers2" "home/user/Pictures/divers"
    char *target = g_strdup_printf("%s/%s",g_path_get_dirname(treeDirSelected),trim(folder)); 
    #if defined(LINUX) || defined(OSX)
    gchar *cmd = g_strdup_printf("mv \"%s\" \"%s\"",treeDirSelected,target);
    g_print("cmd %s",cmd);
    #elif WIN
        //TODO
    #endif
    
    //remove dialogbox
    GtkWidget *pWindow= gtk_widget_get_ancestor (folderInput, GTK_TYPE_WINDOW);
    removeAllWidgets(GTK_CONTAINER(pWindow)); //we clean the content of the window
    gtk_widget_destroy(pWindow);

    //launch the cmd
    GError *err = NULL;
    gchar *stdOut = NULL;
    gchar *stdErr = NULL;
    int status;
    g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err);

    //check the result
    if (status!=0){ 
        updateStatusMessage(g_strdup_printf("error: %s", stdErr));
        g_clear_error(&err);
    }  else {
        int lastIdNode=treeIdNodeSelected;
        refreshPhotoWall(NULL,&lastIdNode);
        updateStatusMessage(g_strdup_printf("Folder renamed!"));
    }
    return FALSE; //to continue handling the event
}

/*
move a folder to another root directory
*/
void moveFolderDialog(void){
    static char *currentFolder = NULL;  //suggest the last copy destination to the user
    char *targetFolder=NULL;
    GtkWidget *dialog;
    
    //choose the destination dir
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    gint res;
    dialog = gtk_file_chooser_dialog_new ("Move Folder",
                                          GTK_WINDOW(pWindowOrganizer),
                                          action,
                                          "Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "Ok",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
    if (currentFolder==NULL) currentFolder=photosRootDir;
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog),currentFolder);
    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT){
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        targetFolder = gtk_file_chooser_get_filename (chooser);
        currentFolder= targetFolder;
        g_print ("  %s",targetFolder);
        //g_free (targetFolder); //mem leak
    }
    if (res == GTK_RESPONSE_CANCEL){
        updateStatusMessage("Move Canceled");
    }
    gtk_widget_destroy (dialog);
    
    //move to target
    if (targetFolder!=NULL){

        //format the cmd
        #if defined(LINUX) || defined(OSX)
        gchar *cmd = g_strdup_printf("mv \"%s\" \"%s/%s\"",treeDirSelected,targetFolder,g_path_get_basename(treeDirSelected));
        g_print("cmd %s",cmd);
        #elif WIN
            //TODO
        #endif

        //launch the cmd
        GError *err = NULL;
        gchar *stdOut = NULL;
        gchar *stdErr = NULL;
        int status;
        g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err);

        //check the result
        if (status!=0){ 
            updateStatusMessage(g_strdup_printf("error: %s", stdErr));
            g_clear_error(&err);
        }  else {
            int lastNode=treeIdNodeSelected;
            //refreshTree(lastNode); //this function will reload the tree and refresh the photos
            refreshPhotoWall(NULL,&lastNode); //this function will reload the tree and refresh the photos
            updateStatusMessage(g_strdup_printf("Folder moved!"));
        }
    }    
}

/*
remove the folder dialog with rmdir command
*/
void deleteFolderDialog(void){
    GtkWidget *dialog;
    //TODO check that we don't delete the last node of the treeview 
   
    int depth=-1;
    GtkTreePath *_treePath=getTreePathFromidNode(treeIdNodeSelected,NULL);
    if (_treePath!=NULL){
        depth=gtk_tree_path_get_depth (_treePath);
        gtk_tree_path_free(_treePath);
    }
    g_print("children:%i-%i-", gtk_tree_model_iter_n_children (pSortedModel,NULL),depth);     
    if (gtk_tree_model_iter_n_children (pSortedModel,NULL)<=1 && depth==1){updateStatusMessage("error: we can't delete this folder we will run out of node in the treeview!");return;}
    dialog = gtk_message_dialog_new (GTK_WINDOW(pWindowOrganizer), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,"Do you really want to delete the folder\n %s ?",treeDirSelected);
    int deleteConfirm = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    if (deleteConfirm==GTK_RESPONSE_OK) { 
        int ret=rmdir(treeDirSelected); //remove directory
        if (ret==-1 && (errno==EEXIST || errno==ENOTEMPTY)){
            updateStatusMessage("error: we couldn't delete the folder because it was not empty");
        } else {
            //find the node to select in the tree
            int idNode=-1;
            gchar *_name, *_fullPathName;
            GtkTreeIter iter;
            GtkTreePath *treePath=getTreePathFromidNode(treeIdNodeSelected,NULL);
            if (gtk_tree_path_prev(treePath)) { //get prev sibling                    
                    gtk_tree_model_get_iter (pSortedModel,&iter,treePath);
                    gtk_tree_model_get(pSortedModel, &iter, BASE_NAME_COL, &_name, ID_NODE, &idNode, FULL_PATH_COL, &_fullPathName, -1);
            }   else  { //get next sibling
                gtk_tree_path_next(treePath);
                if (gtk_tree_model_get_iter (pSortedModel,&iter,treePath)){
                    gtk_tree_model_get(pSortedModel, &iter, BASE_NAME_COL, &_name, ID_NODE, &idNode, FULL_PATH_COL, &_fullPathName, -1);
                } else {                
                    //it means that next sibling has failed, we have to get the parent
                    gchar *parent = g_path_get_dirname(treeDirSelected);
                    idNode=getFileNode(parent);
                }    
                
            } 
            if (treePath!=NULL)  gtk_tree_path_free(treePath);
            g_print("-%i to refresh after delete-",idNode);
            //refresh & select
            refreshPhotoWall(NULL,&idNode);
            updateStatusMessage("folder deleted");
        }
    }
}

/*
create a new folder doalog
*/
void newFolderDialog(void){
    GtkWidget *pDialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);	
	gtk_window_set_position(GTK_WINDOW(pDialog), GTK_WIN_POS_CENTER); //default position
    /*GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);
    int screenWidth = gdk_screen_get_width (screen);
    int screenHeight = gdk_screen_get_height (screen);*/
    int screenWidth=getMonitorWidth(pDialog);
    int screenHeight=getMonitorHeight(pDialog);

    gtk_window_set_default_size(GTK_WINDOW(pDialog), screenWidth*0.5f, screenHeight*0.5f); //default size
    // modal window
    gtk_window_set_title (GTK_WINDOW (pDialog), "New Folder");	
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pDialog),TRUE);  //to avoid multi windows in the app manager
    gtk_window_set_modal (GTK_WINDOW(pDialog),TRUE);
    gtk_window_set_transient_for (GTK_WINDOW(pDialog), GTK_WINDOW(pWindowOrganizer));

    GtkWidget *pVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(pDialog), pVBox);
    gtk_box_set_homogeneous (GTK_BOX(pVBox), FALSE);
    
    GtkWidget *pHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);    
    GtkWidget *nameLabel=gtk_label_new("Folder Name");
    g_object_set(nameLabel, "margin", 10, NULL);
    gtk_box_pack_start(GTK_BOX(pHBox), nameLabel, FALSE, FALSE, 0); 
    GtkWidget *nameInput=gtk_entry_new();
    g_object_set(nameInput, "margin", 10, NULL);
    gtk_entry_set_text (GTK_ENTRY(nameInput),"Untitled Folder"); //default value
    gtk_box_pack_start(GTK_BOX(pHBox), nameInput, TRUE, TRUE, 0); 
    gtk_box_pack_start(GTK_BOX(pVBox), pHBox, FALSE, FALSE, 0);

    GtkWidget *fileChooser= gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(fileChooser), treeDirSelected);
    gtk_box_pack_start(GTK_BOX(pVBox), fileChooser, TRUE, TRUE, 0);
        
    pHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);    
    GtkWidget *btnRename = gtk_button_new_with_label("Create");
    g_object_set(btnRename, "margin", 10, NULL);
    gtk_box_pack_start(GTK_BOX(pHBox), btnRename, FALSE, FALSE, 0); 
    gtk_widget_set_halign(pHBox,GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(pVBox), pHBox, FALSE, FALSE, 0);
    
    GPtrArray *widgetArray = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add(widgetArray, nameInput);
    g_ptr_array_add(widgetArray, fileChooser);
    g_signal_connect (G_OBJECT (btnRename),"button-press-event",G_CALLBACK (newFolderCB), widgetArray); //click
    g_signal_connect (G_OBJECT (btnRename),"key-press-event",G_CALLBACK (newFolderCB), widgetArray); //click
    
    gtk_widget_show_all(pDialog);
}

/*
create a folder processed with mkdir command
*/
static gboolean newFolderCB (GtkWidget *event_box, GdkEvent *event, gpointer data)  {
    //check click or enter on button
    if ( event->type == GDK_KEY_PRESS){
        GdkEventKey *eventKey=(GdkEventKey *)event;
        if (eventKey->keyval!=GDK_KEY_Return) return FALSE;
    } else if (event->type != GDK_BUTTON_PRESS ) {
        return FALSE; 
    }
    //format the cmd
    GPtrArray *widgetArray =data;
    GtkEntry *folderName=g_ptr_array_index(widgetArray,0);
    GtkFileChooser *fileChooser=g_ptr_array_index(widgetArray,1);
    gchar *newFolder=g_strdup_printf("%s/%s",gtk_file_chooser_get_current_folder(fileChooser), gtk_entry_get_text(folderName));    
    #if defined(LINUX) || defined(OSX)
        char *cmd=g_strdup_printf("mkdir \"%s\"",newFolder);    
        g_print("cmd %s",cmd);
    #elif WIN
    //TODO
    #endif

    //launch the cmd
    GError *err = NULL;
    gchar *stdOut = NULL;
    gchar *stdErr = NULL;
    int status;
    g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err);
    
    //remove dialogbox
    GtkWidget *pWindow= gtk_widget_get_ancestor (GTK_WIDGET(folderName), GTK_TYPE_WINDOW);
    removeAllWidgets(GTK_CONTAINER(pWindow)); //we clean the content of the window
    gtk_widget_destroy(pWindow);
    
    //show the result
    if (status!=0){ 
        updateStatusMessage(g_strdup_printf("error: %s", stdErr));
        g_clear_error(&err);
    }  else {
        int idNode=getFileNode(newFolder);
        //refreshTree(idNode); //refresh and select
        refreshPhotoWall(NULL,&idNode); //refresh and select
        updateStatusMessage(g_strdup_printf("Folder created!"));
    }

}