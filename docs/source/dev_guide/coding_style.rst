Coding Style
============

We use Clang formatter for our code. Code style checker is being run
through CI, so it is essential to format your code before you can
actually merge it to ``master``. There is a config for clang-format-7
(``.clang-format`` in the root folder of this project). You will need to
install it locally and configure your IDE to format your code with it.

Install clang formatter
-----------------------

If you are using Ubuntu it is quite simple:

.. code:: sh

   sudo apt install -y clang-format-12
   sudo ln -s /usr/bin/clang-format-12 /usr/bin/clang-format

If you are using different distro you can find an appropriate repository
here ``https://apt.llvm.org/``. And install the newest version:

.. code:: sh

   sudo apt install -y clang-format-9
   sudo ln -s /usr/bin/clang-format-9 /usr/bin/clang-format

Configure IDE
-------------

-  Visual Studio Code - If you are using VS Code you can simply install the
   `Clang-Format` extension and follow the provided instructions to configure VS Code to automatically format on save.
-  CLion

   -  Go to File → Settings → Tools → External Tools and click on the plus
      sign. A window should pop up. Choose a name, for example
      “clang-format”
   -  For the Tool settings tab use this configuration:

      -  Program: clang-format (you should use the name of your
         executable here)
      -  Parameters: ``-style=file -i FileName``
      -  Working directory: ``FileDir`` Now, with your file open,
         you can go to Tools → External tools and run the config above.
         It basically calls clang-format and does in-place formatting
         using the style define in the first ``.clang-format`` file found in
         a parent directory.

-  Other IDE - For different IDEs or text editors, please refer to their
   respective documentation. 
   If you would like to contribute instructions for your preferred
   IDE or editor suggest a update for this wiki. We would be glad to 
   accept your contribution.

Have a good time coding! 😊
