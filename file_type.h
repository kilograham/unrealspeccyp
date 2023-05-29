/*
Portable ZX-Spectrum emulator.
Copyright (C) 2001-2012 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
Copyright (C) 2023 Graham Sanderson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __FILE_TYPE_H__
#define __FILE_TYPE_H__

#include "tools/list.h"

#pragma once

#ifdef USE_STREAM
#include "stream.h"
#endif
namespace xIo
{
class eFileSelect;
}
//namespace xIo

namespace xPlatform
{

struct eFileType : public eList<eFileType>
{
	static const eFileType* First() { return _First(); }
#ifndef USE_STREAM
	virtual bool Open(const void* data, size_t data_size) const = 0;
#else
	virtual bool Open(struct stream *stream) const = 0;
#endif
	virtual bool Open(const char* name) const;
	virtual bool Store(const char* name) const { return false; }
	virtual bool AbleOpen() const { return true; }
	virtual bool Contain(const char* name, char* contain_path, char* contain_name) const { return false; }
	virtual const char* Type() const = 0;
	virtual xIo::eFileSelect* FileSelect(const char* path) const { return NULL; }

	static const eFileType* Find(const char* type)
	{
		for(const eFileType* t = First(); t; t = t->Next())
		{
			if(strcmp(t->Type(), type) == 0)
				return t;
		}
		return NULL;
	}
	static const eFileType* FindByName(const char* name);
};

}
//namespace xPlatform

#endif//__FILE_TYPE_H__
