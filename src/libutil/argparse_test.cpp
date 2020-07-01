// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <vector>

#include <OpenEXR/ImathColor.h>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>

using namespace OIIO;



// Split a command line into a vector of const char* arguments.
std::vector<const char*>
split_commands(string_view commands)
{
    std::vector<const char*> result;
    for (auto& c : Strutil::splitsv(commands)) {
        result.push_back(ustring(c).c_str());
    }
    return result;
}



static std::vector<std::string> prearg;
static std::vector<std::string> postarg;
static std::vector<std::string> callbacklist;


static int
parse_prearg(int argc, const char* argv[])
{
    for (int i = 0; i < argc; i++)
        prearg.emplace_back(argv[i]);
    return 0;
}


static int
parse_postarg(int argc, const char* argv[])
{
    for (int i = 0; i < argc; i++)
        postarg.emplace_back(argv[i]);
    return 0;
}



static int
callback(int argc, const char* argv[])
{
    for (int i = 0; i < argc; i++) {
        // std::cout << "callback " << argv[i] << "\n";
        callbacklist.emplace_back(argv[i]);
    }
    return 0;
}



static void
test_old()
{
    auto args   = split_commands("basic alpha --flag --unflag --intarg 42 "
                               "--floatarg 3.5 --stringarg foo "
                               "--append xxx --append yyy "
                               "--hidden "
                               "--callback who "
                               "bravo charlie");
    bool flag   = false;
    bool unflag = true;
    bool hidden = false;
    int i       = 0;
    float f     = 0;
    std::string s;
    std::vector<std::string> list;
    prearg.clear();
    postarg.clear();
    callbacklist.clear();

    ArgParse ap;
    // clang-format off
    ap.options(
        "basic",
        "%1", parse_prearg, "",
        "%*", parse_postarg, "",
        "--flag", &flag, "Set flag",
        "--unflag %!", &unflag, "Unset flag",
        "--hidden", &hidden, "",  // no help means hidden
        "--intarg %d", &i, "int",
        "--floatarg %f", &f, "float",
        "--stringarg %s", &s, "string",
        "--callback %@ %s", callback, nullptr, "callback",
        "--append %L", &list, "string list",
        nullptr);
    ap.usage();
    // clang-format on

    ap.parse(int(args.size()), args.data());
    OIIO_CHECK_EQUAL(flag, true);
    OIIO_CHECK_EQUAL(unflag, false);
    OIIO_CHECK_EQUAL(hidden, true);
    OIIO_CHECK_EQUAL(i, 42);
    OIIO_CHECK_EQUAL(f, 3.5f);
    OIIO_CHECK_EQUAL(s, "foo");
    OIIO_CHECK_EQUAL(list.size(), 2);
    if (list.size() == 2) {
        OIIO_CHECK_EQUAL(list[0], "xxx");
        OIIO_CHECK_EQUAL(list[1], "yyy");
    }
    OIIO_CHECK_EQUAL(prearg.size(), 1);
    if (prearg.size() >= 1) {
        OIIO_CHECK_EQUAL(prearg[0], "alpha");
    }
    OIIO_CHECK_EQUAL(postarg.size(), 2);
    if (postarg.size() >= 2) {
        OIIO_CHECK_EQUAL(postarg[0], "bravo");
        OIIO_CHECK_EQUAL(postarg[1], "charlie");
    }
    OIIO_CHECK_EQUAL(callbacklist.size(), 2);
    if (callbacklist.size() >= 2) {
        OIIO_CHECK_EQUAL(callbacklist[0], "--callback");
        OIIO_CHECK_EQUAL(callbacklist[1], "who");
    }
}



static void
test_new()
{
    std::cout << "\nTesting new style:\n";
    auto args = split_commands("basic -f -u --ci --cs --cf --istore 15 "
                               "--fstore 12.5 --sstore hi "
                               "--color 0.25 0.5 0.75 "
                               "--app 14 --app 22 "
                               "--sapp hello --sapp world "
                               "--fbi a b c "
                               "bravo charlie");

    // clang-format off
    ArgParse ap;
    ap.intro("new style!")
      .usage("here is my usage")
      .description("description")
      .epilog("epilog");

    ap.arg("filename")
      .action(ArgParse::append())
      .hidden();
    ap.arg("-f")
      .help("Simple flag argument")
      .action(ArgParse::store_true());
    ap.arg("--f2")
      .help("Simple flag argument (unused)")
      .store_true();
    ap.arg("-u")
      .help("Simple flag argument - store false if set")
      .store_false();
    ap.arg("--u2")
      .help("Simple flag argument - store false if set (unused)")
      .store_false();
    ap.arg("--ci")
      .help("Store constant int")
      .action(ArgParse::store_const(42));
    ap.arg("--cf")
      .help("Store constant float")
      .action(ArgParse::store_const(3.14159f));
    ap.arg("--cfdef")
      .help("Store constant float")
      .defaultval(42.0f)
      .action(ArgParse::store_const(3.14159f));
    ap.arg("--cs")
      .help("Store constant string")
      .action(ArgParse::store_const("hey hey"));

    ap.separator("Storing values:");
    ap.arg("--istore")
      .help("store an int value")
      .metavar("INT")
      .action(ArgParse::store<int>());
    ap.arg("--fstore")
      .help("store a float value")
      .metavar("FLOAT")
      .action(ArgParse::store<float>());
    ap.arg("--sstore")
      .help("store a string value")
      .metavar("STRING")
      .action(ArgParse::store());
#if 0
    ap.arg("--color %f:R %f:G %f:B")
      .help("store 3 floats into a color")
      .action(ArgParse::store<float>());
#else
    ap.arg("--color R G B")
      // .metavar("R G B")
      .help("store 3 floats into a color")
      .defaultval(Imath::Color3f(0.0f, 0.0f, 0.0f))
      .action(ArgParse::store<float>());
#endif
    ap.arg("--unsettriple")
      .help("store 3 floats into a triple")
      .metavar("R G B")
      .defaultval(Imath::Color3f(1.0f, 2.0f, 4.0f))
      .action(ArgParse::store<float>());
    ap.arg("--app")
      .help("store an int, will append to a list")
      .metavar("VAL")
      .action(ArgParse::append<int>());
    ap.arg("--sapp")
      .help("store a string, will append to a list")
      .metavar("STR")
      .action(ArgParse::append());
    ap.arg("--fbi")
      .help("Call the FBI")
      .nargs(3);
    ap.add_help(true);
    // clang-format on

    ap.parse(int(args.size()), args.data());
    ap.print_help();

    OIIO_CHECK_EQUAL(ap["f"].get<int>(), 1);
    OIIO_CHECK_EQUAL(ap["f2"].get<int>(), 0);
    OIIO_CHECK_EQUAL(ap["u"].get<int>(), 0);
    OIIO_CHECK_EQUAL(ap["u2"].get<int>(), 1);
    OIIO_CHECK_EQUAL(ap["ci"].get<int>(), 42);
    OIIO_CHECK_EQUAL(ap["cf"].get<float>(), 3.14159f);
    OIIO_CHECK_EQUAL(ap["cfdef"].get<float>(), 42.0f);
    OIIO_CHECK_EQUAL(ap["cs"].get<std::string>(), "hey hey");
    OIIO_CHECK_EQUAL(ap["istore"].get<int>(), 15);
    OIIO_CHECK_EQUAL(ap["fstore"].get<float>(), 12.5f);
    OIIO_CHECK_EQUAL(ap["sstore"].get<std::string>(), "hi");
    OIIO_CHECK_EQUAL(ap["color"].get<Imath::Color3f>(),
                     Imath::Color3f(0.25f, 0.5f, 0.75f));
    OIIO_CHECK_EQUAL(ap["unsettriple"].get<Imath::Color3f>(),
                     Imath::Color3f(1.0f, 2.0f, 4.0f));
    OIIO_CHECK_EQUAL(ap["filename"].type(), TypeDesc("string[2]"));
    std::cout << "\nAll args:\n";
    for (auto& a : ap.params())
        Strutil::printf("  %s = %s   [%s]\n", a.name(), a.get_string(),
                        a.type());
    std::cout << "Extracting filenames:\n";
    auto fn = ap["filename"].as_vec<std::string>();
    for (auto& f : fn)
        Strutil::printf("  \"%s\"\n", f);
}



int
main(int /*argc*/, char* /*argv*/[])
{
    test_old();
    test_new();

    return unit_test_failures != 0;
}
