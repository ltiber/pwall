//photoViewer header

//public function
void photoViewerInit(GtkWindow *parent, int index, int monitor);
void deleteInViewer(void);
void moveInViewer(void);
void updateStatusMessageViewer(char *value);

//public var
extern char *viewedFullPath;
extern int organizerNeed2BeRefreshed;
extern GtkWidget *pWindowViewer;