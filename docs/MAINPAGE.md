# Adaptyst documentation
Welcome to the Adaptyst documentation for contributors!

To get started, please:
1. Read "Communication between the frontend, server, clients, and profilers" below.
2. Go through the source code, starting with ```entrypoint.hpp```/```entrypoint.cpp``` inside ```src``` and ```src/server``` and using the documentation along the way to get to know the specific classes and functions (you can browse "Namespaces" and "Classes" for this purpose).

### Preprocessor definitions
These are the CMake-set Adaptyst-specific (i.e. non-Boost) preprocessor definitions you should be aware of:
* ```SERVER_ONLY```: set when Adaptyst is compiled only with the backend component (i.e. adaptyst-server).
* ```LIBNUMA_AVAILABLE```: set when Adaptyst is compiled with libnuma support.
* ```ADAPTYST_CONFIG_FILE```: the path to the Adaptyst config file, CMake sets it to ```/etc/adaptyst.conf``` by default.
* ```ADAPTYST_SCRIPT_PATH```: the path to the directory with Adaptyst "perf" Python scripts, CMake sets it to ```/opt/adaptyst``` by default.

### Tests
Making sure the tests pass and updating these when needed is crucial during the Adaptyst development. They are implemented using [the GoogleTest framework](https://github.com/google/googletest) and their codes are stored inside the ```test``` directory.

To enable tests in the Adaptyst compilation, run ```build.sh``` with ```-DENABLE_TESTS=ON```. Afterwards, run ```ctest``` inside the newly-created build directory every time you want to run the tests.

### Communication between the frontend, server, clients, subclients, and profilers
The backend (adaptyst-server) consists of the Server, Client, and Subclient components. The communication between these components and the frontend + profilers differs depending on whether adaptyst-server is run externally or internally. The diagrams below explain how this works for both cases.

Please note the following:
1. In both cases, the frontend additionally sends the received subclient connection instructions directly to each profiler before waiting for "start_profile".
2. In case of adaptyst-server running externally, if "p code_paths.lst" is sent by the frontend during the file transfer stage, no code\_paths.lst file is actually created by the server. Instead, it consumes the received content (i.e. the list of source code paths) immediately to produce a source code archive.

**If adaptyst-server is run externally with the frontend connecting to it via TCP, the communication between the frontend, profilers, and server components is as follows (each colour represents a machine; different-coloured blocks can therefore run on different machines, but they don't have to):**

<img class="main_page_img" src="external.svg" alt="External adaptyst-server communication diagram" />

**If adaptyst-server is run internally (i.e. as part of the ```adaptyst``` command) with the frontend connecting to it via file descriptors, the communication is as follows:**

<img class="main_page_img" src="internal.svg" alt="Internal adaptyst-server communication diagram" />
