//multiviewer header

//common type
typedef enum ViewerType {
	PHOTO,VIDEO
}ViewerType;


//public function
void multiViewerInit(GtkWindow *parent, int index, int monitor);
void deleteInViewer(void);
void moveInViewer(void);
void updateStatusMessageViewer(char *value);
int getPrevious(int index);
int getNext(int index);
void multiViewerWidgetDestroy();
int multiViewerWidgetLoad(int index);
void multiViewerWidgetInit(int index);
gboolean multiViewerKeyCB(GtkWidget *widget,  GdkEventKey *event); //we use this callback in photoView.c and videoViewer.c
gboolean multiViewerClickCB (GtkWidget *widget, GdkEventButton *event, gpointer data); //we use this callback in photoView.c and videoViewer.c


//public var
extern char *viewedFullPath;
extern int viewedDirNode; // idNode  Folder for the current photo 
extern GPtrArray *viewedFileSet; //set of files filled with the current folder
extern int viewedIndex;  //index is the place of the photo/video in the CURRENT FOLDER 
extern int organizerNeed2BeRefreshed;
extern GtkWidget *winViewer;
extern GtkWidget *statusMessageViewer;
extern int windowManager;

enum {X11,WAYLAND};