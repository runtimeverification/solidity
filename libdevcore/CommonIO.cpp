/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file CommonIO.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "CommonIO.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#endif
#include <boost/filesystem.hpp>
#include "Assertions.h"

using namespace std;
using namespace dev;

namespace
{

template <typename _T>
inline _T readFile(std::string const& _file)
{
	_T ret;
	size_t const c_elementSize = sizeof(typename _T::value_type);
	std::ifstream is(_file, std::ifstream::binary);
	if (!is)
		return ret;

	// get length of file:
	is.seekg(0, is.end);
	streamoff length = is.tellg();
	if (length == 0)
		return ret; // do not read empty file (MSVC does not like it)
	is.seekg(0, is.beg);

	ret.resize((length + c_elementSize - 1) / c_elementSize);
	is.read(const_cast<char*>(reinterpret_cast<char const*>(ret.data())), length);
	return ret;
}

}

string dev::readFileAsString(string const& _file)
{
	return readFile<string>(_file);
}

string dev::readStandardInput()
{
	string ret;
	while (!cin.eof())
	{
		string tmp;
		// NOTE: this will read until EOF or NL
		getline(cin, tmp);
		ret.append(tmp);
		ret.append("\n");
	}
	return ret;
}

void dev::writeFile(std::string const& _file, bytesConstRef _data, bool _writeDeleteRename)
{
	namespace fs = boost::filesystem;
	if (_writeDeleteRename)
	{
		fs::path tempPath = fs::unique_path(_file + "-%%%%%%");
		writeFile(tempPath.string(), _data, false);
		// will delete _file if it exists
		fs::rename(tempPath, _file);
	}
	else
	{
		// create directory if not existent
		fs::path p(_file);
		if (!fs::exists(p.parent_path()))
		{
			fs::create_directories(p.parent_path());
			try
			{
				fs::permissions(p.parent_path(), fs::owner_all);
			}
			catch (...)
			{
			}
		}

		ofstream s(_file, ios::trunc | ios::binary);
		s.write(reinterpret_cast<char const*>(_data.data()), _data.size());
		assertThrow(s, FileError, "Could not write to file: " + _file);
		try
		{
			fs::permissions(_file, fs::owner_read|fs::owner_write);
		}
		catch (...)
		{
		}
	}
}

#if defined(_WIN32)
class DisableConsoleBuffering
{
public:
	DisableConsoleBuffering()
	{
		m_stdin = GetStdHandle(STD_INPUT_HANDLE);
		GetConsoleMode(m_stdin, &m_oldMode);
		SetConsoleMode(m_stdin, m_oldMode & (~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)));
	}
	~DisableConsoleBuffering()
	{
		SetConsoleMode(m_stdin, m_oldMode);
	}
private:
	HANDLE m_stdin;
	DWORD m_oldMode;
};
#else
class DisableConsoleBuffering
{
public:
	DisableConsoleBuffering()
	{
		tcgetattr(0, &m_termios);
		m_termios.c_lflag &= ~ICANON;
		m_termios.c_lflag &= ~ECHO;
		m_termios.c_cc[VMIN] = 1;
		m_termios.c_cc[VTIME] = 0;
		tcsetattr(0, TCSANOW, &m_termios);
	}
	~DisableConsoleBuffering()
	{
		m_termios.c_lflag |= ICANON;
		m_termios.c_lflag |= ECHO;
		tcsetattr(0, TCSADRAIN, &m_termios);
	}
private:
	struct termios m_termios;
};
#endif

int dev::readStandardInputChar()
{
	DisableConsoleBuffering disableConsoleBuffering;
	return cin.get();
}

boost::filesystem::path dev::weaklyCanonicalFilesystemPath(boost::filesystem::path const &_path)
{
	if (boost::filesystem::exists(_path))
		return boost::filesystem::canonical(_path);
	else
	{
		boost::filesystem::path head(_path);
		boost::filesystem::path tail;
		for (auto it = --_path.end(); !head.empty(); --it)
		{
			if (boost::filesystem::exists(head))
				break;
			tail = (*it) / tail;
			head.remove_filename();
		}
		head = boost::filesystem::canonical(head);
		return head / tail;
	}
}
