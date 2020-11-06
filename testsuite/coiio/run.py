command += run_app("cmake --config Release data -DCMAKE_PREFIX_PATH=/home/anders/code/oiio-al/dist/lib/cmake > build.txt 2>&1", silent=True)
command += run_app("cmake --build . >> build.txt 2>&1", silent=True)
command += run_app("./coiiotest > out.txt 2>&1", silent=True)
