//photoDialogs header

//public function
void changeDateDialog(void);
//char *showExifDialog(gboolean show);
void showPropertiesDialog(void);
char *getGPSData(char *_fullPath);
void copyToDialog(void);
void moveToDialog(void);
void removeThumbnails(char *specialFolder);
void deleteDialog(void);
void renameFolderDialog(void);
void moveFolderDialog(void);
void deleteFolderDialog(void);
void newFolderDialog(void);
gboolean needVideoFlip(char *fullPath);
gboolean hasAudioCodec(char *fullPath);

//public var
//extern int viewedPhotoIndex;
