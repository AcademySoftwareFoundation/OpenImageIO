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

#include "dassert.h"
#include "plugin.h"
#include "strutil.h"
#include "filesystem.h"

#include "imageio.h"
#include "imageio_pvt.h"


OIIO_NAMESPACE_ENTER
{
    using namespace pvt;

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
    // Look for input creator and list of supported extensions
    if (input_creator) {
        if (input_formats.find(format_name) != input_formats.end())
            input_formats[format_name] = input_creator;
        std::string extsym = format_name + "_input_extensions";
        for (const char **e = input_extensions; e && *e; ++e) {
            std::string ext (*e);
            Strutil::to_lower (ext);
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
            Strutil::to_lower (ext);
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
#ifdef DEBUG
        std::cerr << "OpenImageIO WARNING: " << format_name << " had multiple plugins:\n"
                  << "\t\"" << found_path->second << "\"\n"
                  << "    as well as\n"
                  << "\t\"" << plugin_fullpath << "\"\n"
                  << "    Ignoring all but the first one.\n";
#endif
        return;
    }

    Plugin::Handle handle = Plugin::open (plugin_fullpath);
    if (! handle) {
        return;
    }
    
    std::string version_function = format_name + "_imageio_version";
    int *plugin_version = (int *) Plugin::getsym (handle, version_function.c_str());
    if (! plugin_version || *plugin_version != OIIO_PLUGIN_VERSION) {
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
    PLUGENTRY (cineon);
    PLUGENTRY (dds);
    PLUGENTRY (dpx);
    PLUGENTRY (field3d);
    PLUGENTRY (fits);
    PLUGENTRY (hdr);
    PLUGENTRY (ico);
    PLUGENTRY (iff);
    PLUGENTRY (jpeg);
    PLUGENTRY (jpeg2000);
    PLUGENTRY (openexr);
    PLUGENTRY (png);
    PLUGENTRY (pnm);
    PLUGENTRY (psd);
    PLUGENTRY (ptex);
    PLUGENTRY (rla);
    PLUGENTRY (sgi);
    PLUGENTRY (socket);
    PLUGENTRY (softimage);
    PLUGENTRY (tiff);
    PLUGENTRY (targa);
    PLUGENTRY (webp);
    PLUGENTRY (zfile);


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
    DECLAREPLUG (cineon);
    DECLAREPLUG (dds);
    DECLAREPLUG (dpx);
#ifdef USE_FIELD3D
    DECLAREPLUG (field3d);
#endif
    DECLAREPLUG (fits);
    DECLAREPLUG (hdr);
    DECLAREPLUG (ico);
    DECLAREPLUG (iff);
    DECLAREPLUG (jpeg);
#ifdef USE_OPENJPEG
    DECLAREPLUG (jpeg2000);
#endif
    DECLAREPLUG (openexr);
    DECLAREPLUG (png);
    DECLAREPLUG (pnm);
    DECLAREPLUG (psd);
    DECLAREPLUG (ptex);
    DECLAREPLUG (rla);
    DECLAREPLUG (sgi);
#ifdef USE_BOOST_ASIO
    DECLAREPLUG (socket);
#endif
    DECLAREPLUG (softimage);
    DECLAREPLUG (tiff);
    DECLAREPLUG (targa);
#ifdef USE_WEBP
    DECLAREPLUG (webp);
#endif
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

    const char *oiio_library_path = getenv ("OIIO_LIBRARY_PATH");
    if (oiio_library_path && *oiio_library_path) {
        std::string newpath = oiio_library_path;
        if (searchpath.length())
            newpath = newpath + ':' + searchpath;
        searchpath = newpath;
    }

    size_t patlen = pattern.length();
    std::vector<std::string> dirs;
    Filesystem::searchpath_split (searchpath, dirs, true);
    BOOST_FOREACH (std::string &dir, dirs) {
        boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
        for (boost::filesystem::directory_iterator itr (dir);
              itr != end_itr;  ++itr) {
            std::string full_filename = itr->path().string();
            std::string leaf = Filesystem::filename (full_filename);
            size_t found = leaf.find (pattern);
            if (found != std::string::npos &&
                (found == leaf.length() - patlen)) {
                std::string pluginname (leaf.begin(), leaf.begin() + leaf.length() - patlen);
                catalog_plugin (pluginname, full_filename);
            }
        }
    }
}

}


ImageOutput *
ImageOutput::create (const std::string &filename,
                     const std::string &plugin_searchpath)
{
    if (filename.empty()) { // Can't even guess if no filename given
        pvt::error ("ImageOutput::create() called with no filename");
        return NULL;
    }

    // Extract the file extension from the filename (without the leading dot)
    std::string format = Filesystem::file_extension (filename);
    if (format.empty()) {
        // If the file had no extension, maybe it was itself the format name
        format = filename;
    }

    recursive_lock_guard lock (imageio_mutex);  // Ensure thread safety

    // See if it's already in the table.  If not, scan all plugins we can
    // find to populate the table.
    Strutil::to_lower (format);
    if (output_formats.find (format) == output_formats.end())
        catalog_all_plugins (plugin_searchpath.size() ? plugin_searchpath
                                 : pvt::plugin_searchpath.string());

    if (output_formats.find (format) == output_formats.end()) {
        if (input_formats.empty()) {
            // This error is so fundamental, we echo it to stderr in
            // case the app is too dumb to do so.
            const char *msg = "ImageOutput::create() could not find any ImageOutput plugins!  Perhaps you need to set OIIO_LIBRARY_PATH.\n";
            fprintf (stderr, "%s", msg);
            pvt::error ("%s", msg);
        }
        else
            pvt::error ("OpenImageIO could not find a format writer for \"%s\". "
                        "Is it a file format that OpenImageIO doesn't know about?\n",
                         filename.c_str());
        return NULL;
    }

    create_prototype create_function = output_formats[format];
    ASSERT (create_function != NULL);
    return (ImageOutput *) create_function();
}



ImageInput *
ImageInput::create (const std::string &filename, 
                    const std::string &plugin_searchpath)
{
    return create (filename, false, plugin_searchpath);
}



ImageInput *
ImageInput::create (const std::string &filename, 
                    bool do_open,
                    const std::string &plugin_searchpath)
{
    if (filename.empty()) { // Can't even guess if no filename given
        pvt::error ("ImageInput::create() called with no filename");
        return NULL;
    }

    // Extract the file extension from the filename (without the leading dot)
    std::string format = Filesystem::file_extension (filename);
    if (format.empty()) {
        // If the file had no extension, maybe it was itself the format name
        format = filename;
    }

    recursive_lock_guard lock (imageio_mutex);  // Ensure thread safety

    // See if it's already in the table.  If not, scan all plugins we can
    // find to populate the table.
    Strutil::to_lower (format);
    if (input_formats.find (format) == input_formats.end())
        catalog_all_plugins (plugin_searchpath.size() ? plugin_searchpath
                                 : pvt::plugin_searchpath.string());

    // Remember which prototypes we've already tried, so we don't double dip.
    std::vector<create_prototype> formats_tried;

    create_prototype create_function = NULL;
    std::string specific_error;
    if (input_formats.find (format) != input_formats.end()) {
        create_function = input_formats[format];
        ASSERT (create_function != NULL);
        if (filename != format) {
            // If given a full filename, double-check that our guess
            // based on the extension actually works.  You never know
            // when somebody will have an incorrectly-named file, let's
            // deal with it robustly.
            formats_tried.push_back (create_function);
            ImageInput *in = (ImageInput *)create_function();
            if (! do_open && in && in->valid_file(filename)) {
                // Special case: we don't need to return the file
                // already opened, and this ImageInput says that the
                // file is the right type.
                return in;
            }
            ImageSpec tmpspec;
            bool ok = in && in->open (filename, tmpspec);
            if (ok) {
                // It worked
                if (! do_open)
                    in->close ();
                return in;
            } else {
                // Oops, it failed.  Apparently, this file can't be
                // opened with this II.  Clear create_function to force
                // the code below to check every plugin we know.
                create_function = NULL;
                if (in)
                    specific_error = in->geterror();
            }
            delete in;
        }
    }

    if (! create_function) {
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
            // If we already tried this create function, don't do it again
            if (std::find (formats_tried.begin(), formats_tried.end(),
                           plugin->second) != formats_tried.end())
                continue;
            formats_tried.push_back (plugin->second);  // remember

            ImageSpec tmpspec;
            ImageInput *in = (ImageInput*) plugin->second();
            if (! in)
                continue;
            if (! do_open && ! in->valid_file(filename)) {
                // Since we didn't need to open it, we just checked whether
                // it was a valid file, and it's not.  Try the next one.
                delete in;
                continue;
            }
            // We either need to open it, or we already know it appears
            // to be a file of the right type.
            bool ok = in->open(filename, tmpspec, config);
            if (ok) {
                if (! do_open)
                    in->close ();
                return in;
            }
            delete in;
        }
    }

    if (create_function == NULL) {
        if (input_formats.empty()) {
            // This error is so fundamental, we echo it to stderr in
            // case the app is too dumb to do so.
            const char *msg = "ImageInput::create() could not find any ImageInput plugins!\n"
                          "    Perhaps you need to set OIIO_LIBRARY_PATH.\n";
            fprintf (stderr, "%s", msg);
            pvt::error ("%s", msg);
        }
        else if (! specific_error.empty()) {
            // Pass along any specific error message we got from our
            // best guess of the format.
            pvt::error ("OpenImageIO could not open \"%s\" as %s: %s",
                        filename.c_str(), format.c_str(), specific_error.c_str());
        }
        else if (Filesystem::exists (filename))
            pvt::error ("OpenImageIO could not find a format reader for \"%s\". "
                        "Is it a file format that OpenImageIO doesn't know about?\n",
                         filename.c_str());
        else
            pvt::error ("Image \"%s\" does not exist. Also, it is not the name of an image format that OpenImageIO recognizes.\n",
                         filename.c_str());
        return NULL;
    }

    return (ImageInput *) create_function();
}

}
OIIO_NAMESPACE_EXIT
