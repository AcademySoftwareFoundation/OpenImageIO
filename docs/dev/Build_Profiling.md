<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# Build profiling

These are instructions for profiling the build process of OpenImageIO --
useful for figuring out where the build process is spending its time, and
where it might be optimized.

## Profiling module compilation with clang -ftime-trace

These instructions are based on the following references:

- general instructions: https://www.superfunc.zone/posts/001-speeding-up-c-builds/
- ninjatracing.py script: https://github.com/nico/ninjatracing
- Background info: https://aras-p.info/blog/2019/01/16/time-trace-timeline-flame-chart-profiler-for-Clang/

The steps are as follows, and they require the use of clang, ninja, and
the chrome browser:

1. Build with clang using the `-ftime-trace`, flag which is set when you use
   the OIIO build option -DOIIO_BUILD_PROFILE=1. You MUST use clang, as this
   is a clang-specific feature, and you must use the ninja builder (use CMake
   arguments `-G ninja`). This must be a *fresh* build, not an incremental
   build (or the results will only reflect the incrementally build parts).

   This will result in a file, build/.ninja_log, containing timings from
   the build process.

2. Prepare the output using the `ninjatracing.py` script as follows:

   ```
   python3 src/build-scripts/ninjatracing.py -a build/.ninja_log > top_level.json
   ```

3. Running the Chrome browser, visit the URL `chrome://tracing`, and LOAD that
   json file created in the previous step. This will display a timeline of the
   build process, showing which thread, in what order, and how long each
   module took to compile.

4. For even more detail on what's happening within the compilation of each
   module, you can use the `-e` flag to expand the output:

   ```
   python3 src/build-scripts/ninjatracing.py -a -e -g 50000 build/.ninja_log > expanded.json
   ```

   Loading this in Chrome's tracing view, as above, will show a full flame
   chart of the build process, showing the time spent in the different
   functional parts of the compiler and which functions of OIIO it is
   compiling or which templates are being instantiated.
