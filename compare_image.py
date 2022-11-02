# Python file to compare filtered output
import cv2
import os
from os import listdir
import sys

# get the path/directory
folder_dir = os.getcwd()

# Arguments to this program: <SerialCodeFilteredImagesFolder> <ParallelCodeFilteredImagesFolder>
serFold = sys.argv[1]
parFold = sys.argv[2]

# Boolean Checker
incorrectOut = False

for images in os.listdir(folder_dir + "/" + serFold):
 
    # check if the image ends with png
    if (images.endswith(".png")):

        original = cv2.imread(images)
        duplicate = cv2.imread(parFold + "/" + images)

        # 1) Check if 2 images are equals
        if original.shape == duplicate.shape:
            difference = cv2.subtract(original, duplicate)
            b, g, r = cv2.split(difference)

            if (cv2.countNonZero(b) != 0 or cv2.countNonZero(g) != 0 or cv2.countNonZero(r) != 0):
                incorrectOut = True
                print("Output is not correct for image: " + images)
                break

if (not incorrectOut):
    print("All output is correct")
