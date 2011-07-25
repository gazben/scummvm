/* ScummVM - Graphic Adventure Engine
*
* ScummVM is the legal property of its developers, whose names
* are too numerous to list here. Please refer to the COPYRIGHT
* file distributed with this source distribution.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef TOON_RESOURCE_H
#define TOON_RESOURCE_H

#include "common/array.h"
#include "common/str.h"
#include "common/file.h"
#include "common/stream.h"

#define MAX_CACHE_SIZE	(4 * 1024 * 1024)

namespace Toon {

class PakFile {
public:
	PakFile();
	~PakFile();

	void open(Common::SeekableReadStream *rs, Common::String packName);
	uint8 *getFileData(Common::String fileName, uint32 *fileSize);
	Common::String getPackName() { return _packName; }
	Common::SeekableReadStream *createReadStream(Common::String fileName);
	void close();

protected:
	struct File {
		char _name[13];
		int32 _offset;
		int32 _size;
	};
	Common::String _packName;

	uint32 _numFiles;
	Common::Array<File> _files;
	Common::File *_fileHandle;
};

class ToonEngine;

class CacheEntry {
public:
	CacheEntry() : _age(0), _size(0), _data(0) {}
	~CacheEntry() {
		free(_data);
	}

	Common::String _packName;
	Common::String _fileName;
	uint32 _age;
	uint32 _size;
	uint8 *_data;
};

class Resources {
public:
	Resources(ToonEngine *vm);
	~Resources();
	void openPackage(Common::String file);
	void closePackage(Common::String fileName);
	Common::SeekableReadStream *openFile(Common::String file);
	uint8 *getFileData(Common::String fileName, uint32 *fileSize); // this memory must be copied to your own structures!
	void purgeFileData();

protected:
	ToonEngine *_vm;
	Common::Array<uint8 *> _allocatedFileData;
	Common::Array<PakFile *> _pakFiles;
	uint32 _cacheSize;
	Common::Array<CacheEntry *> _resourceCache;

	void removePackageFromCache(Common::String packName);
	bool getFromCache(Common::String fileName, uint32 *fileSize, uint8 **fileData);
	void addToCache(Common::String packName, Common::String fileName, uint32 fileSize, uint8 *fileData);
};

} // End of namespace Toon
#endif
