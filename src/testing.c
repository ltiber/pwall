//gcc -D LINUX  -o ../bin/testing tsoft.c testing.c -I. -g -lm `pkg-config --libs --cflags gstreamer-video-1.0 gtk+-3.0 gstreamer-1.0 gstreamer-pbutils-1.0  gstreamer-wayland-1.0`


//testing generation thumbnail video de masse

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <tsoft.h>

//le return c'est pour le cancel 
int followup(gchar *msg){
    g_print(msg);
    g_free(msg);
    return FALSE; 
}

int main(int argc, char *argv[]){
    g_print("testing started");
    gchar *dirPath="/home/leon/Pictures";
    gchar *targetPath="/home/leon/Downloads/testing-pwall";
    //on a adapté createThumbnail4DirInternal pour ne travailler que sur les videos
    //à remettre video et image
    createThumbnail4Dir(dirPath,targetPath,92,followup);
}