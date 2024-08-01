// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// \file
/// Implementation of ImageBufAlgo demosaic algorithms

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>

#include "imageio_pvt.h"

OIIO_NAMESPACE_BEGIN

template<class Rtype, class Atype>
static bool
bayer_demosaic_impl(ImageBuf& dst, const ImageBuf& src,
                    ImageBufAlgo::BayerPattern bayer_pattern, ROI roi,
                    int nthreads)
{
    int x_offset = (bayer_pattern >> 0) & 1;
    int y_offset = (bayer_pattern >> 1) & 1;

    const ImageSpec& spec = src.spec();
    int src_xbegin        = spec.x;
    int src_xend          = spec.x + spec.width;
    int src_ybegin        = spec.y;
    int src_yend          = spec.y + spec.height;

    ImageBufAlgo::parallel_image(roi, nthreads, [&](ROI roi) {
        Atype box[3][3];

        ImageBuf::Iterator<Rtype> it(dst, roi);

        for (int y = roi.ybegin; y < roi.yend; y++) {
            int line_index[3] = { 0, 1, 2 };

            if (y <= src_ybegin)
                line_index[0] = 2;
            if (y > src_yend - 2)
                line_index[2] = 0;

            // This holds up to 3 lines' pixel iterators.
            // The first or the last one can be unused if we are
            // On the top or bottom line of the image.
            // The objects constructed here are just placeholders,
            // as the iterator doesn't have a default constuctor.
            ImageBuf::ConstIterator<Atype> line_iter[3]
                = { ImageBuf::ConstIterator<Atype>(src),
                    ImageBuf::ConstIterator<Atype>(src),
                    ImageBuf::ConstIterator<Atype>(src) };

            for (int x = roi.xbegin; x < roi.xend; x++) {
                if (x == roi.xbegin) {
                    // Fetch initial box state

                    if (roi.xbegin > src_xbegin) {
                        for (int i = 0; i < 3; i++) {
                            if (line_index[i] == i) {
                                auto it = ImageBuf::ConstIterator<Atype>(
                                    src, roi.xbegin - 1, y - 1 + i, 0);
                                box[i][0] = it[0];
                                it++;
                                box[i][1] = it[0];
                                it++;
                                box[i][2] = it[0];
                                it++;
                                line_iter[i] = it;
                            }
                        }
                    } else {
                        for (int i = 0; i < 3; i++) {
                            if (line_index[i] == i) {
                                auto it = ImageBuf::ConstIterator<Atype>(
                                    src, roi.xbegin, y - 1 + i, 0);
                                box[i][1] = it[0];
                                it++;
                                box[i][2] = it[0];
                                it++;
                                box[i][0]    = box[i][2];
                                line_iter[i] = it;
                            }
                        }
                    }
                } else {
                    // Update the box state

                    if (x < src_xend - 1) {
                        for (int i = 0; i < 3; i++) {
                            if (line_index[i] == i) {
                                auto& it  = line_iter[i];
                                box[i][0] = box[i][1];
                                box[i][1] = box[i][2];
                                box[i][2] = it[0];
                                it++;
                            }
                        }
                    } else {
                        for (int i = 0; i < 3; i++) {
                            if (line_index[i] == i) {
                                Atype tmp = box[i][0];
                                box[i][0] = box[i][1];
                                box[i][1] = box[i][2];
                                box[i][2] = tmp;
                            }
                        }
                    }
                }

                Rtype r = 0;
                Rtype g = 0;
                Rtype b = 0;

                if ((x + x_offset) & 1) {
                    if ((y + y_offset) & 1) {
                        // RGR
                        // GBG
                        // RGR

                        r += box[line_index[0]][0];
                        r += box[line_index[0]][2];
                        r += box[line_index[2]][0];
                        r += box[line_index[2]][2];
                        r /= (Rtype)4;

                        g += box[line_index[0]][1];
                        g += box[line_index[2]][1];
                        g += box[line_index[1]][0];
                        g += box[line_index[1]][2];
                        g /= (Rtype)4;

                        b = box[line_index[1]][1];
                    } else {
                        // GBG
                        // RGR
                        // GBG

                        r += box[line_index[1]][0];
                        r += box[line_index[1]][2];
                        r /= (Rtype)2;

                        g = box[line_index[1]][1];

                        b += box[line_index[0]][1];
                        b += box[line_index[2]][1];
                        b /= (Rtype)2;
                    }
                } else {
                    if ((y + y_offset) & 1) {
                        // GRG
                        // BGB
                        // GRG

                        r += box[line_index[0]][1];
                        r += box[line_index[2]][1];
                        r /= (Rtype)2;

                        g = box[line_index[1]][1];

                        b += box[line_index[1]][0];
                        b += box[line_index[1]][2];
                        b /= (Rtype)2;
                    } else {
                        // BGB
                        // GRG
                        // BGB

                        r = box[line_index[1]][1];

                        g += box[line_index[0]][1];
                        g += box[line_index[2]][1];
                        g += box[line_index[1]][0];
                        g += box[line_index[1]][2];
                        g /= (Rtype)4;

                        b += box[line_index[0]][0];
                        b += box[line_index[0]][2];
                        b += box[line_index[2]][0];
                        b += box[line_index[2]][2];
                        b /= (Rtype)4;
                    }
                }

                it[0] = r;
                it[1] = g;
                it[2] = b;

                it++;
            }
        }
    });

    return true;
}

bool
ImageBufAlgo::bayer_demosaic(ImageBuf& dst, const ImageBuf& src,
                             BayerPattern bayer_pattern, ROI roi, int nthreads)
{
    pvt::LoggedTimer logtime("IBA::bayer_demosaic");

    ROI dst_roi = roi;
    if (!dst_roi.defined()) {
        dst_roi = src.roi_full();
    }

    dst_roi.chbegin = 0;
    dst_roi.chend   = 2;

    ImageSpec dst_spec = src.spec();
    dst_spec.nchannels = 3;
    dst_spec.default_channel_names();
    dst_spec.channelformats.clear();
    dst_spec.alpha_channel = -1;
    dst_spec.z_channel     = -1;

    IBAprep(dst_roi, &dst, &src, nullptr, &dst_spec);

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2(ok, "bayer_demosaic", bayer_demosaic_impl,
                                dst.spec().format, src.spec().format, dst, src,
                                bayer_pattern, dst_roi, nthreads);
    return ok;
}

ImageBuf
ImageBufAlgo::bayer_demosaic(const ImageBuf& src, BayerPattern bayer_pattern,
                             ROI roi, int nthreads)
{
    ImageBuf result;
    bool ok = add(result, src, bayer_pattern, roi, nthreads);
    if (!ok && !result.has_error())
        result.errorfmt("ImageBufAlgo::bayer_demosaic() error");
    return result;
}

OIIO_NAMESPACE_END
