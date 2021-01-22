command += run_app("cmake " + test_source_dir + " -DCMAKE_BUILD_TYPE=Release >> build.txt 2>&1", silent=True)
command += run_app("cmake --build . --config Release >> build.txt 2>&1", silent=True)
if platform.system() == 'Windows' :
    command += run_app("Release\\coiiotest")
else :
    command += run_app("./coiiotest")
