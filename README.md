# detector-network-processor

Application to process data coming in from detector stations.

The main goal is to calculate coincidences on the fly. The calculation itself is not complicated though it requires supporting infrastructure.
This infrastructure includes the classification of detector stations as reliable or unreliable to maintain a good level for the quality of the incoming data.

It uses an event-driven pipeline design and can theoretically use a wide variety of input sources and ouput sinks.
Currently implemented is only a MQTT input source and MQTT, InfluxDB and an ascii output sinks.

## dependencies
Dependencies are
```
boost system
boost program_options
boost beast

libmosquitto
libsasl2
libldap
```
Consult your distributions package manager to discover how to install those dependencies.

Build dependencies are
```
cmake
```

## compiling
For compiling, it is recommended to use an out-of-source build directory. Here it is assumed the build directory is on the same directory level as the cloned repository.
Execute the following commands in order to compile the processor.
```
mkdir build
cd build
cmake ../detector-network-processor -DCMAKE_BUILD_TYPE=Release
make
```
This will result in the executable being written to `output/bin` in the build directory.

## installation
Simply execute
```
make install
```
in the build directory.
### debian based distributions
In the case of debian based distributions you can optionally also build a package for easy installation.
In order to do so, run
```
cpack
```
in the build directory. The debian package will be created in `output/packages` in the build directory.
Install it via
```
apt install ./<package_name>.deb
```

## Configuration
In order to see all configuration options, see the file `config/detector-network-processor.cfg`. Upon installation, this file will be written to `/etc/muondetector/detector-network-processor.cfg`.
Edit this file to your needs.
Commandline options are
```
  -h [ --help ]                         produce help message
  -o [ --offline ]                      Do not send processed data to the 
                                        servers.
  -d [ --debug ]                        Use the ascii sinks for debugging.
  -l [ --local ]                        Run the cluser as a local instance
  -v [ --verbose ] arg (=0)             Verbosity level
  -c [ --config ] arg (=/etc/muondetector/detector-network-processor.cfg)
                                        Specify a configuration file to use
```
## Executing
It is recommended to use the service file provided with the software, it should have been placed in the appropriate directory upon installation. Enable and start it with
```
systemctl enable --now detector-network-processor
```
However, you can also run the software without using the service file.
