Libight
=======

[![Build Status](https://travis-ci.org/TheTorProject/libight.svg?branch=master)](https://travis-ci.org/TheTorProject/libight)

Libight is an experimental library that provides common functionalities
useful to implement open measurement tools on mobile platforms.

As of version 3.0.0, we have successfully compiled and run its tests in the
following systems: MacOS 10.8, OpenBSD 5.5-current, Ubuntu 13.10.

To build the library you need a C90 compiler, a C++11 compiler and
a Unix environment. The `./configure` script should check whether all
the dependencies are in place and should configure the compilers
properly (C++11 must be enabled, otherwise certain C++11 features
such as `std::function` will not be recognized).

How to clone the repository
---------------------------

To clone properly libight repository, make sure that you specify the
`--recursive` command line flag, as in:

    git clone --recursive https://github.com/TheTorProject/libight

Such flag tells git to clone not only the libight repository, but also
the [submodules](http://git-scm.com/docs/git-submodule) contained therein.

Alternatively, once you have cloned the repository, you can checkout all
the submodules using:

    git submodule update --init

How to build it
---------------

    autoreconf -i
    ./configure
    make

If libevent is not found, the build system will configure and
compile the built-in libevent (`src/ext/libevent`). To force
`./configure` to use the libevent at `DIR`, consider using the
`--with-libevent=DIR` option. To force `./configure` to use
the bult-in libevent, specify `--with-libevent=builtin`. For
example, if you want to force `./configure` to use the libevent
installed at `/opt/local`, run:

    ./configure --with-libevent=/opt/local

To compile the unit test programs, run:

    make check

To tell make to produce less output (as in the Linux kernel
build process) run:

    make V=0
