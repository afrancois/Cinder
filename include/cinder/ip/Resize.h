/*
 Copyright (c) 2010, The Barbarian Group
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "cinder/Surface.h"
#include "cinder/Filter.h"
#include "cinder/Rect.h"

namespace cinder { namespace ip {

template<typename T>
void resize( const SurfaceT<T> &srcSurface, SurfaceT<T> *dstSurface, const FilterBase &filter = FilterTriangle() );
template<typename T>
void resize( const ChannelT<T> &srcChannel, ChannelT<T> *dstChannel, const FilterBase &filter = FilterTriangle() );
template<typename T>
void resize( const SurfaceT<T> &srcSurface, const Area &srcArea, SurfaceT<T> *dstSurface, const Area &dstArea, const FilterBase &filter = FilterTriangle() );
//! Returns a new Surface which is a copy of \a srcSurface's area \a srcArea scaled to size \a dstSize using filter \a filter
template<typename T>
SurfaceT<T> resizeCopy( const SurfaceT<T> &srcSurface, const Area &srcArea, const Vec2i &dstSize, const FilterBase &filter = FilterTriangle() );
template<typename T>
void resize( const ChannelT<T> &srcChannel, const Area &srcArea, ChannelT<T> *dstChannel, const Area &dstArea, const FilterBase &filter = FilterTriangle() );

} } // namespace cinder::ip
