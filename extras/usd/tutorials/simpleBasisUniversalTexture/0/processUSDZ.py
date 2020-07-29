import os
import glob
import zipfile
import shutil

def processUSDZtoBASIS() :
    print("png to basis")
    png_files = os.listdir()
    for png_file in png_files :
        if png_file.endswith(".png") :
            os.system("basisu -y_flip " + png_file)


def main() :
    processUSDZtoBASIS()

if __name__ == "__main__":
    main()


