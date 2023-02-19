Please visit the Rochester Software Transactional Memory website (http://www.cs.rochester.edu/research/synchronization/rstm/index.shtml) for detailed information on this software package, to include recent publications, example code, and documentation.

This solution contains the necessary projects to build the various software transactional memory libraries distributed with the Rochester Software Transactional Memory (RSTM) package.

Note that this is basic support, and that we welcome all patches, particularly those that are Windows and VisualStudio specific.

The current release distributes 13 different STM implementations. Each implementation has numerous configuration options (contention manager, inevitability implementation, priority implementation, etc.). In this VisualStudio solution, each STM implementation is encoded as a static library project. These projects are the 13 projects named lib*. We provide a configuration utility that allows clients to select the active library, and generate a config.h file that contains information related to the configuration of the active library.

The current release contains two applications that are compatible with VisualStudio, _bench_ and _swarm_. _bench_ is a suite of microbenchmarks that can be used to test STM implementations. _swarm_ is an OpenGL application that uses transactions for synchronization. _swarm_ requires GLUT, which you will need to install on your system on your own. There are many places online that describe this process. 

Building instructions:

1) Build and execute the stmconfig project. This is a command-line utility that allows you to select and configure any of the 13 different library implementations. Execution of the stmconfig app results in the generation of a custom "config.h" file that is included by all of the libraries and applications.

2) Build the lib* project associated with the implementation that you selected and configured in step 1. The only lib* project that can be built correctly is the one that you configured -- attempting to build any of the other lib* projects will generated compile-time errors. The correct lib* project should build without warnings or errors. Please report any problems.

3) Change the project dependencies for the app that you want to build so that it is dependent on the lib* project that you configured and built. This can be done by selecting the project in the solution explorer, right clicking, and selecting the correct lib* project.

4) Build the app that you want.
