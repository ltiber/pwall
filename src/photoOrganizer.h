//photoOrganizer header


//photoOrganizer public Structure/Object/enum

typedef struct PhotoObj {
	int iDir; //inode of the dir which contains the file used for thumbnails
    int col; //col in the photowall
    int row; //row in the photowall
	int hasFocus; //boolean
	int deleted; //if deleted from the app boolean
    int selected; //if the photo is selected boolean, used for multiselection
	int x,y; //location in its pane
	GtkWidget *pFocusBox;
	GtkWidget *pEventBox;
    char *fullPath;
    long int lastModDate; // lastmodification date of the file in time_t format
    int width,height; 	
} PhotoObj;

//used to load the sequentialy the photos in the photowall
typedef struct RowObj {
	int idNode; //disk system directory inode
    int index; //index in the directory of the first photo in the row (if index ==-1 it means that the row displays the directory name)
    int indexInPhotoArray; //location of the first photo of the row in the photoArray. It doesn't mean that photoObj has been already loaded at this place
    int len; //number of photos in the row
	int hasMore; //boolean hasmore files in this dir
    int loaded; //boolean photos are already loaded in the row
} RowObj;

typedef enum ScrollType {
	TOP,BOTTOM
}ScrollType;


//used for the popup to now the origine
typedef enum ActiveWindowType {
    ORGANIZER,VIEWER
} ActiveWindowType;


enum {
    BASE_NAME_COL,
    FULL_PATH_COL,
    ID_NODE,
    COUNTER,
    N_COL
};


//photoOrganizer public function
void photoOrganizerInit(GtkWidget *waitingScreen);
void updateStatusMessage(char *value);
void scroll2Row(int row, ScrollType type);
void changeFocus(int index,ScrollType type, int clearSelection);
void refreshPhotoArray(int _refreshTree);
int findPhotoFromFullPath(char *fullPath);
void refreshThumbnails(void);
int readRecursiveDir(const gchar *dirPath, GtkTreeIter *parent, int reset);
int whichPhotoHasTheFocus(void);
int whichRowHasTheFocus(void);
int howManySelected(void);
void refreshPhotoWall(GtkWidget* widget, gpointer data);
GtkTreePath *getTreePathFromidNode(int idNode, GtkTreeIter *parent);
char *getFullPathFromidNode(int idNode, GtkTreeIter *parent); //idNode of a directory
int getNextDir(int *row);
int getPreviousDir(int *row);
void addCounters2Tree(GtkTreeIter *parent);
int getPhotoIndex(int col, int row);
int getPhotoCol(int index);
int getPhotoRow(int index); 




// photoOrganizer public Variables

extern GPtrArray *photoArray;
extern GPtrArray *rowArray;
extern ActiveWindowType activeWindow;
extern GtkWidget *pMenuPopup;
extern char *photosRootDir;
extern GtkTreeStore *pStore;
extern GtkWidget *pWindowOrganizer;
extern int treeIdNodeSelected;
extern char *treeDirSelected;
extern GtkTreeModel *pSortedModel;
extern GArray *folderNodeArray;
extern GArray *folderCounterArray;
extern int colMax;
extern GtkAdjustment *scAdjustment;
extern int focusIndexPending;
extern int scRowsInPage;


//public constants
extern int PHOTO_SIZE;
extern int MARGIN;

