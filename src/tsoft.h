//tsoft header

//tsoft toolkit public functions
int  getPhotoSize(const char *filePath, unsigned int *width, unsigned int *height);
int  getPhotoSizePNG(const char *filePath, unsigned int *width, unsigned int *height);
int  getPhotoSizeJPG(const char *filePath, unsigned int *width, unsigned int *height);
int  getPhotoOrientation(const char *fileName);
void removeAllWidgets(GtkContainer *pContainer);
int createThumbnail(const gchar *filePath,const int idNode, const gchar *targetDir, const int size);
int createThumbnail4Dir(const gchar *dirPath,const gchar *targetDir, int size, int (*followupCB)(char *)); //the callback can be null
int countFilesInDir(const gchar *dirPath, const int isImage); //not in subdir
long int getFileTime(const char *path);
long int getFileNode(const char *path); 
GPtrArray *getDirSortedByDate(const gchar *dirPath);
void timeIn(void);
void timeOut(const char * fileName, const char * event);
char *trim(char *s);
char *lower(char *s);
int getMonitorWidth(GtkWidget *widget);
int getMonitorHeight(GtkWidget *widget);
int isInIntegerArray(GArray *intArray, int value);
char *getCanonicalPath(char *path);
char *replaceString(char *orig, char *rep, char *with) ;
char *extractKey(char *text, char *key);




//int (*followupCB)(int);

//tsoft toolkit public Structures/Object/Enum
typedef struct IntObj {
	int value;
} IntObj;

typedef struct LongIntObj {
	long int value;
} LongIntObj;

typedef struct FileObj {
	char * name;
	int idNode;
	long int time;
} FileObj;

enum {ERR_THUMBNAIL_ALREADY_EXIST, ERR_FILE_NOT_VALID, ERR_PIXBUF, ERR_THUMBNAIL_SAVE, PASSED_CREATED};
