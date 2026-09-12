//
// Common Library Functions
// Author: BB
// History:
// 1994-11-13	Created
// 2005-12-20	Update to ANSI-C++

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <cerrno>

#include "b_str.h"
#include "lib.h"

using namespace std;

void sync_streams()
{
	fflush(stdout);
	fflush(stderr);
	cout.flush();
	cerr.flush();
}

#if __WIN == 0
int exec_cmd(const char *cmd)
{
	sync_streams();
//	cout << "system(\"" << cmd << "\")\n";
	return system(cmd);
}
#else
int exec_cmd(const char *cmd)
{
	sync_streams();
	cerr << "can't call '" << cmd << "'\n";
	return 1;
}

#endif

// removes charsequence from stream "istr" with maximum length "line_bufflen"

void get_string(istream &istr, char *line_buffer, int line_bufflen, bool treat_whitespace_as_newline)
{
	int i = 0;
	char c;
	while (!istr.eof())
	{
		c = istr.get();
		if (!istr || istr.eof()) break;
		if (c == '\n' || c == '\r') break;
		if (treat_whitespace_as_newline)
			if (c == ' ' || c == '\t') break;
		line_buffer[i++] = c;
		if (i >= line_bufflen - 2) break;
	}
	line_buffer[i] = '\0';
	// trim string
	while (true)
	{
		c = line_buffer[0];
		if (c == '\t' || c == ' ')
			strcpy(line_buffer, &line_buffer[1]);
		else
			break;
	}
	while (true)
	{
		size_t len = strlen(line_buffer);
		if (len == 0) break;
		c = line_buffer[len-1];
		if (c == '\t' || c == ' ')
			line_buffer[len-1] = '\0';
		else
			break;
	}
}

// returns the filename part from string "pathname"

const char *get_filename(const char *pathname)
{
	const char *p;
	if ((p = strrchr(pathname, PATH_SEPARATOR_CHAR)) == NULL)
#ifndef unix
		p = strrchr(pathname, ':')
#endif
		;
	if (p != NULL)
		return p + 1;
	return pathname;
}

// renames "oldname" to "newname" in directory "output_dir"
// save_flag != 0  ==> save file "oldname" as xxx.tmp where xxx is a counter

int rename_file(const char *oldname, const char * newname, const char *output_dir, int save_flag)
{
//	cout << "rename_file(\"" << oldname << "\", \"" << newname << "\")\n";
	if (remove(newname))
    {
		if (errno != ENOENT)
        {
            cout << "info: can't remove '" << newname << "'!\n";
            cout << "errorcode: " << strerror(errno) << "\n";
        }
    }
	if (save_flag)
	{
		static int save_count;
		ostringstream dstfilename;
		dstfilename << output_dir << PATH_SEPARATOR << save_count++ << ".tmp";
		copy_file(oldname, dstfilename.str().c_str());
	}
	if (rename(oldname, newname))
	{
		cerr << "error: can't rename '" << oldname << "' to '" << newname << "'!\n";
        cerr << "ERROR: " << strerror(errno) << "\n";
		return 1;
	}
	return 0;
}



// copy "srcfilename" to "dstfilename"

char copy_buffer[copy_bufflen];

int copy_file(const char *srcfilename, const char *dstfilename)
{
	ifstream src(srcfilename, ios::in | ios::binary );
	if (!src)
	{
		cerr << "error: can't read '" << srcfilename << "'!\n";
		return 1;
	}
	ofstream dst(dstfilename, ios::out | ios::binary);
//	cout << "copy_file(\"" << srcfilename << "\", \"" << dstfilename << "\")\n";
	if (!dst)
	{
		cerr << "error: can't write '" << dstfilename << "'!\n";
		src.close();
		return 1;
	}
	int ret = copy_stream(src, dst);

	if (ret)
		cout << "error while copying '" << srcfilename << "' to '" << dstfilename <<"'!\n";
//	else
//		cout << "'" << srcfilename << "' to '" << dstfilename <<"' copied!\n";

	src.close();
	dst.close();
	return ret;
}

// copy istream "src" to ostream "dst" and use "buffer" (size "buff_len")

int copy_stream(istream &src, ostream &dst, void *buffer, size_t buff_len)
{
	size_t bytes_read;
	size_t total_bytes_read = 0;
	for (;;)
	{
		if (src.eof() || src.fail())
			break;
		src.read((char *)buffer, buff_len);
		if (src.bad())	// if (!src) geht nicht, da i. a. nicht buff_len
				// Bytes gelesen weden
			return -1;
		bytes_read = src.gcount();
		if (bytes_read == 0)
			break;
		total_bytes_read += bytes_read;
		dst.write((char*)buffer, bytes_read);
		if (!dst)
			return -1;
	}
//	cout << total_bytes_read << " Bytes read\n";
	return 0;
}

// appends ostream "dst" to file "appendname"

bool append_file(const char *appendname, ostream &dst)
{
	ifstream src(appendname, ios::in | ios::binary );
	if (!src)
	{
		cout << "error: can't open '" << appendname << "'!\n";
		return true;
	}
	int ret = copy_stream(src, dst);
	src.close();
	if (ret)
	{
		cout << "error while appending '" << appendname << "'!\n";
	}
	return false;
}

