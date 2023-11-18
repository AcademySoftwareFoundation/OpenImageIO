// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/////////////////////////////////////////////////////////////////////////////
// Private definitions internal to the jxl.imageio plugin
/////////////////////////////////////////////////////////////////////////////


#pragma once


OIIO_PLUGIN_NAMESPACE_BEGIN

class JxlInput final : public ImageInput {
public:
    JxlInput() { init(); }
    ~JxlInput() override { close(); }
    const char* format_name(void) const override { return "jxl"; }
    int supports(string_view feature) const override
    {
        return (feature == "exif" || feature == "ioproxy");
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;

    bool open(const std::string& name, ImageSpec& spec) override;
    bool open(const std::string& name, ImageSpec& spec,
              const ImageSpec& config) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool close() override;

    const std::string& filename() const { return m_filename; }

private:
    std::string m_filename;
    int m_next_scanline;   // Which scanline is the next to read?
    JxlDecoderPtr m_decoder;
    JxlResizableParallelRunnerPtr m_runner;
    std::unique_ptr<ImageSpec> m_config;    // Saved copy of configuration spec
    std::vector<uint8_t> m_icc_profile;
    // std::vector<float> m_pixels;
    std::vector<uint8_t> m_pixels;

    void init()
    {
        ioproxy_clear();
        m_config.reset();
        m_decoder = nullptr;
    }

    void close_file() { init(); }

    friend class JxlOutput;
};



OIIO_PLUGIN_NAMESPACE_END
