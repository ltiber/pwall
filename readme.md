
pwall: browse your photos
=============================

![pwall icon](https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/pwall.png)

pwall is modern style photo viewer and organizer.
It shows the folders of your photos main directory
in the left pane and a photowall in the right.
The photowall is a scrolling panel of square thumbnails
giving a preview of each photo. Loading and unloading of this 
scrolling panel is made dynamicaly, viewing page by viewing page
like smartphones do.
It also allows you to have basics management of photos
(change dates, copy a group of photos to another directory, delete, share, get GPS data...).
Clicking on a photo opens it in fullscreen mode. You can continue your navigation in this mode with left and right keys. 
I use it as an alternative to Shotwell.[more...](https://htmlpreview.github.io/?https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/help.html)
    
![pwall screen](https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/pwallscreenshot.png)
![pwall folder functions](https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/pwalloption1.png)
![pwall photo functions](https://raw.githubusercontent.com/ltiber/pwall/master/res/pwall/pwalloption2.png)

Installation & Build
------------------------

Download the .deb file (release tab) and install it in your linux distro. 
It has been tested in ubuntu 16.04 an 18.04 and has been built
over the GTK 3 framework.
To enjoy all the functions you also need to install jhead
with sudo apt install jhead

You can also build and install a package with the makefile located in the src dir.
With a terminal, goto this dir and run $make then $make install



