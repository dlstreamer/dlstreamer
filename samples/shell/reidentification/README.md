# Reidentification sample
Sample demonstrates pipeline which uses several neural networks to detect and recognize people by faces in the video.
We have to create a file with features that are obtained from the classification plug-in with identification model. These features will be read into the gallery and then used to identify detected objects or track them.
By default, the generator uses model **face-detection-adas-0001** to search for faces and model **face-reidentification-retail-0095** to create features.

## Creating a Gallery for Face Recognition

To recognize faces on a frame, the gallery with features for each person should be created. Gallery can be created using supplied gallery_generator.py script. To do this, you need to create the correct folder with images.
The script uses the **gvametaconvert** to save tensors from **gvaclassify** to a file. Parameter **method** in gvametaconvert is used to create the file name. This file name is defined as:
 
1. image file name - if image is in the root folder
2. subfolder name - if image is in the subfolder
 
Use following command line to run the script:
* **python3 gallery_generator.py -s <images_folder_path>**
 
After running the script a folder (*./features* by default) with tensors in the file and also a gallery.json file with description will be created.
 
For example:
Input images folder structure:
* images
  * person_1
    * img1.png
    * img2.png
  * person_2
    * img1.png
    * img2.png
  * person_3
    * img1.png
 
Outputs will be:
* features
  * person1_0_frame_0_idx_0.tensor
  * person1_1_frame_0_idx_0.tensor
  * person2_0_frame_0_idx_0.tensor
  * person2_1_frame_0_idx_0.tensor
  * person3_0_frame_0_idx_0.tensor
* gallery.json
