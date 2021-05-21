# detector-network-processor

Application to process data coming in from detector stations.

It uses an event-driven pipeline design and can theoretically use a wide variety of input sources and ouput sinks.
Currently implemented is only a MQTT input source and MQTT, InfluxDB and an ascii output sinks.

Compile by downloading this repository, creating a build directory next to the repository and running

`cmake ../detector-network-processor && make`

If you want to use this on a debian system it is recommended to automatically create a package by running

`make package`

The output files will be written to the directory `output` in the build directory.
