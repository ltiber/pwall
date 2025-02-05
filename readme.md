
pwall: browse your photos/videos
================================

![pwall icon](https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/pwall.png)

pwall is modern style photo/video viewer and organizer.\
It shows the folders of your photos/videos main directory
in the left pane and a photowall in the right.
The photowall is a scrolling panel of square thumbnails
giving a preview of each photo/video.\
Loading and unloading of this 
scrolling panel is made dynamicaly, viewing page by viewing page
like smartphones do.\
It also allows you to have basics management of photos/videos
(change dates, copy a group of photos to another directory, delete, share, get GPS data...).
Clicking on a photo/video opens it in fullscreen mode. You can continue your navigation in this mode with left and right keys. 
I use it as an alternative to Shotwell.[more...](https://htmlpreview.github.io/?https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/help.html)
    
![pwall screen](https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/pwallscreenshot.png)
![pwall folder functions](https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/pwalloption1.png)
![pwall photo functions](https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/pwalloption2.png)

Installation & Build
------------------------

Go to [flathub](https://flathub.org/apps/io.github.ltiber.Pwall) and get the installation command.\

Download the .deb file (release tab). 
It has been tested in ubuntu 16.04, 18.04, 20.04, 22.04, 24.04  and has been built
over the GTK 3 framework.\
To enjoy all the features, you also need to install jhead and gstreamer libraries\
with sudo apt install jhead libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-pulseaudio\
To install the .deb file, run sudo dpkg -i pwall-x.x-x.deb (where -x.x-x is the version you've downloaded).\
If you've got dependency errors run  sudo apt install -f



You can also build and install a package with the makefile located in the src dir.
With a terminal, goto this dir and run make then make install



