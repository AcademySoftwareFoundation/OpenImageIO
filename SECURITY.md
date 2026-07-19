# Security Policy

## Supported Versions

This gives guidance about which branches are supported with patches to
security vulnerabilities.

| Version / branch  | Supported                                            |
| ----------------- | ---------------------------------------------------- |
| main              | :white_check_mark: :construction: ALL fixes immediately, but this is a branch under development with a frequently unstable ABI and occasionally unstable API. |
| 3.1.x             | :white_check_mark: All fixes that can be backported without breaking ABI compatibility. New tagged releases monthly. |
| 3.0.x             | :warning: Important fixes that can be easily backported without breaking ABI compatibility. New tagged releases as needed, and becoming less frequent over time. |
| <= 2.5.x         | :x: No longer receiving patches of any kind.        |


## Reporting a Vulnerability

If you think you've found a potential vulnerability in OpenImageIO, please
report it to the maintainers. Include detailed steps to reproduce the issue,
and any other information that could aid an investigation.

The best way to report a vulnerability is to file a GitHub [security
advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/new).
If that is not possible, it is also fine to email your report to
security@openimageio.org. Only the project administrators have access to these
reports.

Our policy is to respond to vulnerability reports within 14 days, and to
address critical security vulnerabilities rapidly and post patches quickly,
usually at the next scheduled patch release at the beginning of every month.
For especially severe vulnerabilities, or when a flaw is believed to be
exploited in the wild, we will try to make a special patch release as soon as
possible.


## What do we consider a vulnerability?

A true vulnerability, by our definition, requires the following:
- It allows arbitrary code execution, privilege escalation, exfiltration of
  secrets beyond network boundaries, destruction of data or equipment, or
  similarly serious outcome.
- It can be plausibly exploited by an untrusted party through normal product
  inputs (for example, a maliciously crafted input image).

We would like to hear about these through the confidential GitHub
vulnerability reporting mechanism described above.

The following are examples of problems that we DO NOT consider
vulnerabilities:
- A crash or program termination that does not directly present the
  opportunity for further exploits. This includes hardening assertions that
  terminate the program rather than allow undefined behavior.
- Out-of-memory errors, as long as the program terminates in a reasonable
  amount of time. (But if a tiny crafted input can make it loop infinitely or
  spin for minutes, then it might qualify as a DOS vulnerability. Use your
  best judgment.)
- Misuse or incorrect use of the APIs. We consider all programs directly using
  OIIO's APIs to be trusted.
- Flaws whose root cause lies in a dependency. These should be reported and
  fixed upstream; the upstream project owns the CVE when one is warranted.

Please do not use the GitHub vulnerability reporting mechanism for these, and
instead file ordinary Issues and PRs. We do not support requesting a CVE for
these kinds of problems.

Users operating in an environment in which it might encounter untrusted inputs
that could be malicious, and are be running in a situation where real damage
could occur are expected to:
- Enable all the OpenImageIO build-time options for safety checks, hardening,
  etc. A problem that only occurs when these are disabled is not considered a
  vulnerability.
- Ensure that all the dependencies OIIO builds against are on their latest
  patched versions, and not rely on the OS distribution default, or even
  OIIO's own "dependency auto-build" scripts. A problem that depends on
  having anything but the lastest dependencies is not an OIIO vulnerbility.


## Other security features

### Signed tags
Starting with OpenImageIO 3.0, we cryptographically sign release tags.
To verify a tag, you can use the `git tag -v` command, which will check
the signature against the public key that is included in the repository.
For example,

```bash
git tag -v v3.0.0.3
```

## Outstanding Security Issues

None known


## History of CVE Fixes

Most recent fixes listed first, more or less:

- CVE-2026-63638: Cineon invalid bit depth heap out-of-bounds write [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-9hxv-jvgr-3x8g) / [Fix: PR #5283](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5283) (fixed in 3.1.16.0)
- CVE-2026-63635: PSD RawColor invalid color mode causes global out-of-bounds read and allocation DoS [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-3c8w-9xvm-r6gf) / [Fix: PR #5282](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5282) (fixed in 3.1.16.0)
- CVE-2026-63635: PSD RawColor invalid color mode causes global out-of-bounds read and allocation DoS [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-3c8w-9xvm-r6gf) / [Fix: PR #5282, 3.1.16.0](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5282) (fixed in 3.1.16.0)
- CVE-2026-63422 OpenEXR plugin partial edge tile heap out-of-bounds write [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-xh5r-whph-qmc5) / [Fix: PR #5295](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5295) (fixed in 3.1.16.0)
- CVE-2026-63420: PSD RawColor indexed image out-of-bounds read in `interleave_row` [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-x877-h4xx-5m5j) / [Fix: PR #5307](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5307)  (fixed in 3.1.16.0)
- CVE-2026-63419: OpenImageIO IFF ZBUFFER tile read writes past caller tile buffer [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-w6wc-gcf4-5pj2) / [Fix: PR #5268](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5268) (fixed in 3.1.16.0)
- CVE-2026-59956: Heap-buffer-overread in IffInput::readimg() when ZBUFFER flag is set [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-hjfv-gvxc-qgvh) / [Fix: PR #5251](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5251) (fixed in 3.0.20.0, 3.1.15.0)
- CVE-2026-59181: Stack buffer overflow in OpenImageIO Cineon reader via unchecked numberOfElements [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-xh8r-vmqq-56pp) / [Fix: PR #5250](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5250) (fixed in 3.0.20.0, 3.1.15.0)
- CVE-2026-59156: Unbounded recursion in FITS header parser leads to stack overflow [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-xvwr-x6ch-v2fq) / [Fix: PR #5248](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5248) (fixed in 3.0.20.0, >= 3.1.15.0)
- CVE-2026-50291: Segmentation Fault in BmpInput::read_native_scanline / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-q3c7-3225-66h7) / [Fix: PR #5030](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5030) (Fixed in 3.0.16.0, 3.1.11.0)
- CVE-2026-43909: Signed integer overflow in SwapRGBABytes loop index leads to out-of-bounds read/write in DPX ABGR decoder / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-g267-j53j-5258) / [Fix: PR5170](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5170) (Fixed in 3.0.18.1, 3.1.13.1)
- CVE-2026-43908: Signed integer overflow in ConvertCbYCrYToRGB leads to heap out-of-bounds write in DPX 4:2:2 decoder / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-2jr5-q49v-3858) / [Fix: PR5170](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5170) (Fixed in 3.0.18.1, 3.1.13.1)
- CVE-2026-43907: Integer overflow in QueryRGBBufferSizeInternal leads to heap out-of-bounds write in DPX decoder (kCbYCr and kABGR) / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-cq46-hp4h-cvfr) / [Fix: PR5170](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5170) (Fixed in 3.0.18.1, 3.1.13.1)
- CVE-2026-43996: Integer wraparound in bounds check of decode_pixel leads to out-of-bounds read in TGA paletted image decoder / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-mq8j-73c4-cr55) / [Fix: PR5165](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5165) (Fixed in 3.2.0.1, 3.1.13.0, 3.0.18.0)
- CVE-2026-43906: HEIF Heap overflow / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-gmrp-x952-3m66) / [Fix: PR5166](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5166) (Fixed in 3.2.0.1, 3.1.13.0, 3.0.18.0)
- CVE-2026-43905: JPEG2000 (OpenJPH) signed integer overflow in buffer allocation / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-pj45-cf3g-28gq) / [Fix: PR5143](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5143) (Fixed in 3.2.0.1, 3.1.13.0, 3.0.18.0)
- CVE-2026-43904: Softimage PIC RLE decoder heap buffer overflow — longCount not clamped to image width / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-4499-j545-7q33) / [Fix: PR5142](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5142) (Fixed in 3.2.0.1, 3.1.13.0, 3.0.18.0)
- CVE-2026-43903: SGI RLE decoder heap buffer overflow — OIIO_DASSERT bounds checks are no-ops in release builds / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-jg3q-vm3q-2j35) / [#5141](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5141) (Fixed in 3.2.0.1, 3.1.13.0, 3.0.18.0)
- CVE-2026-7582: DDS Image ddsinput.cpp out-of-bounds write / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-5pm7-8r3j-2x67). [Fix: #5131](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/5131) (Fixed in 3.2.0.1, 3.1.13.0, 3.0.18.0)
- CVE-2024-55194: Broken pgm had memory access error leading to heap overflow / [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-583r-43f7-cw8w) / [#4559](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/4559) (Fixed in 3.0.2.0, 3.1.4.0)
- CVE-2024-40630: Fixed incorrect image size for certain HEIC files.
  [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-jjm9-9m4m-c8p2) (Fixed in 2.5.13.1)
- CVE-2023-42295: Fix signed integer overflow when computing total number of pixels while reading BMP files. [#3948](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3948) (by xiaoxiaoafeifei) (Fixed in 2.5.3.0/2.6.0.1)
- CVE-2023-36183: Heap-buffer-overflow while reading ICO files [#3872](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3872)  (by xiaoxiaoafeifei)
- TALOS-2023-1709 / CVE-2023-24472: Race condition in TIFF reader. [#3772](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3772) (2.5.1.0/2.4.8.1)
- TALOS-2023-1707 / CVE-2023-24473, TALOS-2023-1708 / CVE-2023-22845: Guard against corrupted Targa. [#3768](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3768) (2.5.1.0/2.4.8.1)
- TALOS-2022-1654 / CVE-2022-43596, TALOS-2022-1655 / CVE-2022-43597 CVE-2022-43598, TALOS-2022-1656 / CVE-2022-43599 CVE-2022-43600 CVE-2022-43601 CVE-2022-43602: Fix possible IFF write errors [#3676](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3676) (2.4.6/2.5.0.0)
- TALOS-2022-1653 / CVE-2022-43594: Fix possible errors when writing BMP files. [#3673](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3673) (by lgritz) (2.4.6/2.5.0.0)
- TALOS-2022-1651 / CVE-2022-43592, TALOS-2022-1652 / CVE-2022-4359: Fix possible DPX write errors. [#3672](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3672) (2.4.6/2.5.0.0)
- TALOS-2022-1657 / CVE-2022-43603: Zfile write safety. [#3670](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3670) (2.4.6/2.5.0.0)
- TALOS-2022-1633 / CVE-2022-41639, TALOS-2022-1643 / CVE-2022-41988: Guard TIFF against buffer overflow for certain CMYK files. [#3632](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3632) (2.4.5/2.5.0.0)
- TALOS-2022-1626, CVE-2022-41794: PSD files protect against corrupted embedded thumbnails. [#3629](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3629) (2.4.5/2.5.0.0)
- TALOS-2022-1627, CVE-2022-41977: Guard TIFF reads against corrupt files with buffer overflows. [#3628](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3628) (2.4.5/2.5.0.0)
- TALOS-2022-1626 / CVE-2022-41794, TALOS-2022-1632 / CVE-2022-41684, TALOS-2022-1636 / CVE-2022-41837: Exif (all formats that support it, TIFF/JPEG/PSD) fix bugs where corrupted Exif blocks could overrun memory. [#3627](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3627) (2.4.5/2.5.0.0)
- TALOS-2022-1634 / CVE-2022-41838, TALOS-2022-1635 / CVE-2022-41999: Fix DDS reading crashes for cubemap files when a cube face was not present, and check for invalid bits per pixel. [#3625](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3625) (2.4.5/2.5.0.0) #3625 (2.4.5/2.5.0.0)
- TALOS-2022-1629, CVE-2022-36354: RLA potential buffer overrun. [#3624](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3624) (2.4.5/2.5.0.0)
- TALOS-2022-1628, CVE-2022-41981: Targa file string overflow safety. [#3622](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3622) (2.4.5/2.5.0.0)
- TALOS-2022-1630, CVE-2022-38143: Protect against corrupt pixel coordinates while reading BMP files. [#3620](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/3620) (by lgritz) (Fixed in 2.4.5/2.5.0.0)
<!-- - CVE-xxx: description [advisory](https://github.com/AcademySoftwareFoundation/OpenImageIO/security/advisories/GHSA-yyy) / [Fix: PR #nnnn](https://github.com/AcademySoftwareFoundation/OpenImageIO/pull/nnnn)
-->
