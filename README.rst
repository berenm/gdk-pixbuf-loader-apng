GDK Pixbuf Loader for APNG animations
================================================================================

STATUS
--------------------------------------------------------------------------------

Totally experimental for now, basic animations are playable in `eog` but not
much more.


BUILD
--------------------------------------------------------------------------------

.. code:: bash

  cmake -Bbuild -H. -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build build


INSTALL
--------------------------------------------------------------------------------

.. code:: bash

  sudo cmake --build build --target install


LICENSE
-------------------------------------------------------------------------------

 This is free and unencumbered software released into the public domain.

 See accompanying file UNLICENSE or copy at http://unlicense.org/UNLICENSE
