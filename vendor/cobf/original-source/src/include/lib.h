
//
// Common Library Functions for COBF
// BB 13.11.1994
//

#if !defined __LIB_H

#define __LIB_H

#include <iostream>

#ifdef unix
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'
#else
#define PATH_SEPARATOR "\\"
#define PATH_SEPARATOR_CHAR '\\'
#endif

void sync_streams();
int exec_cmd(const char *cmd);
const char *get_filename(const char *pathname);

void get_string(std::istream &istr, char *line_buffer, int line_bufflen, bool treat_whitespace_as_newline);

int rename_file(const char *oldname, const char * newname, const char *output_dir, int save_flag);

enum { copy_bufflen = 2000 };
extern char copy_buffer[];

int copy_file(const char *srcfilename, const char *dstfilename);
int copy_stream(std::istream &src, std::ostream &dst, void *buffer = copy_buffer, size_t buff_len = copy_bufflen);
bool append_file(const char *appendname, std::ostream &dst);

#endif



