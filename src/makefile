make:
	gcc -D LINUX  -o ../bin/pwall thumbnail.c tsoft.c photoInit.c photoDialogs.c photoOrganizer.c photoViewer.c -I. -g -lm `pkg-config --libs --cflags gtk+-3.0`
	# even if we have deprecated function gcc compile. -w means no warning are output
	# we have warnings because we want to be compliant with ubuntu 16.04 and 18.04
	# libgtk-3-dev package must be installed
	sudo cp ../bin/pwall ../deb/usr/bin

install:    
	sudo cp ../bin/pwall /usr/bin
	sudo cp -p ../res/pwall/* /usr/share/pwall
    
release:
	sudo dpkg -b ../deb/ ../release/pwall-1.0-1.deb
  
  
  
    # other commands have been used for development and debugging and testing 
    # -lm for math.h 
	# -D LINUX   to test #ifdef LINUX in the code
	# -g for debug with gdb
	# -I. to check all the .h file in the building dir
	# gcc -D LINUX -D X11 -o pepper x11Helper.c thumbnail.c tsoft.c photoInit.c photoOrganizer.c photoViewer.c  -I. `pkg-config --libs --cflags gtk+-3.0 x11` for x11 compilation 
    # sudo ln -s /home/leon/soft/gnome/pwall/pwall /usr/bin    to test all the time the dev version
	# sudo cp /home/leon/soft/gnome/pwall/res/application/pwall.desktop /usr/share/applications   to define the app
	# the app icon must be in /usr/share/icons/hicolor/128x128/apps
    # sudo xdg-icon-resource install --novendor --mode system --size 128 pwall.png pwall
    # dpkg -b deb/ pwall-1.0-1.deb to create a package (to be run in the pwall dir)
    # dpkg -i pwall-1.0-1.deb to install the package
    # Depends: libgtk-3-0 (>= 3.18.9), libgtk-3-common (>= 3.18.9), jhead (>=1:3.00), xdg-utils (>=1.1.1)    ((((to be checked))))
    # tested with ubuntu 18.04 GTK3 3.22.30 16.04 GTK3 3.18.9
    # gdk_display_get_monitor_at_window doesnt exist in 16.04 replaced by gdk_screen_get_monitor_at_window
    # gtk_menu_popup_at_pointer doesnt exist in 16.04 replaced by gtk_menu_popup
    # gtk_menu_popup_at_widget doesnt exist in 16.04


