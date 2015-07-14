/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2012 Live Networks, Inc.  All rights reserved.
// Media Sources
// C++ header

#ifndef _MEDIA_SOURCE_HH
#define _MEDIA_SOURCE_HH

#ifndef _MEDIA_HH
#include "Media.hh"
#endif

// 媒体来源类
class MediaSource : public Medium {
public:
	static Boolean lookupByName(UsageEnvironment& env, char const* sourceName,
		MediaSource*& resultSource);

	virtual void getAttributes() const;
	// attributes are returned in "env's" 'result message'
	// 返回的属性在env的result message中

	// The MIME type of this source:
	// 本source的MIME类型：
	virtual char const* MIMEtype() const;

	// Test for specific types of source:
	// 注意，这些是新定义的，不是从基类继承来的
	virtual Boolean isFramedSource() const;
	virtual Boolean isRTPSource() const;
	virtual Boolean isMPEG1or2VideoStreamFramer() const;
	virtual Boolean isMPEG4VideoStreamFramer() const;
	virtual Boolean isH264VideoStreamFramer() const;
	virtual Boolean isDVVideoStreamFramer() const;
	virtual Boolean isJPEGVideoSource() const;
	virtual Boolean isAMRAudioSource() const;

protected:
	MediaSource(UsageEnvironment& env); // abstract base class抽象基类
	virtual ~MediaSource();

private:
	// redefined virtual functions:重定义虚函数
	virtual Boolean isSource() const;	//返回true
};

#endif
