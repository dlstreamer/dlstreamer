Coding Style
============

We use Clang formatter for our code. Code style checker is being run
through CI, so it's essential for you to format code before you can
actually merge it to ``master``. There is a config for clang-format-7
(``.clang-format`` in the root folder of project). Hence, you need to
install it locally and configure your IDE to format your code by using
it.

Install clang formatter
-----------------------

If you are using Ubuntu 18.04 it's quite simple:

.. code:: sh

   sudo apt install -y clang-format-7
   sudo ln -s /usr/bin/clang-format-7 /usr/bin/clang-format

If you are using different distro you can find an appropriate repository
here ``https://apt.llvm.org/``. And install the newest version:

.. code:: sh

   sudo apt install -y clang-format-9
   sudo ln -s /usr/bin/clang-format-9 /usr/bin/clang-format

Configure IDE
-------------

-  Visual Studio Code If you are using VS Code you just need to install
   corresponding extension clang-format and configure autoformat on save
   (you can find instruction on the extension page).
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
         It basically calls clang-format and does inplace formatting
         using the style define in the first ``.clang-format`` file found in
         a parent directory.

-  Different IDE If you are using different IDE or text editor please
   find an appropriate instruction by yourself and suggest updates for
   this wiki. We would be glad to accept your contributions.

Have a good coding 😊
