See [LICENSE.md](LICENSE.md) for the main open source license of original
code written for the OpenImageIO project, which is the Apache-2.0 license.

The remainder of this file reproduces the open source licensing details
of other projects that have been imported, incorporated into, or derived
into parts of OIIO.

In no particular order:

-------------------------------------------------------------------------

BSD 3-Clause License (https://opensource.org/license/bsd-3-clause)
SPDX-License-Identifier: BSD-3-Clause

* OpenImageIO prior to July 1, 2023

  Code that was contributed to OpenImageIO prior to July 1 2023, and has not
  yet been [relicensed](RELICENSING.md), was contributed under the BSD
  3-clause license. Currently, less than 0.15% of the codebase (by lines of
  code) remains under this license.

* Gelato, Copyright (c) 2004 by NVIDIA Corp.

  The initial (version 0.1) code found in imageio.h, fmath.h, filter.h,
  argparse, and the JPEG reader/writer had elements derived from
  BSD-licensed example code and headers from NVIDIA's Gelato Renderer. It's
  not clear that any of that original code still survives in the modern
  OIIO, but since some elements are derived from that, we still acknowledge
  the original copyright here.

* Open Shading Language. Copyright contributors to the OSL project.
  https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

  A few parts here and there, notably parts of fmath.h and many of the CMake
  files, contain code that was originally developed for Open Shading
  Language but subsequently moved to OIIO.

* OpenColorIO (c) Copyright contributors to the OpenColorIO Project.
  https://github.com/AcademySoftwareFoundation/OpenColorIO

  The sample OpenColorIO configurations in our testsuite are borrowed from
  this ASWF project, also BSD-3-Clause licensed.

* OpenEXR/Imath (c) Copyright contributors to the OpenEXR Project.
  https://github.com/AcademySoftwareFoundation/Imath

  Some templates in vecparam.h were first developed for Imath.

* DPX reader/writer (c) Copyright 2009 Patrick A. Palmer.
  https://github.com/patrickpalmer/dpx

* KissFFT (c) Copyright 2003-2010 Mark Borgerding
  https://github.com/mborgerding/kissfft

* FindOpenVDB.cmake (c) Copyright 2015 Blender Foundation, BSD license.

* Cryptomatte project, Copyright (c) 2014, 2015, 2016 Psyop Media Company,
  LLC. https://github.com/Psyop/Cryptomatte

  The test image in testsuite/cryptomatte/src/cryptoasset.exr is a small crop
  of a sample image from the Cryptomatte project.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

   - Neither the name of the copyright holder nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------

BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
SPDX-License-Identifier: BSD-2-Clause

* xxHash - Fast Hash algorithm
  Copyright (C) 2012-2014, Yann Collet.
  You can contact the author at :
    - xxHash source repository : https://github.com/Cyan4973/xxHash
    - public discussion board : https://groups.google.com/forum/#!forum/lz4c


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the following disclaimer
  in the documentation and/or other materials provided with the
  distribution.

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

-------------------------------------------------------------------------

MIT License
SPDX-License-Identifier: MIT

* PugiXML http://pugixml.org/ https://github.com/zeux/pugixml
  Copyright (c) 2006-2018 Arseny Kapoulkine

* FarmHash, by Geoff Pike https://github.com/google/farmhash
  (c) Copyright 2014 Google, Inc., MIT license.

* FindTBB.cmake. Copyright (c) 2015 Justus Calvin

* {fmt} library - https://github.com/fmtlib/fmt
  Copyright (c) 2012 - present, Victor Zverovich

* UTF-8 decoder function from http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
  Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>

* fasthash
  Copyright (C) 2012 Zilong Tan (eric.zltan@gmail.com)


Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

-------------------------------------------------------------------------

Apache 2.0 License.
SPDX-License-Identifier: Apache-2.0

* CTPL thread pool https://github.com/vit-vit/CTPL
  Copyright 2014 Vitaliy Vitsentiy.

* Droid fonts from the Android SDK. http://www.droidfonts.com
  Copyright Google, Inc.

* function_view.h contains code derived from LLVM. https://llvm.org
  Copyright (c) 2003-2018 University of Illinois at Urbana-Champaign.
  This is licensed under the Apache 2.0 license with LLVM Exceptions.
  https://llvm.org/docs/DeveloperPolicy.html#open-source-licensing-terms

* ninjatracing.py utility for build profiling is Apache-2.0 licensed
  and comes from https://github.com/nico/ninjatracing.
  Copyright 2018 Nico Weber.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

-------------------------------------------------------------------------

Boost Software License - Version 1.0
SPDX-License-Identifier: BSL-1.0

* Some math functions in fmath.h are derived from the
  [Sleef library](https://sleef.org)

  Copyright Naoki Shibata and contributors 2010 - 2017.
  https://github.com/shibatch/sleef/blob/master/LICENSE.txt

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

-------------------------------------------------------------------------

Zlib license
SPDX-License-Identifier: Zlib

* Base-64 encoder from http://www.adp-gmbh.ch/cpp/common/base64.html
  Copyright (C) 2004-2008 René Nyffenegger


This source code is provided 'as-is', without any express or implied
warranty. In no event will the author be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this source code must not be misrepresented; you must not
   claim that you wrote the original source code. If you use this source code
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original source code.
3. This notice may not be removed or altered from any source distribution.

René Nyffenegger rene.nyffenegger@adp-gmbh.ch

-------------------------------------------------------------------------

Unlicense
SPDX-License-Identifier: Unlicense

* gif.h by Charlie Tangora. Public domain / Unlicense.
  https://github.com/ginsweater/gif-h

* bcdec.h by Sergii "iOrange" Kudlai. MIT or Unlicense.
  https://github.com/iOrange/bcdec


This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>

-------------------------------------------------------------------------

Public domain

* lookup3 code by Bob Jenkins, Public Domain.
  http://burtleburtle.net/bob/c/lookup3.c

* The SHA-1 implementation we use is public domain by Dominik Reichl.
  http://www.dominik-reichl.de

-------------------------------------------------------------------------

If we have left anything out, it is unintentional. Please let us know.

