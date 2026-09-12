// 
// BLIB                                                                   
// Implementation Cursor Streams
// Author: BB
// History:
// 1994-10-01	Created
// 1995-07-27	1.0
// 2005-12-19   Update to ANSI-C++
//

#include "b_str.h"
#include <cstring>

using namespace std;

enum { BeginWord, InWord };
enum { StdTabWidth = 8 };
enum { DefaultRightMargin = 75 };

cstr::cstr(ostream &ostr):
main_str(&ostr), tee_str(0),
col(0), line(0), left_margin(0), right_margin(DefaultRightMargin),
tab_width(StdTabWidth), word_wrap(1), tab_compr(StdTabCompr),
end_line("\n"), word_delim(" \t\n\r"),
wlen(0), state(BeginWord)
{
}

cstr::~cstr()
{
	sync();
}

void cstr::sync()
{
	flush_buffer();

	main_str->flush();
	if (tee_str)
		tee_str->flush();

	seekp(0);	    // new write position of the ostrstream
}

int cstr::getx()
{
	sync();
	return col;
}

int cstr::gety()
{
	sync();
	return line;
}

void cstr::tee(ostream &ostr)
{
	sync();
	tee_str = &ostr;
}

void cstr::untee()
{
	sync();
	tee_str = 0;
}

void cstr::set_tabwidth(int tabwidth)
{
	sync();
	tab_width = tabwidth;
	if (tab_width < 0)
		tab_width = 1;
}

void cstr::set_leftmargin(int leftmargin)
{
	sync();
	if (leftmargin == ActPos)
		left_margin = col;
	else
		left_margin = leftmargin;
}

void cstr::set_rightmargin(int rightmargin)
{
	sync();
	if (rightmargin == ActPos)
		right_margin = col;
	else
		right_margin = rightmargin;
}

void cstr::tab(int tab_pos)
{
	sync();
	output_spaces_l3(tab_pos - col); // right margin won't be checked!
}


void cstr::vtab(int lineno)
{
	sync();
	while (line < lineno)
		output_l4('\n');
}

void cstr::set_wordwrap(int flag)
{
	sync();
	word_wrap = flag;
}

void cstr::set_tabcompr(int flag)
{
	sync();
	tab_compr = flag;
}

void cstr::set_endline(const char *str)
{
	sync();
	end_line = str;
}

void cstr::set_worddelim(const char *str)
{
	sync();
	word_delim = str;
}

void cstr::start_paragraph(const char *endline)
{
	set_leftmargin();
	set_endline(endline);
}

void cstr::end_paragraph()
{
	set_leftmargin(0);
	set_endline("\n");
	*this << "\n";
}

//////////////////////////////////////////////////////////////

// character output Layer 1
// output to main output streamand eventually the tee stream

void cstr::output_l1(char c)
{
	if (main_str)
		*main_str << c;
	if (tee_str)
		*tee_str << c;
}

// character output Layer 2
// Cursor (variables col + line) will be updated

void cstr::output_l2(char c)
{
	switch (c)
	{
		case '\t':
			col += (StdTabWidth - (col % StdTabWidth));
			break;
		case '\n':
			++line;
			col = 0;
			break;
		default:
			++col;
			break;
	}
	output_l1(c);
}

// character output layer 3 helper function
// sequences of white spaces will be compressed to tabs 
// if tab compression-Mode = FullTabCompr


void cstr::output_spaces_l3(int count) // Layer 3 function
{
	if (count <= 0) return;
	if (tab_compr == FullTabCompr)
	{
		int nspaces;
		while ((nspaces = StdTabWidth - (col % StdTabWidth)) <= count)
			output_l2('\t'), count -= nspaces;
	}
	while (count--)
		output_l2(' ');

}

// character output layer 3
// treats tab character dependendant of the actual tab width
// and creates indent according to the left margin setting

void cstr::output_l3(char c)
{
	if (col < left_margin)
		output_spaces_l3(left_margin - col); // right margin will be checked!

	switch (c)
	{
		case '\t':
			if (tab_width == StdTabWidth && tab_compr != NoTabCompr)
				output_l2('\t');
			else
				output_spaces_l3(tab_width - (col % tab_width));
			break;
		default:
			output_l2(c);
			break;
	}
}

// character output Layer 4
// Expands Newline characters to the end_line string

void cstr::output_l4(char c)
{
	const char *pc;
	switch (c)
	{
		case '\n':
			pc = end_line;
			while (*pc)
				output_l3(*pc++);
			break;
		default:
			output_l3(c);
	}
}

// character output Layer 5
// treats word wrap and also checks whether right margin is reached

void cstr::output_l5(char c)
{
	if (right_margin == NoMargin)
	{
		output_l4(c);
		return;
	}

	if (c != '\n')
	{
		if (c != '\n' && word_wrap && state == BeginWord)
		{
			wlen = next_word_len() + 1;
			if (col + wlen >= right_margin)
				output_l4('\n');
			state = InWord;
		}

		if (col >= right_margin)
			output_l4('\n');
	}

	switch (c)
	{
		case '\t':
			if (col + tab_width - (col % tab_width) >= right_margin)
				output_l4('\n');
			else
				output_l4('\t');
			break;
		default:
			output_l4(c);
	}

	if (word_wrap && is_word_delim(c))
		state = BeginWord;
}

int cstr::next_char()
{
	if (slen > 0)
	{
		--slen;
		return (unsigned char) *sptr++;
	}
	return -1;
}

int cstr::is_word_delim(char c)
{
	if (strchr(word_delim, c) != NULL)
		return 1;
	return 0;
}

int cstr::next_word_len()
{
	const char *sptr1 = sptr;
	long slen1 = slen;
	while (slen1 > 0 && !is_word_delim(*sptr1))
		slen1--, sptr1++;
	return (int) (slen - slen1);
}


void cstr::flush_buffer()
{
	int c;

//	slen = pcount(); 
	slen = tellp();
	// note: it is absolutely necessary to copy the result of str() to your own string object
	// since the destructor of the temporary string object will be called immediatedly afterwards
	// Then still the c_str() pointer might be valid but not necessarily ;-(
	// So replacing the following to 2 lines by the single line
	// sptr = str();
	// does actually *not* work with Visual C++, but seems to work e. g. under Cygwin with gcc 3.4.4
	// (but even with gcc I think it is a coding bug)
	// 
	string tmp_string = str(); 
	sptr = tmp_string.c_str();

	state = BeginWord;
	for (;;)
	{
		if ((c = next_char()) < 0) return;
		output_l5((char) c);
	}
}
