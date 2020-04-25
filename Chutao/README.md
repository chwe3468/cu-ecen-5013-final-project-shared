# ECEN 5013 AESD - App Code for Chutao

**AESD Camera Application** (aesd-cam)
This program using opencv to capture an image from camera and save it as in /home/cap.jpeg
To view image in command line, please use
'''
cd /home/picture
fbv cap.jpeg
'''
A .ppm version of the image will be saved at /home/cap/ppm, but cannot be viewed with fbv command, because fbv does not suport viewing of .ppm file
</br>

**AESD Client Application** (aesd-cam-client)
Test program.
</br>

**AESD Camera Client Application** (aesd-client)
Send image to Sam's server or my own laptop's server every 3 sec. 
</br>

**AESD Camera Server Application** (aesd-server)
Receive image from my Client (RPi 4), and save it as image in /home/Pictures/cap.ppm
To view image in command line, please use
'''
cd /home/Pictures
fbi cap.ppm
'''
fbi supports viewing of .ppm format.
</br>