clang-tidy docker image for Mesos
=================================

This repository contains a docker image to run clang-tidy
checks over a Mesos repository. It uses a [version of
clang-tidy](https://github.com/mesos/clang-tools-extra/tree/mesos_38)
augmented with Mesos-specific checks.


Running checks
--------------

To run checks over a Mesos checkout invoke

    $ docker run \
        -v <MESOS_CHECKOUT>:/SRC \
        -v <RESULT_DIR>:/OUT \
        [-e CHECKS=<CHECKS>] \
        [-e CONFIGURE_FLAGS=<CONFIGURE_FLAGS>] \
        --rm \
        mesos-tidy

Here `MESOS_CHECKOUT` points to a git checkout of the Mesos source
tree, and `RESULT_DIR` is directory to store result.

Additional configure parameters can be passed to the `./configure`
invocation for Mesos via `CONFIGURE_FLAGS`. By default, `./configure`
will be invoked without arguments.

Optionally, the set of checks to perform can be specified in a
clang-tidy check regex via `CHECKS`. By default, only Mesos-specific
checks will be performed, i.e., the default value for `CHECKS` is
`-*,mesos-*`.

Results from 3rdparty dependencies are filtered from the result set.


Building the docker image
-------------------------

To building the image involves compiling a Mesos-flavored version of
`clang-tidy`, installing Mesos dependencies, and installation of tools
to drive check invocations.

The image can be built with

    $ docker build -t mesos-tidy .
