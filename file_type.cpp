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

#include "platform/platform.h"
#include "platform/io.h"
#include "file_type.h"

namespace xPlatform
{

const eFileType* eFileType::FindByName(const char* name)
{
	int l = strlen(name);
	if(!l)
		return NULL;
	const char* ext = name + l;
	while(ext >= name && *ext != '.')
		--ext;
	++ext;
	assert(name + l + 1 - ext < 8);
	char type[name + l + 1 - ext];
	char* t = type;
	while(*ext)
	{
		char c = *ext++;
		if(c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		*t++ = c;
	}
	*t = '\0';
	return Find(type);
}

bool eFileType::Open(const char* name) const
{
#ifndef USE_MU_SIMPLIFICATIONS
	for(const eFileType* t = First(); t; t = t->Next())
	{
		char contain_path[xIo::MAX_PATH_LEN];
		char contain_name[xIo::MAX_PATH_LEN];
		if(t->Contain(name, contain_path, contain_name))
		{
			return t->Open(name);
		}
	}
#endif
#ifndef USE_EMBEDDED_FILES
	FILE* f = fopen(name, "rb");
	if(f)
	{
		fseek(f, 0, SEEK_END);
		size_t size = ftell(f);
		fseek(f, 0, SEEK_SET);
		byte* buf = (byte*)malloc(size);
		size_t r = fread(buf, 1, size, f);
		fclose(f);
		if(r != size)
		{
			SAFE_DELETE_ARRAY(buf);
			return false;
		}
#ifndef USE_STREAM
		bool ok = Open(buf, size);
#else
		struct stream *stream = memory_stream_open(buf, size, true);
		bool ok = Open(stream);
#endif
		free(buf);
		return ok;
	}
#else
	assert(false);
#endif
	return false;
}

}
//namespace xPlatform
