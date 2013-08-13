///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_ENVMAP_IMAGE_H
#define INCLUDED_ENVMAP_IMAGE_H

//-----------------------------------------------------------------------------
//
//	class EnvmapImage
//
//-----------------------------------------------------------------------------

#include <ImfArray.h>
#include <ImfRgba.h>
#include <ImfEnvmap.h>
#include <ImathBox.h>
#include "namespaceAlias.h"


class EnvmapImage
{
  public:

      EnvmapImage ();
      EnvmapImage (CustomImf::Envmap type,
                   const IMATH_NAMESPACE::Box2i &dataWindow);
      
      void				resize (CustomImf::Envmap type,
	      					const IMATH_NAMESPACE::Box2i &dataWindow);

      void				clear ();

      CustomImf::Envmap	type () const;
      const IMATH_NAMESPACE::Box2i &		dataWindow () const;

      CustomImf::Array2D<CustomImf::Rgba> &
                                        pixels ();
      const CustomImf::Array2D<CustomImf::Rgba> &
                                        pixels () const;
      
      CustomImf::Rgba 	filteredLookup (IMATH_NAMESPACE::V3f direction,
					float radius,
					int numSamples) const;

  private:
      
      CustomImf::Rgba 	sample (const IMATH_NAMESPACE::V2f &pos) const;

      CustomImf::Envmap	_type;
      IMATH_NAMESPACE::Box2i                              _dataWindow;
      CustomImf::Array2D<CustomImf::Rgba>	_pixels;
};


#endif
