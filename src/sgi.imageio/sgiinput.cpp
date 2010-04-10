/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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
#include "sgi_pvt.h"



// Obligatory material to make this a recognizeable imageio plugin
extern "C" {
    DLLEXPORT int sgi_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *sgi_input_imageio_create () {
        return new SgiInput;
    }
    DLLEXPORT const char *sgi_input_extensions[] = {
        "sgi", "rgb", "rgba", "bw", "int", "inta", NULL
    };
}



bool
SgiInput::open (const std::string &name, ImageSpec &spec)
{
    // saving name for later use
    m_filename = name;

    m_fd = fopen (m_filename.c_str (), "rb");
    if (!m_fd) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    read_header ();

    const short SGI_MAGIC = 0x01DA;
    if (m_sgi_header.magic != SGI_MAGIC) {
        error ("\"%s\" is not a SGI file, magic number doesn't match",
               m_filename.c_str());
        close ();
        return false;
    }

    int height = 0;
    int nchannels = 0;
    switch (m_sgi_header.dimension) {
        case sgi_pvt::ONE_SCANLINE_ONE_CHANNEL:
          height = 1;
          nchannels = 1;
          break;
        case sgi_pvt::MULTI_SCANLINE_ONE_CHANNEL:
          height = m_sgi_header.ysize;
          nchannels = 1;
          break;
        case sgi_pvt::MULTI_SCANLINE_MULTI_CHANNEL:
          height = m_sgi_header.ysize;
          nchannels = m_sgi_header.zsize;
          break;
        default:
          error ("Bad dimension: %d", m_sgi_header.dimension);
          close ();
          return false;
    }

    if (m_sgi_header.colormap == sgi_pvt::COLORMAP
            || m_sgi_header.colormap == sgi_pvt::SCREEN) {
        error ("COLORMAP and SCREEN color map types aren't supported");
        close ();
        return false;
    }

    m_spec = ImageSpec (m_sgi_header.xsize, height, nchannels, TypeDesc::UINT8);
    if (strlen (m_sgi_header.imagename))
        m_spec.attribute("ImageDescription", m_sgi_header.imagename);

    if (m_sgi_header.storage == sgi_pvt::RLE) {
        m_spec.attribute("compression", "rle");
        read_offset_tables ();
    }

    spec = m_spec;
    return true;
}



bool
SgiInput::read_native_scanline (int y, int z, void *data)
{
    if (y < 0 || y > m_spec.height)
        return false;

    y = m_spec.height - y - 1;

    std::vector<unsigned char> scanline(m_spec.width * m_spec.nchannels);
    long scanline_off_chan1 = 0;
    long scanline_off_chan2 = 0;
    long scanline_off_chan3 = 0;
    long scanline_off_chan4 = 0;
    std::vector<unsigned char> first_channel;
    std::vector<unsigned char> second_channel;
    std::vector<unsigned char> third_channel;
    std::vector<unsigned char> fourth_channel;
    if (m_sgi_header.storage == sgi_pvt::RLE) {
        // reading and uncompressing first channel (red in RGBA images)
        scanline_off_chan1 = start_tab[y];
        int scanline_len_chan1 = length_tab[y];
        first_channel.resize (m_spec.width);
        uncompress_rle_channel (scanline_off_chan1, scanline_len_chan1,
                                &first_channel[0]);
        if (m_spec.nchannels >= 3) {
            // reading and uncompressing second channel (green in RGBA images)
            scanline_off_chan2 = start_tab[y + m_spec.height];
            int scanline_len_chan2 = length_tab[y + m_spec.height];
            second_channel.resize (m_spec.width);
            uncompress_rle_channel (scanline_off_chan2, scanline_len_chan2,
                                    &second_channel[0]);
            // reading and uncompressing third channel (blue in RGBA images)
            scanline_off_chan3 = start_tab[y + m_spec.height * 2];
            int scanline_len_chan3 = length_tab[y + m_spec.height * 2];
            third_channel.resize (m_spec.width);
            uncompress_rle_channel (scanline_off_chan3, scanline_len_chan3,
                                    &third_channel[0]);
            if (m_spec.nchannels == 4) {
                // reading and uncompressing fourth channel (alpha in RGBA images)
                scanline_off_chan4 = start_tab[y + m_spec.height * 3];
                int scanline_len_chan4 = length_tab[y + m_spec.height * 3];
                fourth_channel.resize (m_spec.width);
                uncompress_rle_channel (scanline_off_chan4, scanline_len_chan4,
                                        &fourth_channel[0]);
            }
        }
    }
    else {
        // first channel (red in RGBA)
        scanline_off_chan1 = sgi_pvt::SGI_HEADER_LEN + y * m_spec.width;
        fseek (m_fd, scanline_off_chan1, SEEK_SET);
        first_channel.resize (m_spec.width);
        fread (&first_channel[0], 1, m_spec.width, m_fd);
        if (m_spec.nchannels >= 3) {
            // second channel (breen in RGBA)
            scanline_off_chan2 = sgi_pvt::SGI_HEADER_LEN +  (m_spec.height + y)
                                 * m_spec.width;
            fseek (m_fd, scanline_off_chan2, SEEK_SET);
            second_channel.resize (m_spec.width);
            fread (&second_channel[0], 1, m_spec.width, m_fd);
            
            // third channel (blue in RGBA)
            scanline_off_chan3 = sgi_pvt::SGI_HEADER_LEN + (2 * m_spec.height + y)
                                 * m_spec.width;
            fseek (m_fd, scanline_off_chan3, SEEK_SET);
            third_channel.resize (m_spec.width);
            fread (&third_channel[0], 1, m_spec.width, m_fd);

            if (m_spec.nchannels == 4) {
                // fourth channel (alpha in RGBA)
                scanline_off_chan4 = sgi_pvt::SGI_HEADER_LEN + (3 * m_spec.height + y)
                                     * m_spec.width;
                fseek (m_fd, scanline_off_chan4, SEEK_SET);
                fourth_channel.resize (m_spec.width);
                fread (&fourth_channel[0], 1, m_spec.width, m_fd);
            }
        }
    }

    if (m_spec.nchannels == 1) {
        memcpy (data, &first_channel[0], first_channel.size());
        return true;
    }

    for (int i = 0, j = 0; i < m_spec.width; i++, j += m_spec.nchannels) {
        scanline[j] = first_channel[i];
        scanline[j+1] = second_channel[i];
        scanline[j+2] = third_channel[i];
        if (m_spec.nchannels == 4)
            scanline[j+3] = fourth_channel[i];
    }

    memcpy (data, &scanline[0], scanline.size());
    return true;
}



void
SgiInput::uncompress_rle_channel(int scanline_off, int scanline_len,
                                 unsigned char *out)
{
    std::vector<unsigned char> rle_scanline(scanline_len);
    fseek (m_fd, scanline_off, SEEK_SET);
    fread (&rle_scanline[0], 1, scanline_len, m_fd);

    int i = 0;
    while (i < scanline_len) {
        unsigned char byte = rle_scanline[i];
        int count = byte & 0x7F;
        i++;
        if (byte & 0x80) {
            while (count--)
                *(out++) = rle_scanline[i++];
        }
        else {
            while (count--)
                *(out++) = rle_scanline[i];
            i++;
        }
    }
}



bool
SgiInput::close()
{
    if (m_fd)
        fclose (m_fd);
    init ();
    return true;
}



void
SgiInput::read_header()
{
    fread(&m_sgi_header.magic, sizeof(m_sgi_header.magic), 1, m_fd);
    fread(&m_sgi_header.storage, sizeof(m_sgi_header.storage), 1, m_fd);
    fread(&m_sgi_header.bpc, sizeof(m_sgi_header.bpc), 1, m_fd);
    fread(&m_sgi_header.dimension, sizeof(m_sgi_header.dimension), 1, m_fd);
    fread(&m_sgi_header.xsize, sizeof(m_sgi_header.xsize), 1, m_fd);
    fread(&m_sgi_header.ysize, sizeof(m_sgi_header.ysize), 1, m_fd);
    fread(&m_sgi_header.zsize, sizeof(m_sgi_header.zsize), 1, m_fd);
    fread(&m_sgi_header.pixmin, sizeof(m_sgi_header.pixmin), 1, m_fd);
    fread(&m_sgi_header.pixmax, sizeof(m_sgi_header.pixmax), 1, m_fd);
    fread(&m_sgi_header.dummy, sizeof(m_sgi_header.dummy), 1, m_fd);
    fread(&m_sgi_header.imagename, sizeof(m_sgi_header.imagename), 1, m_fd);
    m_sgi_header.imagename[79] = '\0';
    fread(&m_sgi_header.colormap, sizeof(m_sgi_header.colormap), 1, m_fd);
    //dont' read dummy bytes
    fseek (m_fd, 404, SEEK_CUR);

    if (littleendian()) {
        swap_endian(&m_sgi_header.magic);
        swap_endian(&m_sgi_header.dimension);
        swap_endian(&m_sgi_header.xsize);
        swap_endian(&m_sgi_header.ysize);
        swap_endian(&m_sgi_header.zsize);
        swap_endian(&m_sgi_header.pixmin);
        swap_endian(&m_sgi_header.pixmax);
        swap_endian(&m_sgi_header.colormap);
    }
}



void
SgiInput::read_offset_tables ()
{
    int tables_size = m_sgi_header.ysize * m_sgi_header.zsize;
    start_tab.resize(tables_size);
    length_tab.resize(tables_size);
    fread (&start_tab[0], sizeof(uint32_t), tables_size, m_fd);
    fread (&length_tab[0], sizeof(uint32_t), tables_size, m_fd);

    if (littleendian ()) {
        swap_endian (&length_tab[0], length_tab.size ());
        swap_endian (&start_tab[0], start_tab.size());
    }
}
