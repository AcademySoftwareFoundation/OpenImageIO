/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "dassert.h"
#include "plugin.h"
#include "strutil.h"
#include "filesystem.h"

#include "imageio.h"
#include "imageio_pvt.h"

using namespace OpenImageIO;
using namespace OpenImageIO::pvt;


typedef std::map <std::string, create_prototype> PluginMap;

namespace {

// Map format name to ImageInput creation
static PluginMap input_formats;
// Map format name to ImageOutput creation
static PluginMap output_formats;
// Map file extension to ImageInput creation
static PluginMap input_extensions;
// Map file extension to ImageOutput creation
static PluginMap output_extensions;
// Map format name to plugin handle
static std::map <std::string, Plugin::Handle> plugin_handles;
// Map format name to full path
static std::map <std::string, std::string> plugin_filepaths;

// FIXME -- do we use the extensions above?


static std::string pattern = Strutil::format (".imageio.%s",
                                              Plugin::plugin_extension());

/// Register the input and output 'create' routine and list of file
/// extensions for a particular format.
static void
declare_plugin (const std::string &format_name,
                create_prototype input_creator, const char **input_extensions,
                create_prototype output_creator, const char **output_extensions)
{
//    std::cerr << "declaring plugin for " << format_name << "\n";

    // Look for input creator and list of supported extensions
    if (input_creator) {
        if (input_formats.find(format_name) != input_formats.end())
            input_formats[format_name] = input_creator;
        std::string extsym = format_name + "_input_extensions";
        for (const char **e = input_extensions; e && *e; ++e) {
            std::string ext (*e);
            boost::algorithm::to_lower (ext);
            // std::cerr << "  input extension " << ext << "\n";
            if (input_formats.find(ext) == input_formats.end())
                input_formats[ext] = input_creator;
        }
    }

    // Look for output creator and list of supported extensions
    if (output_creator) {
        if (output_formats.find(format_name) != output_formats.end())
            output_formats[format_name] = output_creator;
        for (const char **e = output_extensions; e && *e; ++e) {
            std::string ext (*e);
            boost::algorithm::to_lower (ext);
            // std::cerr << "  output extension " << ext << "\n";
            if (output_formats.find(ext) == output_formats.end())
                output_formats[ext] = output_creator;
        }
    }
}



static void
catalog_plugin (const std::string &format_name,
                const std::string &plugin_fullpath)
{
    // Remember the plugin
    std::map<std::string, std::string>::const_iterator found_path;
    found_path = plugin_filepaths.find (format_name);
    if (found_path != plugin_filepaths.end()) {
        // Hey, we already have an entry for this format
        if (found_path->second == plugin_fullpath) {
            // It's ok if they're both the same file; just skip it.
            return;
        }
        // if (verbosity > 1)
        std::cerr << "ImageIO WARNING: " << format_name << " had multiple plugins:\n"
                  << "\t\"" << found_path->second << "\"\n"
                  << "    as well as\n"
                  << "\t\"" << plugin_fullpath << "\"\n"
                  << "    Ignoring all but the first one.\n";
        return;
    }

    Plugin::Handle handle = Plugin::open (plugin_fullpath);
    if (! handle) {
        // If verbosity > 1
        // std::cerr << "Open of " << plugin_fullpath << " failed:\n" 
        //          << Plugin::geterror() << "\n";
        return;
    }
//        if (verbosity > 1)
//    std::cerr << "Succeeded in opening " << plugin_fullpath << "\n";
    
    std::string version_function = format_name + "_imageio_version";
    int *plugin_version = (int *) Plugin::getsym (handle, version_function.c_str());
    if (! plugin_version || *plugin_version != OPENIMAGEIO_PLUGIN_VERSION) {
        // OpenImageIO::error ("Plugin \"%s\" did not have '%s_imageio_version' symbol\n",
        //                    plugin_fullpath.c_str(), format_name.c_str());
        Plugin::close (handle);
        return;
    }

    // Add the filepath and handle to the master lists
    plugin_filepaths[format_name] = plugin_fullpath;
    plugin_handles[format_name] = handle;

    create_prototype input_creator =
        (create_prototype) Plugin::getsym (handle, format_name+"_input_imageio_create");
    const char **input_extensions =
        (const char **) Plugin::getsym (handle, format_name+"_input_extensions");
    create_prototype output_creator =
        (create_prototype) Plugin::getsym (handle, format_name+"_output_imageio_create");
    const char **output_extensions =
        (const char **) Plugin::getsym (handle, format_name+"_output_extensions");

    if (input_creator || output_creator)
        declare_plugin (format_name, input_creator, input_extensions,
                        output_creator, output_extensions);
    else
        Plugin::close (handle);   // not useful
}

}

#ifdef EMBED_PLUGINS

#ifdef OPENIMAGEIO_NAMESPACE
namespace OPENIMAGEIO_NAMESPACE {
#endif

// Make extern declarations for the input and output create routines and
// list of file extensions, for the standard plugins that come with OIIO.
// These won't be used unless EMBED_PLUGINS is defined.  Use the PLUGENTRY
// macro to make the declaration compact and easy to read.
#define PLUGENTRY(name)                                 \
    ImageInput *name ## _input_imageio_create ();       \
    ImageOutput *name ## _output_imageio_create ();     \
    extern const char *name ## _output_extensions[];    \
    extern const char *name ## _input_extensions[];

    PLUGENTRY (bmp);
//    PLUGENTRY (cineon);
    PLUGENTRY (dds);
    PLUGENTRY (dpx);
    PLUGENTRY (fits);
    PLUGENTRY (hdr);
    PLUGENTRY (ico);
    PLUGENTRY (jpeg);
    PLUGENTRY (jpeg2000);
    PLUGENTRY (openexr);
    PLUGENTRY (png);
    PLUGENTRY (pnm);
    PLUGENTRY (sgi);
    PLUGENTRY (socket);
    PLUGENTRY (softimage);
    PLUGENTRY (tiff);
    PLUGENTRY (targa);
    PLUGENTRY (zfile);

#ifdef OPENIMAGEIO_NAMESPACE
}
using namespace OPENIMAGEIO_NAMESPACE;
#endif

#endif // defined(EMBED_PLUGINS)


namespace {

/// Add all the built-in plugins, those compiled right into libOpenImageIO,
/// to the catalogs.  This does nothing if EMBED_PLUGINS is not defined,
/// in which case they'll be registered only when read from external
/// DSO/DLL's.
static void
catalog_builtin_plugins ()
{
#ifdef EMBED_PLUGINS
    // Use DECLAREPLUG macro to make this more compact and easy to read.
#define DECLAREPLUG(name)                                               \
    declare_plugin (#name,                                              \
                    (create_prototype) name ## _input_imageio_create,   \
                    name ## _input_extensions,                          \
                    (create_prototype) name ## _output_imageio_create,  \
                    name ## _output_extensions)

    DECLAREPLUG (bmp);
//    DECLAREPLUG (cineon);
    DECLAREPLUG (dds);
    DECLAREPLUG (dpx);
    DECLAREPLUG (fits);
    DECLAREPLUG (hdr);
    DECLAREPLUG (ico);
    DECLAREPLUG (jpeg);
#ifdef USE_JASPER
    DECLAREPLUG (jpeg2000);
#endif
    DECLAREPLUG (openexr);
    DECLAREPLUG (png);
    DECLAREPLUG (pnm);
    DECLAREPLUG (sgi);
#ifdef USE_BOOST_ASIO
    DECLAREPLUG (socket);
#endif
    DECLAREPLUG (softimage);
    DECLAREPLUG (tiff);
    DECLAREPLUG (targa);
    DECLAREPLUG (zfile);
#endif
}

/// Look at ALL imageio plugins in the searchpath and add them to the
/// catalog.  This routine is not reentrant and should only be called
/// by a routine that is holding a lock on imageio_mutex.
static void
catalog_all_plugins (std::string searchpath)
{
    catalog_builtin_plugins ();

    const char *imageio_library_path = getenv ("IMAGEIO_LIBRARY_PATH");
    if (imageio_library_path && *imageio_library_path) {
        std::string newpath = imageio_library_path;
        if (searchpath.length())
            newpath = newpath + ':' + searchpath;
        searchpath = newpath;
    }
//    std::cerr << "catalog_all_plugins: searchpath = '" << searchpath << "'\n";

    size_t patlen = pattern.length();
//    std::cerr << "pattern is " << pattern << ", length=" << patlen << "\n";
    std::vector<std::string> dirs;
    Filesystem::searchpath_split (searchpath, dirs, true);
    BOOST_FOREACH (std::string &dir, dirs) {
//        std::cerr << "Directory " << dir << "\n";
        boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
        for (boost::filesystem::directory_iterator itr (dir);
              itr != end_itr;  ++itr) {
            std::string full_filename = itr->path().string();
            std::string leaf = itr->path().leaf();
//            std::cerr << "\tfound file " << full_filename << ", leaf = '" << leaf << "'\n";
            size_t found = leaf.find (pattern);
            if (found != std::string::npos &&
                (found == leaf.length() - patlen)) {
                std::string pluginname (leaf.begin(), leaf.begin() + leaf.length() - patlen);
//                std::cerr << "\t\tFound imageio plugin " << full_filename << "\n";
//                std::cerr << "\t\t\tplugin name = '" << pluginname << "'\n";
                catalog_plugin (pluginname, full_filename);
//                plugin_names.push_back (full_filename);
//                plugin_handles.push_back (0);
            }
        }
    }
//    std::cerr << "done catalog_all\n";
}

}


ImageOutput *
ImageOutput::create (const std::string &filename, const std::string &plugin_searchpath)
{
    if (filename.empty()) { // Can't even guess if no filename given
        OpenImageIO::pvt::error ("ImageOutput::create() called with no filename");
        return NULL;
    }

    // Extract the file extension from the filename
    std::string format = boost::filesystem::extension (filename);
    if (format.empty()) {
        // If the file had no extension, maybe it was itself the format name
        format = filename;
    } else {
        if (format[0] == '.')
            format.erase (format.begin());  // Erase leading dot
        // if (verbose > 1)
        // std::cerr << "extension of '" << filename << "' is '" << format << "'\n";
    }

    recursive_lock_guard lock (imageio_mutex);  // Ensure thread safety

    // See if it's already in the table.  If not, scan all plugins we can
    // find to populate the table.
    boost::algorithm::to_lower (format);
    if (output_formats.find (format) == output_formats.end())
        catalog_all_plugins (plugin_searchpath);

    if (output_formats.find (format) == output_formats.end()) {
        if (input_formats.empty()) {
            // This error is so fundamental, we echo it to stderr in
            // case the app is too dumb to do so.
            const char *msg = "ImageOutput::create() could not find any ImageOutput plugins!\n"
                          "    Perhaps you need to set IMAGEIO_LIBRARY_PATH.\n";
            fprintf (stderr, "%s", msg);
            OpenImageIO::pvt::error ("%s", msg);
        }
        else
            OpenImageIO::pvt::error ("ImageOutput::create_format() could not find a plugin for \"%s\"\n    searchpath = \"%s\"\n",
                                     filename.c_str(), plugin_searchpath.c_str());
        return NULL;
    }

    create_prototype create_function = output_formats[format];
    ASSERT (create_function != NULL);
    return (ImageOutput *) create_function();
}



ImageInput *
ImageInput::create (const std::string &filename, const std::string &plugin_searchpath)
{
    if (filename.empty()) { // Can't even guess if no filename given
        OpenImageIO::pvt::error ("ImageInput::create() called with no filename");
        return NULL;
    }

    // Extract the file extension from the filename
    std::string format = boost::filesystem::extension (filename);
    if (format.empty()) {
        // If the file had no extension, maybe it was itself the format name
        format = filename;
    } else {
        if (format[0] == '.')
            format.erase (format.begin());  // Erase leading dot
        // if (verbose > 1)
        // std::cerr << "extension of '" << filename << "' is '" << format << "'\n";
    }

    recursive_lock_guard lock (imageio_mutex);  // Ensure thread safety

    // See if it's already in the table.  If not, scan all plugins we can
    // find to populate the table.
    boost::algorithm::to_lower (format);
    if (input_formats.find (format) == input_formats.end())
        catalog_all_plugins (plugin_searchpath);

    create_prototype create_function = NULL; 
    if (input_formats.find (format) != input_formats.end()) {
        create_function = input_formats[format];
    } else {
        // If a plugin can't be found that was explicitly designated for
        // this extension, then just try every one we find and see if
        // any will open the file.  Pass it a configuration request that
        // includes a "nowait" option so that it returns immediately if
        // it's a plugin that might wait for an event, like a socket that
        // doesn't yet exist).
        ImageSpec config;
        config.attribute ("nowait", (int)1);
        for (PluginMap::const_iterator plugin = input_formats.begin();
             plugin != input_formats.end(); ++plugin)
        {
            ImageSpec test_spec;
            ImageInput *test_plugin = (ImageInput*) plugin->second();
            bool ok = test_plugin->open(filename, test_spec, config);
            if (ok)
                test_plugin->close ();
            delete test_plugin;
            if (ok) {
                create_function = plugin->second;
                break;
            }
        }
    }

    if (create_function == NULL) {
        if (input_formats.empty()) {
            // This error is so fundamental, we echo it to stderr in
            // case the app is too dumb to do so.
            const char *msg = "ImageInput::create() could not find any ImageInput plugins!\n"
                          "    Perhaps you need to set IMAGEIO_LIBRARY_PATH.\n";
            fprintf (stderr, "%s", msg);
            OpenImageIO::pvt::error ("%s", msg);
        }
        else if (boost::filesystem::exists (filename))
            OpenImageIO::pvt::error ("ImageInput::create() could not find a plugin for \"%s\"\n    searchpath = \"%s\"\n",
                                 filename.c_str(), plugin_searchpath.c_str());
        else
            OpenImageIO::pvt::error ("ImageInput::create() could not find \"%s\"\n",
                                     filename.c_str());
        return NULL;
    }

    return (ImageInput *) create_function();
}
