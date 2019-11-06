#!/bin/bash
# used in the copyTo dialog of pepper app
# receive the folder as parameter
cd $1
# put the suffix .~1~ at the begining of the filename
# these files are created if copy to an existing file
# the system makes a backup with the pattern {filename}.JPG.~[0-9]~ 
# for instance IMG_3221.JPG.~1~ will be changed in ~1~IMG_3221.JPG
for file in *.~?~; do mv $file `echo $file |sed -e "s/\(.*\)\.\(~[0-9]~$\)/\2\1/"`;done
