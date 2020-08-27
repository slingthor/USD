PYTHON

I use pyenv for developement : this allows you to install different versions of python and easealy manage them.
https://realpython.com/intro-to-pyenv/  ==> instructions on  how to install pyenv
The short version of that document (use the long version if the short one fails):
    Build dependencies:
    => sudo apt-get install -y make build-essential libssl-dev zlib1g-dev \
libbz2-dev libreadline-dev libsqlite3-dev wget curl llvm libncurses5-dev \
libncursesw5-dev xz-utils tk-dev libffi-dev liblzma-dev python-openssl
    Use pyenv installer:
    => curl https://pyenv.run | bash
    At the end you should see something like this:
    WARNING: seems you still have not added 'pyenv' to the load path.

    # Load pyenv automatically by adding
    # the following to ~/.bashrc:

    export PATH="$HOME/.pyenv/bin:$PATH"
    eval "$(pyenv init -)"
    eval "$(pyenv virtualenv-init -)"
    PYTHON_CONFIGURE_OPTS="--enable-shared" pyenv install --force 3.7.x
    
    => at this point go ahead and add those lines in your ~/.bashrc
    
    Installation of python version:
    => $ pyenv install -v 3.7.X   // replace 3.7.X with whatever version of python you want
    
    Usage:
    => $ pyenv versions   // will display the installed version of python that you currently have; 
                          // one of them will probably have * next to it so that one is the current one
    => $ pyenv global 3.X.X // this will allow you to select different versions of python to work with 
                            // as the current version
    => $ pip install pipenv
    
CMAKE
    install it from the Software icon in your linux version

GIT
    => $ sudo apt install git
    
USD DEPENDENCIES
    => $ pip install PySide2
    => $ pip install PyOpenGL
    => $ sudo apt-get install -y libglew-dev
    
USD BUILD
    => go into the usd repo
    => $ python build_scripts/build_usd.py --debug -v /opt/local/USD // here you can play with the options for building. this is a minimal building.
                                                                     // /opt/local/USD was my choice but you can put whatever folder you want there
    once the build is finished or even before add those lines in your ~/.bashrc:
        export PYTHONPATH="/opt/local/USD/lib/python:$PYTHONPATH"
        export PATH="/opt/local/USD/bin:$PATH"
        export TF_DEBUG="GLF_DEBUG_CONTEXT_CAPS"

At this point you should be able to build and run usdview.

PNG TO BASIS FORMAT
    => the png file has to in linear color space not in gamma corrected
        => to do this open gimp and in Image->Precision switch to linear from gamma
    => to create the basis file use the following command
        => $ basis -y_flip linear_png.png
    => to use the basis format instead of the png
        => unzip the usdz file
        => transform the usdc file to usda format : from binary to text
            => $ usdcat -o myfile.usda myfile.usdc
            => in the usda file replace all occurances of .png with .basis
        => transform the usda file to usdc format : this is necessary because otherwize there will be differences in running times between formats
            => $ usdcat -0 myfile.usdc myfile.usda





