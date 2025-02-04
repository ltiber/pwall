//photoInit.h

//public variable
extern char *thumbnailDir; 
extern char *appDir;
extern char *resDir;
extern int photosRootDirCounter;
extern GtkApplication *app;

//public function
void loadTreeModel(void);
void helpCB (GSimpleAction *action, GVariant *parameter, gpointer user_data);
void initPrimaryMenu(GtkWidget *menuButton);
