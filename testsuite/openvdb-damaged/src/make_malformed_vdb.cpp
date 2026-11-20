// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Generator for the malformed .vdb fixtures in this directory. The files it
// writes are committed, so this only needs to be run if they must be
// regenerated. It needs the OpenVDB headers and library, which the testsuite
// itself does not, so it is built by hand rather than by CMake:
//
//   c++ -std=c++17 make_malformed_vdb.cpp -o make_malformed_vdb \
//       -I$OPENVDB_ROOT/include -L$OPENVDB_ROOT/lib -lopenvdb -ltbb
//   ./make_malformed_vdb .
//
// Every grid below carries an empty tree, so the whole hazard lives in the
// file_bbox_min/file_bbox_max metadata that the reader trusts for the image
// extents. That keeps the fixtures a few hundred bytes each.

#include <openvdb/io/Stream.h>
#include <openvdb/openvdb.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace openvdb;


// io::Archive::writeHeader() always stamps the file with the compiled-in
// OPENVDB_FILE_VERSION, with no public way to ask for an older one. Some CI
// configs build against an OpenVDB old enough to warn (but still function
// fine) on anything newer than OPENVDB_FILE_VERSION_MULTIPASS_IO (224), which
// is well past everything these fixtures use. So patch the version field
// back down after writing. The field is a little-endian uint32 straight
// after the 8-byte magic number; this is stable file layout, not API.
static void
cap_file_version(const std::string& path, uint32_t max_version)
{
    FILE* f = fopen(path.c_str(), "r+b");
    if (!f)
        return;
    fseek(f, 8, SEEK_SET);
    uint32_t version = 0;
    if (fread(&version, sizeof(version), 1, f) == 1 && version > max_version) {
        fseek(f, 8, SEEK_SET);
        fwrite(&max_version, sizeof(max_version), 1, f);
    }
    fclose(f);
}


static void
write_grid(const std::string& path, GridBase::Ptr grid)
{
    GridPtrVec grids;
    grids.push_back(grid);
    io::File file(path);
    // Otherwise OpenVDB recomputes file_bbox_* from the tree on write and
    // discards the hostile values we are trying to bake in.
    file.setGridStatsMetadataEnabled(false);
    file.write(grids);
    file.close();
    cap_file_version(path, OPENVDB_FILE_VERSION_MULTIPASS_IO);
    printf("wrote %s\n", path.c_str());
}


static void
write_bbox_grid(const std::string& path, const char* name, const Vec3i& bbmin,
                const Vec3i& bbmax)
{
    FloatGrid::Ptr g = FloatGrid::create(0.0f);
    g->setName(name);
    g->insertMeta(GridBase::META_FILE_BBOX_MIN, Vec3IMetadata(bbmin));
    g->insertMeta(GridBase::META_FILE_BBOX_MAX, Vec3IMetadata(bbmax));
    write_grid(path, g);
}


static void
truncate_copy(const std::string& src, const std::string& dst, size_t nbytes)
{
    FILE* in = fopen(src.c_str(), "rb");
    if (!in)
        return;
    std::vector<char> buf(nbytes);
    size_t n = fread(buf.data(), 1, nbytes, in);
    fclose(in);
    FILE* out = fopen(dst.c_str(), "wb");
    if (!out)
        return;
    fwrite(buf.data(), 1, n, out);
    fclose(out);
    printf("wrote %s (%zu bytes)\n", dst.c_str(), n);
}


// Overwrite bytes at a fixed offset in an already-written file.
static void
patch_copy(const std::string& src, const std::string& dst, long offset,
           const void* data, size_t nbytes)
{
    FILE* in = fopen(src.c_str(), "rb");
    if (!in)
        return;
    std::vector<char> buf;
    char chunk[4096];
    for (size_t r; (r = fread(chunk, 1, sizeof(chunk), in)) > 0;)
        buf.insert(buf.end(), chunk, chunk + r);
    fclose(in);
    size_t n = buf.size();
    if (offset + nbytes > n)
        return;
    memcpy(buf.data() + offset, data, nbytes);
    FILE* out = fopen(dst.c_str(), "wb");
    if (!out)
        return;
    fwrite(buf.data(), 1, n, out);
    fclose(out);
    printf("wrote %s\n", dst.c_str());
}



int
main(int argc, char** argv)
{
    initialize();
    const std::string dir = argc > 1 ? argv[1] : ".";
    auto p                = [&](const char* n) { return dir + "/" + n; };

    const int kIntMin = -2147483647 - 1;
    const int kIntMax = 2147483647;

    // A bbox whose extent blows past the reader's per-dimension ceiling.
    write_bbox_grid(p("bad-bbox-huge.vdb"), "huge", Vec3i(0, 0, 0),
                    Vec3i(1000000000, 1000000000, 1000000000));

    // A bbox that stays under the per-dimension ceiling but describes ~32 PB
    // of dense float voxels, so only the total-size limit catches it.
    write_bbox_grid(p("bad-bbox-bomb.vdb"), "bomb", Vec3i(0, 0, 0),
                    Vec3i(200000, 200000, 200000));

    // Extents that overflow 32-bit arithmetic outright.
    write_bbox_grid(p("bad-bbox-overflow.vdb"), "overflow",
                    Vec3i(kIntMin, kIntMin, kIntMin),
                    Vec3i(kIntMax, kIntMax, kIntMax));

    // A max coordinate that overflows when rounded up to the leaf-node grid.
    write_bbox_grid(p("bad-bbox-roundup.vdb"), "roundup", Vec3i(0, 0, 0),
                    Vec3i(kIntMax, 8, 8));

    // An inverted bbox, i.e. a negative resolution.
    write_bbox_grid(p("bad-bbox-inverted.vdb"), "inverted",
                    Vec3i(100, 100, 100), Vec3i(-100, -100, -100));

    // Degenerate but well-formed: a grid with no active voxels at all.
    {
        FloatGrid::Ptr g = FloatGrid::create(0.0f);
        g->setName("empty");
        write_grid(p("bad-bbox-empty.vdb"), g);
    }

    // A file that ends in the middle of the grid descriptor. Truncating any
    // deeper trips an unbounded length-prefixed string read inside OpenVDB
    // that costs ~12 GB of RSS, which is no good for CI or the fuzzer.
    truncate_copy(p("bad-bbox-huge.vdb"), p("truncated.vdb"), 80);

    // A valid file written through io::Stream, which has no grid offsets, so
    // io::File::open() reads each grid inline after its descriptor.
    {
        FloatGrid::Ptr g = FloatGrid::create(0.0f);
        g->setName("stream");
        g->tree().setValue(Coord(1, 2, 3), 0.5f);
        g->tree().setValue(Coord(6, 5, 4), 1.0f);
        GridPtrVec grids;
        grids.push_back(g);
        const std::string path = p("good-stream.vdb");
        std::ofstream os(path, std::ios_base::binary);
        io::Stream stream(os);
        // Plain active-mask compression needs neither zlib nor blosc.
        stream.setCompression(io::COMPRESS_ACTIVE_MASK);
        stream.write(grids);
        os.close();
        cap_file_version(path, OPENVDB_FILE_VERSION_MULTIPASS_IO);
        printf("wrote %s\n", path.c_str());
    }

    // Header layout of these files: version at byte 8, has-grid-offsets flag
    // at 20, metadata count at 57, grid count at 61, grid name length at 65.
    {
        uint32_t version = 300;  // OPENVDB_UNSUPPORTED_FILE_VERSION
        patch_copy(p("bad-bbox-empty.vdb"), p("bad-version-future.vdb"), 8,
                   &version, sizeof(version));
        int32_t ngrids = 0x7fffffff;
        patch_copy(p("good-stream.vdb"), p("bad-stream-count.vdb"), 61, &ngrids,
                   sizeof(ngrids));
        uint32_t namelen = 0xfffffff0;
        patch_copy(p("good-stream.vdb"), p("bad-stream-desc-length.vdb"), 65,
                   &namelen, sizeof(namelen));
    }

    return 0;
}
