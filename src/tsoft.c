//tsoft.c
//global functions needed by the application
//basic toolkit
#include <stdlib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <tsoft.h>

/* Error exit handler */
#define ERREXIT(msg)  (exit(0))

//public functions in header file

//private globale values
static int verbose=FALSE;
static gint64 initTime=(gint64)0;
GError *error = NULL;

//private functions
static unsigned int read_2_bytes (FILE * myfile);
static int read_1_byte (FILE * myfile) ;
static void removeAllWidgetsCallback(GtkWidget *pWidget, gpointer data);
static gint compareTimeItems(gconstpointer a,  gconstpointer b);
static int createThumbnail4DirInternal(const gchar *dirPath,const gchar *targetDir, int size, int (*followupCB)(char *), int reset);
static int createThumbnail4Photo(const gchar *filePath,const int iDir, const gchar *targetDir, const int size , const long int time, const gboolean removeExt);
static int createThumbnail4Video(const gchar *filePath,const int iDir, const gchar *targetDir, const int size , const long int time);
static int thumbnailNeedsRefresh(const gchar *filePath,const int iDir, const gchar *targetDir);
static int _countFilesInDir(const gchar *dirPath, const gboolean extSupportedOnly, const gboolean recursive, const gboolean reset);



int  getPhotoSize(const char *filePath, unsigned int *width, unsigned int *height) {
    int ret=getPhotoSizeJPG(filePath, width, height);
    if (!ret) ret=getPhotoSizePNG(filePath, width, height);
    return ret;
}

/*
Retrieve the width and height from header of the jpeg.
We give the int var to be updated through &var1 et &var2.
*/
int  getPhotoSizeJPG(const char *filePath, unsigned int *width, unsigned int *height) {
    timeIn();
    FILE *infile=fopen(filePath, "rb");
    if (!infile) return FALSE;
    //g_print("meta1\n");
    int first = fgetc(infile);
    int second = fgetc(infile);
    //look for starting jpeg section
    if (first != 0xff || second != 0xd8){
        fclose(infile);
        return FALSE;
    }
    int found=FALSE;
    //look for exif segment FFE1 (test 2 bytes consécutifs)
    first = fgetc(infile); 
    int i=0;
    long sectionLength=0;
    int LIMIT=100000; //test au max des 50000 1er char avant d'abandonner
    while(i<LIMIT){
        second=fgetc(infile);        
        if (first == 0xff && second == 0xe1){
            //longueur du segment est dans les 2 prochains bytes
            first=fgetc(infile);
            second=fgetc(infile);
            //g_print("%02X %02X",first,second); //debug
            //concat des 2 bytes exemple 0X69(first) 0X30(second) => 0X6930 
            sectionLength=second + (first << 8);
            //sauter
            //g_print("%ld",sectionLength); //debug
            fseek(infile, sectionLength-2, SEEK_CUR);
            found=TRUE;
            //g_print("meta2 length%ld\n", sectionLength );
            break;
        }
        first=second; //prepare next loop
        if (first==EOF) break;
        i++;
    }
    if (found){
        //get the length of the section and jump it while we have 0xffeN which means a section
        first=fgetc(infile);
        second=fgetc(infile);
        //g_print("afterexifsection :%02X %02X",first,second);
        int j=1;
        while (first == 0xff && second >= 0xe1 && second <=0xe9){
            //g_print("%isection\n",j);
            first=fgetc(infile); //length
            second=fgetc(infile); //length
            sectionLength=second + (first << 8);
            fseek(infile, sectionLength-2, SEEK_CUR);
            first=fgetc(infile);
            second=fgetc(infile); 
            j++;
        }
    } else{
        fseek(infile, 4, SEEK_SET); //s'il n'y a pas d'exif section on retourne en haut du fichier
        first = fgetc(infile); 
        second=fgetc(infile); 
    }
    found=FALSE; //reinit
    //workaround for s7 photos
    //read first and second (attention pas sur que le fseek du dessus soit correct - j'enleverai 2 bytes)
    //si de type 0xffen recup encore longueur et saute de cette longueur -2
    
    //look for the width and height section 
    i=0;
    while(i<LIMIT){        
        //SOF0 SOF2   http://dev.exiv2.org/projects/exiv2/wiki/The_Metadata_in_JPEG_files     
        if ((first == 0xff && second == 0xc0)||(first == 0xff && second == 0xc2)){
            //la hauteur est au byte 4 et 5, la largeur au byte 6 et 7
            fseek(infile, 3, SEEK_CUR);
            first=fgetc(infile);//4
            second=fgetc(infile);//5
            //g_print("height %02X %02X",first,second);            
            *height=second + (first << 8); //concat des 2 bytes exemple 0X69(first) 0X30(second) => 0X6930 
            first=fgetc(infile);//6
            second=fgetc(infile);//7 
            *width=second + (first << 8);  
            //g_print("width %02X %02X",first,second);            
            found=TRUE;
            if (verbose) g_print("width %i,height %i\n",*width,*height);
            break;
        }
        first=second; //prepare next loop
        second=fgetc(infile); //fix for instagram photos
        if (first==EOF) break;
        i++;
    }

    fclose(infile);

    if (verbose) {
        gchar *fileName = g_path_get_basename (filePath);
        g_print("%s width %i,height %i\n",filePath,*width,*height);
        timeOut(fileName,"getImageSize");
        g_free(fileName);
    }
    return found;
}

/* Read one byte, testing for EOF */
static int read_1_byte (FILE * myfile) {
  int c;
  c = getc(myfile);
  if (c == EOF)    ERREXIT("Premature EOF in JPEG file");
  return c;
}

/* Read 2 bytes, convert to unsigned int */
/* All 2-byte quantities in JPEG markers are MSB first */
static unsigned int read_2_bytes (FILE * myfile) {
  int c1, c2;
  c1 = getc(myfile);
  if (c1 == EOF)    ERREXIT("Premature EOF in JPEG file");
  c2 = getc(myfile);
  if (c2 == EOF)    ERREXIT("Premature EOF in JPEG file");
  return (((unsigned int) c1) << 8) + ((unsigned int) c2);
}

/*
Retrieve the size in the header of the png file
*/
int  getPhotoSizePNG(const char *filePath, unsigned int *width, unsigned int *height) {
    timeIn();
    FILE *infile=fopen(filePath, "rb");
    if (!infile) return FALSE;
    //g_print("meta1\n");
    int first = fgetc(infile);
    int second = fgetc(infile);
    int third = fgetc(infile);
    //look for starting png section
    if (first != 0x89 || second != 0x50 || third !=0x4E ){ //do we have .PN at the beginning of the file?
        fclose(infile);
        return FALSE;
    }
    fseek(infile, 15, SEEK_CUR); //index 18,19  starting index at 0  (18-2-1 ) -2 car on a déjà lu 3 bytes mais on commence à 0
    first=fgetc(infile);//18
    second=fgetc(infile);//19
    //g_print("width %02X %02X",first,second);            
    *width=second + (first << 8); //concat des 2 bytes exemple 0X69(first) 0X30(second) => 0X6930 
    fseek(infile, 2, SEEK_CUR); //index 22,23 (22-19-1)
    first=fgetc(infile);//22
    second=fgetc(infile);//23 
    *height=second + (first << 8);           
    //g_print("height %02X %02X",first,second);            
    if (verbose) g_print("width %i,height %i\n",*width,*height);
    fclose(infile);

    if (verbose) {
        gchar *fileName = g_path_get_basename (filePath);
        timeOut(fileName,"getImageSizePNG");
        g_free(fileName);
    }
    return TRUE;
}
 /*
 We only handle the 3 6 8 for rotation otherwise we need symmetry
 if (j==3) dst1=gdk_pixbuf_rotate_simple(dst,GDK_PIXBUF_ROTATE_UPSIDEDOWN); //rotate 180
 if (j==6) dst1=gdk_pixbuf_rotate_simple(dst,GDK_PIXBUF_ROTATE_CLOCKWISE); //rotate 90 clockwise
 if (j==8) dst1=gdk_pixbuf_rotate_simple(dst,GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE); //rotate 90 counterclockwise
*/
int getPhotoOrientation(const char *fileName){
    FILE * myfile;
    unsigned char exif_data[65536L];
    int n_flag, set_flag;
    unsigned int length, i;
    int is_motorola; /* Flag for byte order */
    unsigned int offset, number_of_tags, tagnum;

    if ((myfile = fopen(fileName, "rb")) == NULL) {
      g_print("can't open %s\n", fileName);
      return -1;
    }
    
    /* Read File head, check for JPEG SOI + Exif APP1 */
    for (i = 0; i < 4; i++)    exif_data[i] = (unsigned char) read_1_byte(myfile);
    if (exif_data[0] != 0xFF ||    exif_data[1] != 0xD8 ||    exif_data[2] != 0xFF ||    exif_data[3] != 0xE1)
    {fclose(myfile); return -1;}

    /* Get the marker parameter length count */
    length = read_2_bytes(myfile);
    /* Length includes itself, so must be at least 2 */
    /* Following Exif data length must be at least 6 */
    if (length < 8)   {fclose(myfile); return -1;}
    length -= 8;

      /* Read Exif head, check for "Exif" */
  for (i = 0; i < 6; i++) exif_data[i] = (unsigned char) read_1_byte(myfile);
  if (exif_data[0] != 0x45 ||      exif_data[1] != 0x78 ||      exif_data[2] != 0x69 ||      exif_data[3] != 0x66 ||      exif_data[4] != 0 ||      exif_data[5] != 0)    {fclose(myfile); return -1;}

  /* Read Exif body */
  for (i = 0; i < length; i++)    exif_data[i] = (unsigned char) read_1_byte(myfile);
  if (length < 12) {fclose(myfile); return -1;} /* Length of an IFD entry */

  /* Discover byte order */
  if (exif_data[0] == 0x49 && exif_data[1] == 0x49)    is_motorola = 0;
  else if (exif_data[0] == 0x4D && exif_data[1] == 0x4D)    is_motorola = 1;
  else {fclose(myfile); return -1;}

  /* Check Tag Mark */
  if (is_motorola) {
    if (exif_data[2] != 0) {fclose(myfile); return -1;}
    if (exif_data[3] != 0x2A) {fclose(myfile); return -1;}
  } else {
    if (exif_data[3] != 0) {fclose(myfile); return -1;}
    if (exif_data[2] != 0x2A) {fclose(myfile); return -1;}
  }

  /* Get first IFD offset (offset to IFD0) */
  if (is_motorola) {
    if (exif_data[4] != 0) {fclose(myfile); return -1;}
    if (exif_data[5] != 0) {fclose(myfile); return -1;}
    offset = exif_data[6];
    offset <<= 8;
    offset += exif_data[7];
  } else {
    if (exif_data[7] != 0) {fclose(myfile); return -1;}
    if (exif_data[6] != 0) {fclose(myfile); return -1;}
    offset = exif_data[5];
    offset <<= 8;
    offset += exif_data[4];
  }
  if (offset > length - 2) {fclose(myfile); return -1;} /* check end of data segment */

  /* Get the number of directory entries contained in this IFD */
  if (is_motorola) {
    number_of_tags = exif_data[offset];
    number_of_tags <<= 8;
    number_of_tags += exif_data[offset+1];
  } else {
    number_of_tags = exif_data[offset+1];
    number_of_tags <<= 8;
    number_of_tags += exif_data[offset];
  }
  if (number_of_tags == 0) {fclose(myfile); return -1;}
  offset += 2;

  /* Search for Orientation Tag in IFD0 */
  for (;;) {
    if (offset > length - 12) {fclose(myfile); return -1;} /* check end of data segment */
    /* Get Tag number */
    if (is_motorola) {
      tagnum = exif_data[offset];
      tagnum <<= 8;
      tagnum += exif_data[offset+1];
    } else {
      tagnum = exif_data[offset+1];
      tagnum <<= 8;
      tagnum += exif_data[offset];
    }
    if (tagnum == 0x0112) break; /* found Orientation Tag */
    if (--number_of_tags == 0) {fclose(myfile); return -1;}
    offset += 12;
  }
  
      /* Get the Orientation value */
    if (is_motorola) {
      if (exif_data[offset+8] != 0) {fclose(myfile); return -1;}
      set_flag = exif_data[offset+9];
    } else {
      if (exif_data[offset+9] != 0) {fclose(myfile); return -1;}
      set_flag = exif_data[offset+8];
    }
    if (set_flag > 8) {fclose(myfile); return -1;}
    if (verbose) g_print("orientation %i",set_flag);
    
    fclose(myfile);
    return set_flag;
}

//remove container's children
void removeAllWidgets(GtkContainer *pContainer){
    gtk_container_foreach (pContainer, removeAllWidgetsCallback, pContainer);
}

static void removeAllWidgetsCallback(GtkWidget *pWidget, gpointer data){
  GtkContainer *pContainer=data;
  if (GTK_IS_CONTAINER(pWidget))  {
    if (verbose) g_print("container %p\n", pWidget);       
    removeAllWidgets(GTK_CONTAINER(pWidget)); //remove children
    gtk_widget_destroy(pWidget); //remove the widget
    }
  else {
    //gtk_container_remove (pContainer, pWidget);
    if (verbose) g_print("remove %p\n", pWidget); 
    gtk_widget_destroy(pWidget); 
  }        
}

/*
To use with timout, it calculate the difference time
*/
void timeIn(void){
    initTime=g_get_monotonic_time ();        
}

void timeOut(const char * fileName, const char * event){
    gint64 now=g_get_monotonic_time ();        
    int inter=(now-initTime) / 1000 ; //en ms
    g_print("%s-%s %ims\n",fileName,event,inter);
}

//public function
int createThumbnail4Dir(const gchar *dirPath,const gchar *targetDir, int size, int (*followupCB)(char *)){
    return createThumbnail4DirInternal(dirPath, targetDir, size, followupCB, TRUE);
}
//createThumbnails for a directory
static int createThumbnail4DirInternal(const gchar *dirPath,const gchar *targetDir, int size, int (*followupCB)(char *), int reset){
    static int counter=0;
    static int cancel=FALSE;
    if (reset){
        counter=0;
        cancel=FALSE;
    }
    DIR *directory = opendir(dirPath); //open the current dir
    struct dirent *pFileEntry;    
    if(directory != NULL) {
      int iDir = getFileNode(dirPath); // get the inode of the folder
      while((pFileEntry = readdir(directory)) != NULL)   {
        if (cancel) {
            g_print("cancel caught at %s", pFileEntry -> d_name);
            break; //interrupt the process
        }
        char *fileName = pFileEntry -> d_name; // Get filename
        if (g_strcmp0(fileName,"..") == 0 || g_strcmp0(fileName,".") == 0) continue; //avoid the . and .. dirs
        if (pFileEntry->d_type==DT_DIR) {
            //recursivité
            //g_print("isDir\n");
            char *_dirPath=g_strdup_printf ("%s/%s",dirPath,fileName);
            createThumbnail4DirInternal(_dirPath, targetDir, size, followupCB, FALSE);
            g_free (_dirPath);
        } else { //if not needed if (pFileEntry->d_type==DT_REG || pFileEntry->d_type==DT_LNK)
            //g_print("isFile\n");
            
            if (!isHiddenFile(pFileEntry -> d_name) && (hasPhotoExt(pFileEntry -> d_name) || hasVideoExt(pFileEntry -> d_name)) ) {
                //g_print("isJPEG\n");
                counter++;
                gchar *filePath =g_strdup_printf ("%s/%s",dirPath,fileName);
                gchar *msg=g_strdup_printf ("%s - %i completed" ,filePath,counter);
                if (followupCB!=NULL && counter%5==0) {
                    cancel=followupCB(msg);
                    // free(msg) must be done in followupCB later in the gtk thread
                } 
                long int time = getFileTime(filePath);
                int retCreate = createThumbnail(filePath,iDir,targetDir, size, time);
                //if (retCreate == ERR_FILE_NOT_VALID) cancel = TRUE; //we interrupt all the process for debugging
                g_free (filePath); 
            }
        }
      }
      closedir(directory);
    }
    return counter;
}

/*
hidden file start with .
*/
int isHiddenFile(const gchar *name){
    return g_str_has_prefix(name,".");
}

/*
has file extension jpg jpeg png
*/
int hasPhotoExt(const gchar *name){
    if (g_str_has_suffix(name, ".jpg") || g_str_has_suffix(name, ".JPG") || g_str_has_suffix(name, ".jpeg") || g_str_has_suffix(name, ".JPEG") || g_str_has_suffix(name, ".png") || g_str_has_suffix(name, ".PNG"))
        return TRUE; else return FALSE;
}

/*
has EXIF extension only jpeg files
*/
int hasExifExt(const gchar *name){
    if (g_str_has_suffix(name, ".jpg") || g_str_has_suffix(name, ".JPG") || g_str_has_suffix(name, ".jpeg") || g_str_has_suffix(name, ".JPEG") )
        return TRUE; else return FALSE;
}


/*
has file extension jpg jpeg png
*/
int hasVideoExt(const gchar *name){
    if (g_str_has_suffix(name, ".mp4") || g_str_has_suffix(name, ".MP4") ||
        g_str_has_suffix(name, ".mpg") || g_str_has_suffix(name, ".MPG") || g_str_has_suffix(name, ".mpeg") || g_str_has_suffix(name, ".MPEG") || 
        g_str_has_suffix(name, ".avi") || g_str_has_suffix(name, ".AVI") ||
        g_str_has_suffix(name, ".mov") || g_str_has_suffix(name, ".MOV"))
        return TRUE; else return FALSE;
}


int countFilesInDir(const gchar *dirPath, const gboolean extSupportedOnly, const gboolean recursive){
    return _countFilesInDir(dirPath,extSupportedOnly,recursive,TRUE);
}
/*
Count the number of files in a directory (count in sub-directory in recursive is TRUE).
extSupportedOnly means that we count only file that have photo or video extension: jpg, png, mp4...
We don't count hidden files
*/
static int _countFilesInDir(const gchar *dirPath, const gboolean extSupportedOnly, const gboolean recursive, const gboolean reset){
    static int counter=0;
    if (reset) counter=0;
    DIR *directory = opendir(dirPath); //open the current dir
    struct dirent *pFileEntry;
    if(directory != NULL) {
      while((pFileEntry = readdir(directory)) != NULL)   {
        if (g_strcmp0(pFileEntry -> d_name,"..") == 0 || g_strcmp0(pFileEntry -> d_name,".") == 0) continue;
        if (extSupportedOnly && !hasPhotoExt(pFileEntry -> d_name) && !hasVideoExt(pFileEntry -> d_name)) continue;
        if (pFileEntry->d_type==DT_DIR) { 
            if (recursive){
                char *_dirPath=g_strdup_printf ("%s/%s",dirPath, pFileEntry->d_name);
                _countFilesInDir(_dirPath, extSupportedOnly, recursive,FALSE);
                g_free (_dirPath);
            } else {
                continue;
            }
        }  else {
            counter++;
        }
      }
      closedir(directory);
    }
    return counter;
}

/*
size ex:92
iDir is the inode of the dir part of the file
Thumbnails will be stored in ~/.pwall/mini/%iDir%/%fileName%
*/
int createThumbnail(const gchar *filePath, const int iDir, const gchar *targetDir, const int size, const long int time){
    gchar *fileName = g_path_get_basename (filePath);
    if (!thumbnailNeedsRefresh(filePath,iDir,targetDir))  return ERR_THUMBNAIL_ALREADY_EXIST; 
    if (hasPhotoExt(fileName)) return createThumbnail4Photo(filePath,iDir, targetDir, size, time, FALSE);
    if (hasVideoExt(fileName)) return createThumbnail4Video(filePath,iDir, targetDir, size, time);
    return ERR_FILE_NOT_VALID;
}

/*
if photoLastModified<=thumbnailLastModified we don't need to create new thumbnail
*/
static int thumbnailNeedsRefresh(const gchar *filePath, const int iDir, const gchar *targetDir){
    //check if thumbnail already exists
    gchar *targetFilePath =getThumbnailPath(targetDir,iDir,filePath);
    GdkPixbufLoader *loader=NULL;
    long int thumbnailLastModified=getFileTime(targetFilePath);
    if (thumbnailLastModified!=-1){ //thumbnail exists
        long int photoLastModified=getFileTime(filePath);
        if (photoLastModified!=thumbnailLastModified) {
            //g_print("thumbnail already exist for %s\n",filePath);
            g_free(targetFilePath);       
            return FALSE;//if photoLastModified<=thumbnailLastModified we don't create new thumbnail
        }
    }
    g_free(targetFilePath);        
    return TRUE;
}
    

static int createThumbnail4Photo(const gchar *filePath, const int iDir, const gchar *targetDir, const int size, const long int time, const gboolean removeExt){
    g_print("\ncreateThumbnail4Photo started\n");
    gchar *fileName = g_path_get_basename (filePath);
    gchar *targetSubDir=g_strdup_printf ("%s/%i",targetDir,iDir);
    if (getFileNode(targetSubDir)==-1) mkdir(targetSubDir, 0775); // read, no write for others
    gchar *_fileName;//with no extension
    gchar *targetFilePath;
    if (removeExt) {
        _fileName=g_strndup(fileName,strlen(fileName)-4);
        targetFilePath =g_strdup_printf ("%s/%s",targetSubDir,_fileName);
    } else {
        targetFilePath =g_strdup_printf ("%s/%s",targetSubDir,fileName);
    }
    GdkPixbufLoader *loader=NULL;
    int compX=-1,compY=-1,cropX=0,cropY=0;
    float ratio=1.0;
    //calculate width and height compression values
    int x=0,y=0;
    int isPhoto=getPhotoSize(filePath,&x,&y);
    if (!isPhoto) {g_print("%s is not a valid JPG or PNG",filePath); return ERR_FILE_NOT_VALID;} 
    if (x>y) {
        compY=size;
        ratio=(float)x/y; //1 float is enough pour do the operation in float
        compX=size * ratio +1; //round to the upper int
    } else if (x==y) {
        compX=size;
        compY=size;        
    } else {
        compX=size;
        ratio=(float)y/x; 
        compY=size * ratio +1; 
    }
    
    //calculate cropX cropY
    if (compY>compX) {
           int diff=compY-compX;
           cropY=diff/2;   //round to lower int
           cropX=0; 
    } else {
           int diff=compX-compY;
           cropX=diff/2;   
           cropY=0; 
    }
    if (verbose) g_print("%s width %i height %i - resize%i,%i - crop%i,%i\n", filePath, x,y,compX, compY, cropX, cropY);
    
    // load the image from a filePath
    timeIn();
    GError *error = NULL;
    GdkPixbuf *src = gdk_pixbuf_new_from_file_at_size(filePath, compX, compY, &error);
    if(error)   {
        //g_print("Error for file %s .gdk_pixbuf_new_from_file_at_size failed : %s\n", filePath, error->message);
        g_print("pixbuf loader needed");
        g_clear_error(&error);
        //if file is too large files, gdk_pixbuf_new_from_file_at_size can fail
        //we give a second chance with GdkPixbufLoader
        GFile *_file=g_file_new_for_path(filePath);
        error=NULL;
        GFileInputStream *input_stream = g_file_read (_file, NULL, &error);
        loader=gdk_pixbuf_loader_new();
        gdk_pixbuf_loader_set_size(loader,compX, compY); 
        guchar *buffer = g_new0 (guchar, 65535);
	    goffset bytes_read, bytes_read_total = 0;
        while (TRUE) {
            bytes_read = g_input_stream_read (G_INPUT_STREAM (input_stream), buffer, 65535, NULL, &error);
            if  (bytes_read==0) break;
            gdk_pixbuf_loader_write (loader, buffer, bytes_read, &error);
            bytes_read_total+=bytes_read;
        }
        //g_print("totalbytesread %i",bytes_read_total);
        src=gdk_pixbuf_loader_get_pixbuf (loader);
        if (src==NULL || error){
            g_print ("-error: %s\n", error->message);
            g_clear_error(&error);
            g_free(fileName);
            if (removeExt) g_free(_fileName);
            g_free(targetSubDir);
            g_free(targetFilePath);
            gdk_pixbuf_loader_close (loader, &error);
            g_object_unref(loader);
            g_free (buffer);
            g_object_unref (G_OBJECT (input_stream));
            return ERR_PIXBUF;
        }
        g_free (buffer);
        g_object_unref (G_OBJECT (input_stream));
    }
    if (verbose) {
        int width = gdk_pixbuf_get_width (src);
        int height = gdk_pixbuf_get_height (src);
        g_print("pixbuf loaded at height %i, width %i",height,width);
    } 
    if (verbose) timeOut(fileName,"loadFromDiskatSize");
 
     //crop the square image
    timeIn();
    GdkPixbuf *dst = gdk_pixbuf_new_subpixbuf (src,cropX,cropY,size,size);
    if (verbose) timeOut(fileName,"crop");

 
     //rotate the image
    int j=getPhotoOrientation(filePath);
    GdkPixbuf *dst1=NULL;   //We only handle the 3 6 8 for rotation otherwise we need symmetry
    if (j==3) dst1=gdk_pixbuf_rotate_simple(dst,GDK_PIXBUF_ROTATE_UPSIDEDOWN); //rotate 180
    if (j==6) dst1=gdk_pixbuf_rotate_simple(dst,GDK_PIXBUF_ROTATE_CLOCKWISE); //rotate 90 clockwise
    if (j==8) dst1=gdk_pixbuf_rotate_simple(dst,GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE); //rotate 90 counterclockwise
    
    //save to file system to current dir   
    timeIn();
    if (dst1)
        gdk_pixbuf_save (dst1, targetFilePath, "jpeg", &error, "quality", "100", NULL);
    else
        gdk_pixbuf_save (dst, targetFilePath, "jpeg", &error, "quality", "100", NULL);
    if (verbose) timeOut(fileName,"saveToDisk");

    //change modification time
    setFileTime(targetFilePath,time);

    //release pointers
    g_free(fileName);
    if (removeExt) g_free(_fileName);
    g_free(targetSubDir);
    g_free(targetFilePath);  
    g_object_unref(src); 
    g_object_unref(dst);
    if (dst1) g_object_unref(dst1); 
    if (loader) {
        gboolean _close=gdk_pixbuf_loader_close (loader, NULL);
        if(_close) g_object_unref(loader);
    }   
    if(error)    {
        g_warning("save thumbnail failed with error: %s\n", error->message);
        g_clear_error(&error);
        return ERR_THUMBNAIL_SAVE;
    }
    g_print("Thumbnail created for %s\n",filePath);
    return PASSED_CREATED;
}

static int createThumbnail4Video(const gchar *filePath,const int iDir, const gchar *targetDir, const int size, const long int time){
    //we first extract an image from the video we put in a tmp dir
    gchar *fileName = g_path_get_basename (filePath);
    gchar *targetSubDir=g_strdup_printf ("%s/tmp",targetDir);
    if (getFileNode(targetSubDir)==-1) mkdir(targetSubDir, 0775); // read, no write for others

    gchar *targetFilePathTmp =g_strdup_printf ("%s/%s.jpg",targetSubDir,fileName); //we add a.jpg extension

 //   gchar *targetFilePathTmp =g_strdup_printf ("%s/%i.jpg",targetDir,idNode);
    //at first we used ffmpegthumbnailer to create a new thumbnail but we ran into errors for compressed video
    //char *cmd=g_strdup_printf("ffmpegthumbnailer -i \"%s\" -o \"%s\" -s %i",filePath,targetFilePathTmp,size*2); //Error: decodeVideoFrame() failed: frame not finished
    
    //use better ffmpeg
    //ffmpeg -loglevel panic -y -ss 00:00:01 -i test_960.mp4 -vframes 1 -vf scale=w=184:h=184:force_original_aspect_ratio=decrease GOPR0663_small_mini.jpg
    char *cmd=g_strdup_printf("ffmpeg -loglevel panic -y -ss 00:00:01 -i \"%s\" -vframes 1 -vf scale=w=%i:h=%i:force_original_aspect_ratio=decrease \"%s\"",filePath,size*2,size*2,targetFilePathTmp);
    
    g_print("\nvideo %s\n",cmd);
    GError *err = NULL;    gchar *stdOut = NULL;    gchar *stdErr = NULL;    int status;
    g_spawn_command_line_sync (cmd, &stdOut, &stdErr, &status, &err);
    if (err){
        g_print ("-error message: %s\n", err->message);
        g_clear_error(&err);
        return ERR_FFMPEGTHUMBNAILER_DOESNT_EXIST;       
    } else if (stdErr[0]!='\0' && !strstr(stdErr,"deprecated")){ //deprecated is accepted         
        g_print("-error stderr: %s\n",stdErr); 
        //check if the tmp file has been created anyway
        struct stat attr;
        if (stat(targetFilePathTmp, &attr)>=0)
            g_print("%s created",targetFilePathTmp);        
        else 
            return ERR_FILE_NOT_VALID;
    } else {//done
        g_print("%s created",targetFilePathTmp);    
    }
    g_free(cmd);
    //we use createThumbnail4Photo to crop and resize the targetFilePathTmp //
    int ret= createThumbnail4Photo(targetFilePathTmp, iDir, targetDir,size, time, TRUE);
    remove (targetFilePathTmp); //we remove the temp file
    return ret;
}
/*
Last mod date of a file : number from 1970/01/01
*/
long int getFileTime(const char *path) {
    struct stat attr;
    if (stat(path, &attr)>=0) { //check no errors with stat function
        //long int x = mktime(localtime(&attr.st_mtime)); //mktime convert a time to a number from 1970/01/01 (time_t)
        //check conversion is correct
        //char _date[20];
        //strftime(_date, 20, "%Y-%m-%d %H:%M:%S", localtime(&x)); //localtime create a new tm struc which represent a time 
        //g_print("%s",_date);
        return attr.st_mtime;
        //strftime(date, 20, "%d-%m-%y", localtime(&(attrib.st_ctime)));
        //printf("Last modified time: %s", ctime(&attr.st_mtime));
    } else return -1;
}

/*
change modification time to the transmitted time
change the accesstime to NOW
*/
void setFileTime(const char *path, long int _time){
    struct utimbuf new_times;
    new_times.actime = time(NULL); // access time changed to now //
    new_times.modtime = _time;    // modification time is changed //
    utime(path, &new_times);   
}

int getFileNode(const char *path) {
    struct stat attr;
    if (stat(path, &attr)>=0) { //check no errors with stat function
        int x = attr.st_ino; 
        return x;
    } else return -1;
}

/*
size in MB
*/
float getFileSize(const char *path) {
    struct stat attr;
    if (stat(path, &attr)>=0) { //check no errors with stat function
        long int x = attr.st_size; 
        return (float)x/1000000;
    } else return -1;
}



//Not recursive
GPtrArray *getDirSortedByDate(const gchar *dirPath){
    GPtrArray *array=g_ptr_array_new_with_free_func (g_free);
    DIR *directory = opendir(dirPath); //open the current dir
    struct dirent *pFileEntry;
    if(directory != NULL) {
      while((pFileEntry = readdir(directory)) != NULL)   {
        char *fileName = pFileEntry -> d_name; // Get filename
        int idNode=pFileEntry->d_ino;
        //if (g_strcmp0(fileName, "IMG_2532.JPG")==0) g_print("debug getDir IMG_2532-idnode%i\n", idNode);
        if (g_strcmp0(fileName,"..") == 0 || g_strcmp0(fileName,".") == 0) continue;
        if (pFileEntry->d_type==DT_REG || pFileEntry->d_type==DT_LNK){
            
            if (!isHiddenFile(pFileEntry -> d_name) && (hasPhotoExt(pFileEntry -> d_name) || hasVideoExt(pFileEntry -> d_name))) {   
                FileObj *pFileObj=malloc(sizeof(FileObj));
                pFileObj->name=g_strdup_printf("%s",fileName);
                pFileObj->idNode=idNode;

                gchar *filePath =g_strdup_printf ("%s/%s",dirPath,fileName);
                pFileObj->time=getFileTime(filePath);
                g_free (filePath);
                g_ptr_array_add (array, pFileObj); 
            }
        }
      }
      closedir(directory);
      
      //we sort the array
      g_ptr_array_sort (array, compareTimeItems);
      
      return array;
      
    } else return NULL;
}

//descending sorting
static gint compareTimeItems(gconstpointer a,  gconstpointer b){
  FileObj *alpha = *(FileObj **) a; //pointer of pointer
  FileObj *beta = *(FileObj **) b;
  return (gint) (beta->time - alpha->time );
}

char *trim(char *s) {
    char *ptr;
    if (!s)
        return NULL;   // handle NULL string
    if (!*s)
        return s;      // handle empty string
    for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

char *lower(char *s){
    //g_print("before lower %s", s );
    //for ( ; *s; ++s) *s = tolower(*s);
    for(int i = 0; s[i]; i++){
        s[i] = tolower(s[i]);
    }
    //g_print("After lower %s", s );
    return s;
}
/*
widget needs to be a realized window
we used gdk_screen_get_monitor_at_window to be compliant with gtk-3.18
*/
int getMonitorWidth(GtkWidget *widget){        
        
        GdkScreen *scr = gtk_widget_get_screen(widget);
        GdkWindow *win = gtk_widget_get_window(widget);
        /*GdkDisplay *dpy = gtk_widget_get_display(widget);
        GdkMonitor *_monitor = gdk_display_get_monitor_at_window(dpy, win);
        int scale=gdk_monitor_get_scale_factor(_monitor);
    g_print("scale%i",scale);*/
        int monitor = gdk_screen_get_monitor_at_window(scr, win); 
        GdkRectangle geometry;
        //gdk_monitor_get_geometry(monitor, &geometry);
        gdk_screen_get_monitor_geometry(scr,monitor,&geometry);
        return geometry.width;
}
/*
widget needs to be a realized window
we used gdk_screen_get_monitor_at_window to be compliant with gtk-3.18
*/
int getMonitorHeight(GtkWidget *widget){
        //GdkDisplay *dpy = gtk_widget_get_display(widget);
        GdkScreen *scr = gtk_widget_get_screen(widget);
        GdkWindow *win = gtk_widget_get_window(widget);
        //GdkMonitor *monitor = gdk_display_get_monitor_at_window(dpy, win);
        int monitor = gdk_screen_get_monitor_at_window(scr, win); 
        GdkRectangle geometry;
        //gdk_monitor_get_geometry(monitor, &geometry);
        gdk_screen_get_monitor_geometry(scr,monitor,&geometry);
        return geometry.height;
}
/*
Look for the number in the array 
*/
int isInIntegerArray(GArray *intArray, int value){
    if (intArray==NULL) return FALSE;
    int ret=FALSE;    
      for (int i=0;i<intArray->len;i++){
        int j = g_array_index (intArray, int, i);
        if (j == value){
            ret=TRUE;
            break;
        }
    }
    return ret;
}
/*get the full absolute path*/
char *getCanonicalPath(char *path){
    char actualpath [4092];
    return realpath(path, actualpath);
}

/*replace a string by another one*/
char *replaceString(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

/*
Get the fullpath to access to the thumbnail
It has the following schema : ~/.pwall/mini/%iDir%/%basename%
*/
gchar *getThumbnailPath(const gchar *_thumbnailDir, const int iDir, const gchar *fullPathFile){
    gchar *basename=g_path_get_basename(fullPathFile);
    gchar *ret=g_strdup_printf("%s/%i/%s",_thumbnailDir,iDir,basename);
    g_free(basename);
    return ret;
}
/*
extract key value from a list of key value in string
example of string key1=value1\nkey2=value2
*/
char *extractKey(char *text, char *key){
    if (!text || !key) return NULL;
	char *ptr = strstr(text, key);
    if (!ptr) return NULL;
	char *res, *tmp ;
	gboolean read=TRUE;
    gboolean found=FALSE;
	int i=0, start=0, len=0;
	while (read){
		if (!found && ptr[i]==':') {found=TRUE;start=i+1;}
		if (found && (ptr[i]=='\0' ||ptr[i]=='\n' )) {read=FALSE;len=i-start;tmp=&ptr[start]; res=strndup(tmp,len);}
 		i++;
	}
	return (found)?res:NULL;
}
