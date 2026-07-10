# Using the Benchmarking Tool

One of the prerequisites is to have the respective libraries installed. Hence do install the following libraries

- libdw
- libelf

These can be installed via the command `apt-get install libdw-dev libelf-dev`.

To benchmark the packages present another tool is used called `airspeed velocity`. To install it please run the follow command

```pip install asv```

In the parent directory run the following command to get a brief benchmark of your current packages

```asv run```

Use the `-v` flag to get a verbose output.

To compare the all the commits across all the branches you may make use of the following command.

```asv run ALL```

To run benchmarks from a particular commit or tag you can use the commit hash or the tag

```asv run [TAG|HASH]..[branch]```

To compare between tags 

```asv show [TAG]..[branch]```

To have a local server to display all the graphs 

```asv publish```