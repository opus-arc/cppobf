// 
// BLIB                                                                   
// Interface Cursor Streams 
// Author: BB
// History:
// 1994-10-01	Created
// 1995-07-27	1.0
// 2005-12-19   Update to ANSI-C++
//


#if !defined __B_STR_H

#define __B_STR_H

#include <sstream>

class cstr: public std::ostringstream
{
public:

	enum { NoTabCompr, StdTabCompr, FullTabCompr };
	enum { ActPos = -1, NoMargin = -2 };

	cstr(std::ostream &);
	~cstr();

	void tee(std::ostream &);
	void untee();

	void sync();
	void tab(int tabpos);
	void vtab(int lineno);
	void set_tabwidth(int tabwidth);
	void set_leftmargin(int leftmargin = ActPos);
	void set_rightmargin(int rightmargin = ActPos);
	void set_wordwrap(int flag);
	void set_tabcompr(int flag);
	void set_endline(const char *str);
	void set_worddelim(const char *str);

	int getx();
	int gety();

	void start_paragraph(const char *end_line = "\n");
	void end_paragraph();

private:
	std::ostream *main_str, *tee_str;
	int col, line;
	int left_margin, right_margin;
	int tab_width, word_wrap, tab_compr;
	const char *end_line;
	const char *word_delim;

	long slen;			// only valid during call of flush_buffer()
	const char *sptr;

	int wlen;
	int state;

	int next_char();
	int next_word_len();

	int is_word_delim(char c);

	void output_l1(char c);
	void output_l2(char c);
	void output_spaces_l3(int count);
	void output_l3(char c);
	void output_l4(char c);
	void output_l5(char c);

	void flush_buffer();
};

#endif
