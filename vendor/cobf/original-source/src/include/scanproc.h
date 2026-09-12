//
// COBF procedures for scanning sourcefiles
// Author: BB
// History:
// 1995-01-26	Created
// 2005-12-20	Update to ANSI-C++


#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>

#include "lib.h"
#include "b_tcont.h"
#include "b_str.h"

using namespace std;

bool debug_mode = false;
bool debugid_mode = false;
int verbose_level = QUIET;
bool stringshroud_flag = true;
bool filter_mode = false;	/* filter_mode = true: no cobfing */
bool concat_sourcefiles_flag = false;
bool filename_preserving_flag = false;
bool do_not_use_a_z_for_shrouding = false;

bool exclude_standardincludes_from_pp = true;
bool multiple_include_flag = false;

StringDictionary token_dict;		/* contains identifiers which will
					   only shrouded by preprocessor */


List<StringObject> arg_sourcefile_list;/* contains all sourcefiles in the
					   order specified in the command line */

List<StringObject> arg_sheader_list;	/* contains all separate headerfiles as
					   specified in the command line
					   with the -hs option */
StringDictionary arg_iheader_dict;	/* contains all internal headerfiles as
					   specified in the command line
					   with the -hi option */

List<SourceFile> shroud_file_list;	/* list of all files to be shrouded */
Set<SourceFile> shroud_file_set;	/* like shroud_file_list, but without path */



StringDictionary system_macro_dict;	/* contains all identifiers which must
					   not be shrouded. This set is initialized
					   via the -m option in the command line and
					   eventually grows when scanning the sourcefiles */

StringDictionary id_dict;		/* contains all identifiers found at
					   scanning the sourcefiles which
					   must be shrouded */

StringDictionary macros_with_double_hashes_dict;
StringDictionary macros_with_single_hashes_dict;
					/* contains macros which args
					   shouldn't be shrouded because the
					   macros contain the '#' and/or the
					   '##' operator and/or macros which
					   are members of the above sets.
					   These sets are emitted in the
					   cobf.log file and should be checked by user.
					 */
StringDictionary args_of_macros_with_double_hashes_dict;
StringDictionary args_of_macros_with_single_hashes_dict;
StringDictionary concat_token_macro_dict;

/* The follwing sets are only for statistical purposes! */
StringDictionary external_include_dict; /* contains all files which appear in
					   #include-stmts and are not specified
					   as source or header files. That's ok,
					   if this file is a standard header
					   like stdio.h */

StringDictionary possibly_wrong_external_include_dict;
					/* Normally standard headers are
					included via #include <...>. If you
					are using #include "..." and the file
					is not specified as a source or header file
					then COBF will complain. */

StringDictionary special_identifiers_dict; /* e. g. beginning with '__' */
StringDictionary no_processing_macro_dict;


StringMapList id_mapping_list, id_inverse_mapping_list;


const char *act_filename;		/* for yyerror error messages */

ofstream os, sh, ush;			/* global COBF output streams */

enum { DELIM, WORD };
int last_char_type;
char last_char;
int right_margin = 70;
int output_len;
enum { PROCESSING_INCLUDES, PPP, SCANNING, COBFING, LAST_PASS };
int scan_mode;
bool wordwrap;		// tells write_token to emit newlines at right margin
bool skipping_hash;	// in state processing a preprocessor line


enum { string_bufflen = 4000 };

char string_buffer[string_bufflen];
char yyt_buffer[string_bufflen];
char last_yytbuffer[string_bufflen];
char lastlast_yytbuffer[string_bufflen];
int last_token;

int ifblocknesting = 0;
int cobf_system_macro_nesting = 0;


/* special char squences to encode hashes for preprocessor input */

#define TPP_DIRECTIVE_HASH "%%%%%"
#define TPP_CONCAT_HASH	   "%%%%"
#define TPP_QUOTE_HASH     "%%%"
#define TPP_MACRO_HASH     "%%"

/////////////////////////////////////////////////////////////////////

// Class "Sourcefile" represents all sourcefiles passed in the command line

bool SourceFile::printExtendedFlag = false;	// extended printing used in logfile

SourceFile::SourceFile(const char *filename, const char *fileobfname, include_type incltype):
StringObject(filename)
{
	obfname = saveString(fileobfname);	// obfuscated name of sourcefile
	includetype = incltype;
	includefiles.ownsElements(true);
}

SourceFile::~SourceFile()
{
	delete [] obfname;
}

void SourceFile::print(ostream &aOstream)
{
	if (printExtendedFlag)
		printExt(aOstream);
	else
		aOstream << getKey();
}

void SourceFile::printExt(ostream &aOstream)
{
	int i;
	cstr str(aOstream);
	bool oldPrintExtendedFlag = printExtendedFlag;

	printExtendedFlag = false;
	str << getKey();
	if (has_obfname())
		 str << "(" << get_obfname() << ")";
	printExtendedFlag = oldPrintExtendedFlag;

	switch (includetype)
	{
		case prep_include:
			str << "[P]";
			break;
		case sep_include:
			str << "[S]";
			break;
		case ext_include:
			str << "[X]";
			break;
		default:
			;
	}
	if (get_include_set().size() > 0)
	{
		str.tab(20);
		str << "includes:\t";
		str.set_leftmargin();
		for (i = 0; i < get_include_set().size(); ++i)
			str << get_include_set()[i] << "\t";
	}
}

void SourceFile::add_include_file(const char *filename, include_type incltype)
{
	if (!includefiles.isIn(filename))
		includefiles.add( * new SourceFile(filename, filename, incltype));
}

/////////////////////////////////////////////////////////////////////


StringMapObject::StringMapObject(const char *name, const char *aMapname):
StringObject(name)
{
	mapname = saveString(aMapname);
}

StringMapObject::~StringMapObject()
{
	delete [] mapname;
}

/////////////////////////////////////////////////////////////////////

bool StringMapList::registerStringMapping(const char *key, const char *mapname)
{
	if (getElem(key) != NULL)
		return true; // key exists already
	add ( * new StringMapObject(key, mapname) );
	return false;
}

const char* StringMapList::getStringMapping(const char *key)
{
	StringMapObject *pStringMapObject = get(key);
	if (pStringMapObject == NULL)
		return NULL;
	return pStringMapObject->getMapName();
}

/////////////////////////////////////////////////////////////////////


/* called by LEX when reached EOF of a input file */
#ifdef yywrap
#undef yywrap
#endif
extern "C" int yywrap(void)
{
	fclose(yyin);

	sync_streams();

	return 1;
}

/* called by LEX to show errors detected when scanning the input file */

void yyerror(char *fmt, ...)
{
	sync_streams();
	fprintf(stderr, "cobf: ");
	va_list va;
	va_start(va, fmt);
	(void) vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, " in '%s' line %d\n", act_filename, YYX_LINENO);
	sync_streams();
	exit (1);
}

static void tell_filepos()
{
	cout << get_filename(act_filename) << " " << YYX_LINENO << ": ";
}

/* called by LEX to remove a C/C++ string constant from input stream */
/* the static variable string_buffer contains this string constant */

void skip_string()
{
	int i = 0;
	char c = 0;

	string_buffer[i++] = '\"';
	for (;;)
	{
		c = yyinput();
		if (c == '\n')
			YYX_INCLINE();
		if (c == '\\')
		{
			string_buffer[i++] = c;
			c = yyinput();
			if (c == '\n')
				YYX_INCLINE();
			string_buffer[i++] = c;
			continue;
		}
		if (c == EOF)
			yyerror("string constant: EOF reached");
		if (i >= string_bufflen - 3)
			yyerror("string constant too long");
		string_buffer[i++] = c;
		if (c == '\"') break;
	}
	string_buffer[i] = '\0';
}


static void update_concat_token_macro_dict(const char *string)
{
	concat_token_macro_dict.registerString(string);
	if (verbose_level >= INFO)
	{
		tell_filepos();
		cout << "info: identifier '" << string << "' should not be shrouded (context with ##)\n";
	}
}

/* COBF passes must no call yylex() directly to get the
   next token from the input stream. They shall call get_token. */

static int get_token()
{
	int token = yylex();
	switch (token)
	{
		case STRING:
			strcpy(yyt_buffer, string_buffer);
			break;
		case QUOTE_HASH:
			/* the stringize operator */
			strcpy(yyt_buffer, "#");
			break;
		case CONCAT_HASH:
			strcpy(yyt_buffer, "##");
			if (scan_mode == PPP && last_token == IDENTIFIER)
			{
				/* all identifiers which are operands of the
				   token concatenation operator ## must not be shrouded */
				update_concat_token_macro_dict(last_yytbuffer);
			}
			break;
		case IDENTIFIER:
			strcpy(yyt_buffer, (char *)yytext);
			if (scan_mode == PPP && last_token == CONCAT_HASH)
				update_concat_token_macro_dict(yyt_buffer);
			if (scan_mode == PPP && strncmp(yyt_buffer, "__", 2) == 0)
				special_identifiers_dict.registerString(yyt_buffer);
			break;
		default:
			strcpy(yyt_buffer, (char *)yytext);
	}
	/* Caution! WHITESPACE and NEWLINE are differnt tokens! */
	if (token != WHITESPACE && token != NEWLINE)
	{
		last_token = token;
		strcpy(last_yytbuffer, yyt_buffer);
	}
//	cout << "{" << yyt_buffer << "}\n";
	return token;
}

/* called at beginning of scanning */

static bool setinput(const char *filename)
{
	FILE *fh;

	if ((fh = fopen(filename, "r")) == NULL)
	{
		cerr << "cobf: can't open '" << filename << "'\n";
		return true;
	}
	act_filename = filename;

//	cout << "StringDictionary macros_with_double_hashes_dict: " <<
//		macros_with_double_hashes_dict << "\n";
//	cout << "StringDictionary macros_with_single_hashes_dict: " <<
//		macros_with_single_hashes_dict << "\n";

	yyrestart(fh); 

	/* this is done to accept programs beginning with a preprocessor stmt */
	/* (see preprocessor stmt rule in cobf.l! ) */
	/* PS. In GNU flex this action is redundant because the generated
	   scanner puts automaticly a newline char in the input buffer!
	*/
	ungetc('\n', yyin);
	YYX_LINENO = 0;

	if (verbose_level >= PROCESS)
		cout << "Reading '" << filename << "'\n";

	output_len = 0;
	last_char_type = DELIM;
	last_char = ' ';

	wordwrap = true;
	skipping_hash = false;

	last_token = -1;

	sync_streams();
	return false;
}

/* called by LEX to remove C-style comment from input stream */

void skip_comment1()
{
	int c=0;
	int c1=0;

//	c1 = '\0';
	do
	{
		while ((c = yyinput()) != '*')
		{
			if (c == EOF) break;
			if (c == '\n')
				YYX_INCLINE();
		}

		if ((c1 = yyinput()) == EOF) break;
		if (c1 != '/') unput(c1);
	}
	while (c1 != '/');

	if (c == EOF || c1 == EOF)
		yyerror("comment: EOF reached");
}

/* called by LEX to remove C++-style comment from input stream */

void skip_comment2()
{
	int c;
	while ((c = yyinput()) != '\n' && c != EOF)
		;

	/* this is done to accept preprocessor stmts after a C++-style comment */
	/* (see preprocessor stmt rule in cobf.l! ) */
	unput('\n');
}

/* called by LEX for single char tokens */
int token(const char *t)
{
	return (unsigned char) *t;
}

static void write_shroudedtoken(const char *token)
{
	char c;
	while ((c = *token++) != '\0')
	{
		os << c;
		++output_len;
		if (c == '\n')
			output_len = 0;
	}
}

void write_hexbyte(int val)
{
	os << "\\x" << hex << val << dec;
	output_len += 4;
}

static void write_shroudedstring(const char *token)
{
	os << *token++;
	++output_len;
	char c;

	while ((c = *token++) != '"' && c != '\0')
	{
		// do splitting strings only in the last pass to avoid
		// multiple splitting
		if (output_len + 5 > right_margin && scan_mode == COBFING)
//		if (0)
		{
			// Split lines with backslash if preprocessor stmt
			os << "\"";
			if (skipping_hash)
				os << " \\";
			os << "\n\"";
			output_len = 1;
		}

		if (c == '\\')
		{
			c = *token++;
			if (c == '\0') break;
			if (c >= '0' && c <= '7') // octal number ?
			{
				int val = c - '0';
				while ((c = *token++) >= '0' && c <= '7')
					val = val * 8 + c - '0';
				write_hexbyte(val);
				--token;
			}
			else if (c == 'x') // hexadecimal number ?
			{
				int val = 0, val1;
				for (;;)
				{
					c = *token++;
					val1 = -1;
					if (c >= '0' && c <= '9') val1 = c - '0';
					if (c >= 'A' && c <= 'F') val1 = c - 'A' + 10;
					if (c >= 'a' && c <= 'f') val1 = c - 'a' + 10;
					if (val1 < 0) break;
					val = val * 16 + val1;
				}
				write_hexbyte(val);
				--token;

			}
			else
			{
				os << '\\' << c;
				output_len += 2;
			}
		}
		else
			write_hexbyte((unsigned char) c);

	}
	if (c != '\0')
	{
		os << c;
		++output_len;
	}
}

/* write a token to global output stream "os" */
/* performs wordwrap at right margin and whitespace compression */

void write_token(const char *token)
{
	char c = *token;
	int char_type = DELIM;

	int len = strlen(token);
	if (scan_mode == SCANNING || len == 0) return;

	if (isalpha(c) || isdigit(c) || c == '_')
		char_type = WORD;

	bool write_shroudedstring_flag = c == '"' && stringshroud_flag && !skipping_hash;

	if (output_len + (int) strlen(token) + 1 > right_margin
		&& !write_shroudedstring_flag
		&& wordwrap
		&& c != '\n'
		&& last_char != ')' /* avoid Borland Cpp bug with macros with no arguments*/)
	{
		// Split lines with backslash if preprocessor stmt
		if (skipping_hash)
			os << " \\";
		os << '\n';
		output_len = 0;
		last_char = '\n';
		last_char_type = DELIM;
	}

#if 0
	// BB 990921
	while (((output_len / 15) % 2) == 1)
		os << ' ', ++output_len,last_char = ' ';
#endif

	// Rules for emitting a space:
	// 1. First char of actual token is identical with last char of
	//    last token (example: '> >' must no be written as '>>' !)
	//    ['> >' is a legal char sequence for example in a C++ template definition!!]
	//    [But there's no prob to write '( (' as '((' !]
	// 2. Type of last and actual token is a identifier (WORD)
	// 3. Actual token or last token is '*' and the type of the other token
	//    is not identifier
	//    [ a / * b must no be written as a /* b !!]

	if (/* 1st Rule: */ (last_char_type == WORD && char_type == WORD) ||
	    /* 2nd Rule: */ (last_char == c && strchr("()[]{}*,;", c) == NULL) ||
	    /* 3nd Rule: */ ((last_char == '*' && char_type != WORD) ||
			     (c == '*' && last_char_type != WORD))
	   )
	{
		os << ' ', ++output_len;
	}

	if (write_shroudedstring_flag)
		write_shroudedstring(token);
	else
		write_shroudedtoken(token);

	last_char = token[len - 1];
	last_char_type = char_type;
}

// remove a sequence like <stdio.h> from input stream and store the
// char sequence in "string_buffer"

void skip_stdinclude()
{
	int i = 0;
	char c;
	string_buffer[i++] = '<';
	for (;;)
	{
		c = yyinput();
		if (c == '\n')
			YYX_INCLINE();
		if (c == EOF)
			yyerror("#include: EOF reached");
		if (i < string_bufflen - 3)
			string_buffer[i++] = c;
		if (c == '>') break;
	}
	string_buffer[i] = '\0';
}


static bool include_error;
static int include_pass;
static int include_count_in_this_pass;
StringDictionary include_msg_dict; /* contains names of already reported include files */
static List<StringObject> include_stack;

#define BEGIN_INCLUDE	"_beg_inc___"
#define END_INCLUDE	"_end_inc___"

// procedure to expand include stmts
// (when includefile specified with -h-option):
//
// 1. Mark actual position in sourcefile with macro BEGIN_INCLUDE "includefile"
// 2. append include file at actual file position
//    (insert newlines to avoid problems with includefiles which
//     dont't end with a newline)
// 3. Mark actual file position with macro END_INCLUDE
// 4. go to step 1 if one or more file were included in the actual step
// 5. Rule to prevent infinite recursion at step 2:
//    - when scanning the macro BEGIN_INCLUDE "includefile" then
//	push "includefile" on the include stack
//    - pop last entry from include stack when scanning mcro END_INCLUDE
//    - append only includefiles which are no on the include stack

static bool is_in_include_stack(const char *filename)
{
	int i;
	for (i = 0; i < include_stack.size(); ++i)
		if (include_stack[i].hasKey(filename))
			return true;
	return false;
}

/* copy file "include_filename" to global stream "os" */

static bool make_include(const char *include_filename, char include_type)
{
	ostringstream include_pathname;
	get_searchpathname(include_filename, include_type == '\"', include_pathname);
	if (*include_pathname.str().c_str() == '\0')
	{
		cout << "#include " << include_filename << " not found!\n"
			"(use -i option to add new search path for source and header files!)\n";
		include_error = true;
		return true;
	}
	write_token("\n/*#include ");
	write_token(include_pathname.str().c_str());
	write_token(" */");

	if (is_in_include_stack(include_filename))
	{
		if (verbose_level >= DEBUG)
		{
			tell_filepos();
			cout << "debug: include file '" << include_filename << "' already included!\n";
		}
		return false;
	}

	if (verbose_level >= DEBUG)
	{
		tell_filepos();
		cout << "debug: appending include file '" << include_pathname.str().c_str() << "'\n";
	}
	if (debug_mode)
		os << "\n#pragma line 1 \"" << include_filename << "\"\n";
	os << "\n#" BEGIN_INCLUDE "\"" << include_filename << "\"\n";
	os << "\n";
	bool ret = append_file(include_pathname.str().c_str(), os);
	os << "\n";
	os << "\n#" END_INCLUDE "\n";
	if (debug_mode)
		os << "\n#pragma line " << YYX_LINENO << " \"" <<
					get_filename(act_filename) << "\"\n";
	if (ret)
	{
		include_error = true;
		return true;
	}
	else
		++include_count_in_this_pass;

	return ret;
}

// update entry in "sourcefile_dict": "sourcefile" includes "includefile"

static void log_include_file(const char *sourcefile, const char *includefile, include_type incltype)
{
//	cout << "sourcefile " << sourcefile << "\n";
//	cout << "includefile " << includefile << "\n";
	shroud_file_set.getSafe(get_filename(sourcefile)).
		add_include_file(includefile, incltype);
	if (arg_sourcefile_list.isIn(get_filename(includefile)))
		possibly_wrong_external_include_dict.registerString(includefile);
}


StringDictionary already_used_external_includes_dict;
					/* List of include files alreaedy used
					   in actual file (scanmode == COBFING) */

// remove #include stmt from input stream
// return value: true, if next line preprocessor stmt (beginning with '#')

bool skip_include()
{
	int token=0;

	static char include_filename[string_bufflen];

	if ((token = get_token()) <= 0) return false;
	while (token == WHITESPACE)
		if ((token = get_token()) <= 0) return false;
	switch (token)
	{
		case STRING:
			break;
		case OP:
			if (strcmp(yyt_buffer, "<") == 0)
			{
				skip_stdinclude();
				break;
			}
			yyerror("#include: unsupported syntax");
			break;
		case IDENTIFIER:
			strcpy(string_buffer, yyt_buffer);
			tell_filepos();
			cout << "Warning: can't resolve  #include " << yyt_buffer<< "\n"
			"It will be handled as an external include!\n";
			break;

		default:
			yyerror("#include: unsupported syntax");
			break;
	}

	if (string_buffer[0] == '"' || string_buffer[0] =='<')
	{
		strcpy(include_filename, &string_buffer[1]);
		include_filename[strlen(include_filename)-1] = '\0';
	}
	else
		strcpy(include_filename, string_buffer);

	if (include_filename[0] && scan_mode == PROCESSING_INCLUDES)
	{
		if (arg_iheader_dict.isIn(include_filename)) // "internal" headerfile
		{
			log_include_file(act_filename, include_filename, prep_include);
			make_include(include_filename, string_buffer[0]); // do the include
									  // by pasting the file
		}
		else
		{
			write_token("\n" TPP_DIRECTIVE_HASH "include");
			write_token(string_buffer);
			if (arg_sheader_list.isIn(get_filename(include_filename)))
				log_include_file(act_filename, include_filename, sep_include);
			else
			{
				log_include_file(act_filename, include_filename, ext_include);
				if (!include_msg_dict.isIn(include_filename))
				{
				include_msg_dict.registerString(include_filename);
				if (verbose_level >= INFO)
				{
					tell_filepos();
					cout <<
					"info: #include " << string_buffer <<
					" won't be shrouded\n"
					"(dont't forget to declare global symbols of "
					"this include via -t option!)\n";
				}
				if (string_buffer[0] == '\"')
				{
					cout <<
					"Are you really sure that '" << include_filename <<
					"' isn't a part of your sources??\n"
					"(if it is, specify this header with the -hs or -hi option!)\n";
					possibly_wrong_external_include_dict.registerString(include_filename);
				}
				}
			}
		}
	}
	if (scan_mode == PPP)
	{
		write_token("\n" TPP_DIRECTIVE_HASH "include");
		write_token(string_buffer);
	}
	else if (scan_mode == COBFING)
	{
		if (!already_used_external_includes_dict.isIn(include_filename) || multiple_include_flag)
		{
		already_used_external_includes_dict.registerString(include_filename);
		if (arg_sheader_list.isIn(get_filename(include_filename)))
		{
			ostringstream include_string;
			include_string <<
				string_buffer[0] <<
				shroud_file_set.getSafe(get_filename(include_filename)).get_obfname() <<
				string_buffer[strlen(string_buffer) - 1] <<
				"\n";
			write_token("\n#include");
			write_token(include_string.str().c_str());
		}
		else
		{
			if (token_dict.size() > 0)
				write_token("\n#include\"" UNCOBF_HEADER "\"");
			write_token("\n#include");
			write_token(string_buffer);
			if (token_dict.size() > 0)
				write_token("\n#include\"" COBF_HEADER "\"");

			external_include_dict.registerString(include_filename);
		}
		}
	}

	while ((token = get_token()) > 0)
	{
		if (token == NEWLINE || token == HASH) break;
	}
	if (token == HASH) return true;
	return false;
}

//
// called by lex: yytext contains a identifier which was made invisible
// for preprocessor. Now return the original identifier in yytext
//

void skip_tpp_id()
{
	int i = 0;
	int j = 3;	// Prefix is %%(
	int c;
	for (;;)
	{
		while (yytext[j] == ' ')
			++j;
		if (j >= (int) strlen((char *)yytext) || yytext[j] == ')') break;
		sscanf((char *)&yytext[j], "%d", &c);
		while (yytext[j] != ' ')
			++j;
		yytext[i++] = (char) c;
	}
	yytext[i] = '\0';
}

// write a token of type IDENTIFIER
bool exclude_from_ppflag = false; 	// = true: hiding the identifier against preprocesing


void get_shrouded_id(StringWithCount &aString, char *string_buffer)
{
	const char *pMapString = id_mapping_list.getStringMapping(aString);
	if (pMapString != NULL)
	{
		// there exists a predefined mapping ...
		strcpy(string_buffer, pMapString);
		return;
	}

	long count = aString.getCount();

	if (!do_not_use_a_z_for_shrouding)
	{
		// the 26 most frequent identifiers are renamed to a .. z
		// getCount() is the position in the table ordered by frequency
		if (aString.getCount() <= 'z' - 'a')
			sprintf(string_buffer, "%s%c", id_prefix, (char) (count + 'a'));
		else
			// now rename the identifiers to l1 ... l99999999
			sprintf(string_buffer, "%s%ld", id_prefix, aString.getCount() - ('z' - 'a') - 1);
	}
	else
			sprintf(string_buffer, "%s%ld", id_prefix, aString.getCount());

	if (debugid_mode)
	{
		strcat(string_buffer, "_");
		strcat(string_buffer, aString.getKey());
	}

	const int max_id_clash = 5;
	int i;
	for (i = 0; i < max_id_clash; ++i)
	{
		const char *pMapString = id_inverse_mapping_list.getStringMapping(string_buffer);
		if (pMapString == NULL) return; // there is no clash between this shrouded identifier and one of the identifiers
									    // provided by the predefined mapping list
		strcat(string_buffer, "_");
	}
	cout << "Error: identifier '" << string_buffer << "' is conflicting with identifier mapping list\n";
	exit (1);
}

static void write_id(const char *id_string)
{
	int i;
	if (filter_mode || scan_mode == PROCESSING_INCLUDES ||
		(scan_mode == PPP && !exclude_from_ppflag) ||
		(scan_mode != PPP && system_macro_dict.isIn(id_string)))
	{
		write_token(id_string);
		return;
	}

	if (debug_mode)
	{
		write_token(" /*");
		write_token(id_string);
		write_token("*/ ");
	}
	if (scan_mode == PPP && exclude_from_ppflag)
	{
		// prevent identifier from being preprocessed
		// inverse procedure is done in function skip_tpp_id()
		ostringstream buffer;
		// emit space to separate from prior token!
		buffer << " " TPP_MACRO_HASH "(";
		for (i = 0; i < (int) strlen(id_string); ++i)
		{
			// write ASCII value of chars instead of chars itself
			buffer << (int) (unsigned char) id_string[i];
			if (i < (int) strlen(id_string) - 1)
				buffer << " ";
		}
		buffer << ")";
		write_token(buffer.str().c_str());
		return;
	}

	// now start obfuscating ...

	StringWithCount *pString;
	if ((pString = id_dict.get(id_string)) == NULL)
		yyerror("identifier '%s' not found", id_string);

	get_shrouded_id(*pString, string_buffer);

	if (token_dict.get(string_buffer) != NULL)
		yyerror("identifier clash: (shrouded) identifier '%s' also defined as system token!\n", string_buffer);
	
	write_token(string_buffer);
	if (system_macro_dict.isIn(string_buffer))
		cout << "warning: macro identifier '" << string_buffer << "' also used as identifier by cobf!\n";

	// if identifier is a token write a #undef stmt in header uncobf.h
	// to re-obfuscate the identifier (uncobf.h is
	// included before including a external header)
	if ((pString = token_dict.get(id_string)) != NULL)
	{
		if (pString->getCount() == 0)
		{
			sh << "#define " << string_buffer << " " << id_string << "\n";
			pString->incCount();
			ush << "#undef " << string_buffer << "\n";
		}
	}
}

// reads one preprocessor line
// Returns the read tokenlist with
// numtokenlist[i] = type of token
// stringtokenlist[i] = value of token as string
// expp_flag = exclude pp line from shrouding
// token = type of last token

static bool cobf_system_macro_flag = false;

static void read_pp_line(List<NumObject> &numtokenlist,
List<StringObject> &stringtokenlist, bool &expp_flag, int &token)
{
	int i;
	StringDictionary tokendict; // stringtokenlist as dictionary

	numtokenlist.ownsElements(true);
	stringtokenlist.ownsElements(true);

	numtokenlist.add( * new NumObject(IDENTIFIER) );
	stringtokenlist.add( * new StringObject(yyt_buffer) );
	while ((token = get_token()) > 0)
	{
		if (token == NEWLINE || token == HASH) break; // HASH =^ next pp stmt

		numtokenlist.add( * new NumObject(token) );
		stringtokenlist.add( * new StringObject(yyt_buffer) );
		tokendict.registerString(yyt_buffer);
	}
	expp_flag = false;

	if (scan_mode != PPP)
		return;

//	for (int k = 0; k < stringtokenlist.size(); ++k)
//		cout << k << ":'" << stringtokenlist[k] << "'\n";


	cobf_system_macro_flag = false;
	if (tokendict.isIn(COBF_SYSTEM_MACRO))
		cobf_system_macro_flag = true;


	// check whether to exclude act #-line from preprocessing when
	// this line system macro or macro in 'no_processing_macro_dict'

	for (i = 0; i < tokendict.size(); ++i)
	{
		const char *actToken = tokendict[i];
		if (strcmp(actToken, "defined") == 0) continue;
		if (system_macro_dict.isIn(actToken) || no_processing_macro_dict.isIn(actToken))
		{
			if (verbose_level >= INFO)
			{
				tell_filepos();
				cout << "info: #";
				for (int j= 0; j < 3; ++j)
				{
					if (j >= stringtokenlist.size()) break;
					cout << stringtokenlist[j];
				}
				cout << " not preprocessed because of '" << tokendict[i] <<"'\n";
			}
			if (stringtokenlist.size() > 2 && strcmp(stringtokenlist[0], "define") == 0)
			{
				no_processing_macro_dict.registerString(stringtokenlist[2]);
				if (verbose_level >= INFO)
				{
					tell_filepos();
					cout << "info: identifier '" << stringtokenlist[2] << "' added to the list of non-processed macros\n";
				}

			}
			expp_flag = true;
			break;
		}
	}

	if (strcmp(stringtokenlist[0], "define") == 0)
	{
		if (tokendict.isIn("#"))
			macros_with_single_hashes_dict.registerString(stringtokenlist[2]);
		if (tokendict.isIn("##"))
			macros_with_double_hashes_dict.registerString(stringtokenlist[2]);
		for (i = 3; i < stringtokenlist.size(); ++i)
		{
			if (macros_with_single_hashes_dict.isIn(stringtokenlist[i]))
				macros_with_single_hashes_dict.registerString(stringtokenlist[i]);
			if (macros_with_double_hashes_dict.isIn(stringtokenlist[i]))
				macros_with_double_hashes_dict.registerString(stringtokenlist[i]);
		}
	}
}

static void update_cobf_dict(const char *string)
{
	if (system_macro_dict.isIn(string))
		return;
	id_dict.registerString(string);
}

static void process_pp_line(List<NumObject> &tokennumlist,
	List<StringObject> &tokenstringlist, bool expp_flag)
{
	int i;
//	cout << "processing " << tokenstringlist << "\n";
	exclude_from_ppflag = false;
	if (scan_mode == PPP && (ifblocknesting > 0 || expp_flag))
	{
		write_token("\n" TPP_DIRECTIVE_HASH);
		exclude_from_ppflag = true;
	}
	else
		write_token("\n#");
	for (i = 0; i < tokennumlist.size(); ++i)
	{
		bool output = false;
		switch ((int) tokennumlist[i].getNum())
		{
		case IDENTIFIER:
			if (i > 0) // ignore 1st entry (#ifdef #define etc.)
			{
			if ((exclude_from_ppflag || scan_mode == COBFING) && i > 0)
			{
				write_id(tokenstringlist[i]);
				output = true;
			}
			else if (scan_mode == SCANNING && i > 0)
				update_cobf_dict(tokenstringlist[i]);
			}
			break;
		case QUOTE_HASH:
			if (exclude_from_ppflag)
			{
				write_token(TPP_QUOTE_HASH);
				output = true;
			}
			break;
		case CONCAT_HASH:
			if (exclude_from_ppflag)
			{
				write_token(TPP_CONCAT_HASH);
				output = true;
			}
			break;
		case WHITESPACE:
			if (i > 0 && tokennumlist[i-1].getNum() != WHITESPACE)
				write_token(" ");
			output = true;
			break;
		}
		if (!output)
			write_token(tokenstringlist[i]);
	}
	if (debug_mode)
	{
		ostringstream ifnestingstring;
		ifnestingstring << " /* [" << ifblocknesting << "] */";
		write_token(ifnestingstring.str().c_str());
	}
	exclude_from_ppflag = false;

}

static const char *get_define_identifier(List<NumObject> &tokennumlist,
					List<StringObject> &tokenstringlist)
{
	int i;
	for (i = 1; i < tokenstringlist.size(); ++i)
		if (tokennumlist[i].getNum() == IDENTIFIER)
			return tokenstringlist[i];
	return "";
}

static bool skip_define()
{
	int token=0;
	bool expp_flag=false;

	List<NumObject> numtokenlist;
	List<StringObject> stringtokenlist;

	read_pp_line(numtokenlist, stringtokenlist, expp_flag, token);

	// if a token is to be redefined then it shall not be shrouded
	const char *def_id = get_define_identifier(numtokenlist, stringtokenlist);
//	tell_filepos();
//	cout << "#define " << def_id << "\n";
	if (scan_mode == PPP)
	{
		if (cobf_system_macro_nesting > 0)
		{
			system_macro_dict.registerString(def_id);
			if (verbose_level >= INFO)
			{
				tell_filepos();
				cout << "info: identifier '" << def_id << "' added to the system macro list (macro nesting depth: "<<cobf_system_macro_nesting<<")\n";
			}

		}
		else if (ifblocknesting > 0)
		{
			no_processing_macro_dict.registerString(def_id);
			if (verbose_level >= INFO)
			{
				tell_filepos();
				cout << "info: identifier '" << def_id << "' added to the list of non-processed macros (#if nesting depth: "<<ifblocknesting<<")\n";
			}
		}

	}
	process_pp_line(numtokenlist, stringtokenlist, expp_flag);

	if (token == HASH)
		return true;
	return false;
}

static bool skip_undef_else_endif()
{
	int token=0;
	bool expp_flag=false;

	List<NumObject> numtokenlist;
	List<StringObject> stringtokenlist;

	read_pp_line(numtokenlist, stringtokenlist, expp_flag, token);

	process_pp_line(numtokenlist, stringtokenlist, expp_flag);

	if (strcmp(stringtokenlist[0], "endif") == 0)
	{
		if (ifblocknesting > 0)
			--ifblocknesting;
		if (cobf_system_macro_nesting > 0)
			--cobf_system_macro_nesting;
	}



	if (token == HASH)
		return true;
	return false;

}

static bool skip_if()
{
	int token=0;
	bool expp_flag=false;

	List<NumObject> numtokenlist;
	List<StringObject> stringtokenlist;

	read_pp_line(numtokenlist, stringtokenlist, expp_flag, token);

	if (strncmp(stringtokenlist[0], "if", 2) == 0 &&	// if or elif?
		(expp_flag || ifblocknesting > 0))
	{
		++ifblocknesting;
		if (cobf_system_macro_flag)
			++cobf_system_macro_nesting;
	}
	process_pp_line(numtokenlist, stringtokenlist, expp_flag);

	if (token == HASH)
		return true;
	return false;
}

static bool skip_startendinclude()
{
	int token=0;
	bool expp_flag=false;

	List<NumObject> numtokenlist;
	List<StringObject> stringtokenlist;

	read_pp_line(numtokenlist, stringtokenlist, expp_flag, token);

	if (strcmp(stringtokenlist[0], BEGIN_INCLUDE) == 0)
	{
		strcpy(string_buffer, &string_buffer[1]);
		string_buffer[strlen(string_buffer)-1] = '\0';
		include_stack.add( * new StringObject(string_buffer) );
	}
	else
		include_stack.removeLast();
	if (scan_mode == PROCESSING_INCLUDES)
		process_pp_line(numtokenlist, stringtokenlist, expp_flag);

//	cout << "Include_stack:" << include_stack << "\n";

	if (token == HASH)
		return true;
	return false;
}

void skip_hash()
{
	int token=0;
	for (;;)
	{
		// When preprocessing word wrap isn't allowed
		wordwrap = false;
		if (scan_mode == COBFING)
			wordwrap = true;
		skipping_hash = true;
		if ((token = get_token()) <= 0) break;
		if (token == IDENTIFIER)
		{
			if (strcmp(yyt_buffer, "include") == 0)
			{
				if (skip_include()) continue;
				break;
			}
			else if (strcmp(yyt_buffer, "define") == 0)
			{
				if (skip_define()) continue;
				break;
			}
			else if (strncmp(yyt_buffer, "if", 2) == 0)
			{			// #if, #ifdef, #ifndef ...
				if (skip_if()) continue;
				break;
			}
			else if (strcmp(yyt_buffer, "elif") == 0)
			{
				if (skip_if()) continue;
				break;
			}
			else if (strcmp(yyt_buffer, "else") == 0)
			{
				if (skip_undef_else_endif()) continue;
				break;
			}
			else if (strcmp(yyt_buffer, "endif") == 0)
			{
				if (skip_undef_else_endif()) continue;
				break;
			}
			else if (strcmp(yyt_buffer, "undef") == 0)
			{
				if (skip_undef_else_endif()) continue;
				break;
			}
			else if (strcmp(yyt_buffer, "pragma") == 0)
				wordwrap = false;
			else if (strcmp(yyt_buffer, BEGIN_INCLUDE) == 0)
			{
				if (skip_startendinclude()) continue;
				break;
			}
			else if (strcmp(yyt_buffer, END_INCLUDE) == 0)
			{
				if (skip_startendinclude()) continue;
				break;
			}

		}
		// remaining preprocessor directives:
		// #error #line #pragma

		if (scan_mode == PPP)
			write_token("\n" TPP_DIRECTIVE_HASH);
		else
			write_token("\n#");

		write_token(yyt_buffer);

		while ((token = get_token()) > 0)
		{
			if (token == NEWLINE || token == HASH) break;
			write_token(yyt_buffer);
		}
		if (token == HASH) continue;
		if (token == NEWLINE) break;
	}
	write_token("\n");
	wordwrap = true;
	skipping_hash = false;
}

bool open_ofstream(ofstream &str, const char *filename, bool binary)
{
	if (verbose_level >= 9)
		cout << "open_ofstream(\"" << filename << "\")\n";
	if (binary)
		str.open(filename, ios::out | ios::binary);
	else
		str.open(filename, ios::out);
	if (!str)
	{
		cerr << "cobf: can't write '" << filename << "'\n";
		return true;
	}
	if (verbose_level >= INFO)
		cout << "writing '" << filename << "'\n";
	return false;
}

////////////////////////////////////////////////////////////////////////
//
// Pass 1 processing includes
//
////////////////////////////////////////////////////////////////////////

static void processinclude()
{
	int token=0;
	scan_mode = PROCESSING_INCLUDES;

	while ((token = get_token()) > 0)
	{
		switch (token)
		{
			case IDENTIFIER:
				write_id(yyt_buffer);
				break;
			case HASH:
				skip_hash();
				break;
			case WHITESPACE:
				break;
			case NEWLINE:
				if (!debug_mode)
					break;
			default:
				write_token(yyt_buffer);
				break;
		}
	}
}

static bool process_include_pass(const char *input_filename, const char *output_filename, bool &cont_flag)
{
	cont_flag = false;
	
    if (verbose_level >= PROCESS)
        cout << include_pass << " ";
	
    if (setinput(input_filename))
		return true;

	if (open_ofstream(os, output_filename))
		return true;

	include_error = false;
	include_count_in_this_pass = 0;

	include_stack.ownsElements(true);
	include_stack.reset();

	if (verbose_level >= DEBUG)
		cout << "-- include pass " << include_pass << " of '" << get_filename(input_filename) << "'\n";
	os << "/* PASS 1 -- processing includes of '" << get_filename(input_filename) << "' */\n";
	processinclude();
	os << "\n";
	os.close();

	// shouldn't be necessary !?
#ifndef unix
    _fcloseall();
#endif

	if (include_error) return true;
	if (include_count_in_this_pass > 0)
		cont_flag = true;
	return false;
}

bool processinclude_file(const char *input_filename, const char *output_filename)
{
	include_msg_dict.reset();
	include_pass = 0;
	bool cont_flag = true;
	bool ret=false;
	while (cont_flag)
	{
		++include_pass;
		ret = process_include_pass(input_filename, output_filename, cont_flag);
		if (ret) break;
		remove(input_filename);
		rename(output_filename, input_filename);
	}
	if (!ret) rename(input_filename, output_filename);
	include_msg_dict.reset();
	return ret;
}

////////////////////////////////////////////////////////////////////////
//
// Pass 2 prepare sourcefiles for preprocessing
//
////////////////////////////////////////////////////////////////////////

static void ppp()
{
	int token=0;
	scan_mode = PPP;

	bool arg_mode = false;	// checking arguments mode
	int arg_nesting = 0;    // nesting of brackets in argument line
	char *act_arg = NULL;
	strcpy(lastlast_yytbuffer, "");
	while ((token = get_token()) > 0)
	{
		switch (token)
		{
			case IDENTIFIER:
				write_id(yyt_buffer);
				if (arg_mode)
				{
					if (macros_with_single_hashes_dict.isIn(act_arg))
						args_of_macros_with_single_hashes_dict.registerString(yyt_buffer);

					if (macros_with_double_hashes_dict.isIn(act_arg))
						args_of_macros_with_double_hashes_dict.registerString(yyt_buffer);
				}

				break;
			case HASH:
				skip_hash();
				break;
			case WHITESPACE:
				break;
			case NEWLINE:
				if (!debug_mode)
					break;
			default:
				if (strcmp(yyt_buffer, "(") == 0)
				{
//					cout << "Macro '" << lastlast_yytbuffer << "'\n";
					if (!arg_mode &&
						macros_with_single_hashes_dict.isIn(lastlast_yytbuffer)||
						macros_with_double_hashes_dict.isIn(lastlast_yytbuffer))
					{
						arg_mode = true;
						arg_nesting = 0;
						act_arg = saveString(lastlast_yytbuffer);
						if (verbose_level > DEBUG)
							cout << "Processing Macro '" << act_arg << "'\n";
					}
					if (arg_mode)
						++arg_nesting;
				}
				if (arg_mode && strcmp(yyt_buffer, ")") == 0)
				{
					--arg_nesting;
					if (arg_nesting == 0)
					{
						arg_mode = false;
						delete [] act_arg;
					}
				}

				write_token(yyt_buffer);
				break;
		}
		if (token != WHITESPACE && token != NEWLINE)
			strcpy(lastlast_yytbuffer, yyt_buffer);
	}
}

bool ppp_file(const char *input_filename, const char *output_filename, bool sheaderflag)
{
	if (setinput(input_filename))
		return 1;

	if (open_ofstream(os, output_filename))
		return true;
	os << "/* PASS 2 -- prepare '" << get_filename(input_filename) << "' for preprocessing */\n";
	ifblocknesting = 0;
	cobf_system_macro_nesting = 0;
	if (sheaderflag)
		++ifblocknesting;
	ppp();
	os << "\n";
	os.close();
	if (verbose_level >= DEBUG)
	{
		cstr ostr(cout);
		ostr << "\n*** The following macros won't be additionally processed by the pre-processor:\n" << no_processing_macro_dict << "\n***\n\n";

	}
	return 0;
}


////////////////////////////////////////////////////////////////////////
//
// Pass 4 Build identifier dictionary
//
////////////////////////////////////////////////////////////////////////

static void scan()
{
	int token=0;
	scan_mode = SCANNING;

	while ((token = get_token()) > 0)
	{
		switch (token)
		{
			case HASH:
				skip_hash();
				break;
			case IDENTIFIER:
				update_cobf_dict(yyt_buffer);
				break;
			default:
				break;
		}
	}
}

static int compareStringWithCount(BObject *pBObject1, BObject *pBObject2)
{
	return ((StringWithCount*) pBObject1)->getCount() <
		      ((StringWithCount*) pBObject2)->getCount();
}



bool scan_file(const char *filename)
{
	if (setinput(filename))
		return 1;

	scan();

	return 0;
}

void scan_finished()
{
	int i;
	if (id_dict.size() == 0)
	{
		cout << "Error - No identifiers found!\nEither the source files are empty or the script\nfor calling the preprocesor doesn't work!\n";
		exit (1);
	}
	List<StringWithCount> id_sortlist;
	id_sortlist += id_dict;
	id_sortlist.sort(compareStringWithCount);


	if (verbose_level >= DEBUG)
	{
		cstr ostr(cout);
		ostr << "id_sortlist: " << id_sortlist << "\n";
#if 1
		for (i = 0; i < id_sortlist.size(); ++i)
			cout << i << " : " << id_sortlist[i] << " : " << id_sortlist[i].getCount() << "\n";
#endif
	}
	
	
	for (i = 0; i < id_sortlist.size(); ++i)
	{
//		cout << id_sortlist[i] << "(" << id_sortlist[i].getCount() << ")\n";
		// here we change the counter of the token in the
		// "id_dict" container! ("id_dict" und "id_sortlist" share the
		// same objects!!)
		id_sortlist[i].resetCount();
		id_sortlist[i].addCount(i);
	}

	if (write_dictionary)
	{
		id_sortlist.sort();
		ofstream os;
		if (open_ofstream(os, write_dictionary_filename))
			return;
		for (i = 0; i < id_sortlist.size(); ++i)
			os << id_sortlist[i] << "\n";
		os.close();

	}

	if (write_mapping)
	{
		id_sortlist.sort();
		ofstream os;
		if (open_ofstream(os, write_mapping_filename))
			return;
		for (i = 0; i < id_sortlist.size(); ++i)
		{
			get_shrouded_id(id_sortlist[i], string_buffer);
			os << id_sortlist[i] << " " <<  string_buffer << "\n";
		}
		os.close();

	}
}

////////////////////////////////////////////////////////////////////////
//
// Pass 5 write obfuscated sourcefile
//
////////////////////////////////////////////////////////////////////////

#include <time.h>

static void write_fileheader(ostream &os, const char *filename = "")
{
	os << "/*\n   ";
	if (*filename)
		os << "'" << filename << "' ";
	time_t t;
	time(&t);
	os << "Obfuscated by COBF (Version "<< version << " by BB) at " << ctime(&t) << "*/\n";
}

bool open_cobfheader(const char *outputcobfheader, const char *outputuncobfheader)
{
	if (open_ofstream(sh, outputcobfheader))
		return true;
	write_fileheader(sh);
	if (open_ofstream(ush, outputuncobfheader))
		return true;
	write_fileheader(ush);
	return false;
}

bool close_cobfheader()
{
	sh.close();
	ush.close();

	return false;
}


bool cobf_file(const char *inputfilename, const char *outputfilename)
{

	if (setinput(inputfilename))
		return true;

	if (open_ofstream(os, outputfilename))
		return true;

	int token;
	scan_mode = COBFING;

	already_used_external_includes_dict.ownsElements(true);

	write_fileheader(os, filename_preserving_flag ?
					get_filename(inputfilename) : "");
	if (token_dict.size() > 0)
		write_token("#include\"" COBF_HEADER "\"\n");
	while ((token = get_token()) > 0)
	{
		switch (token)
		{
			case HASH:
				skip_hash();
				break;
			case IDENTIFIER:
				write_id(yyt_buffer);
				break;
			case WHITESPACE:
				break;
			case NEWLINE:
				if (!debug_mode)
					break;
			default:
				write_token(yyt_buffer);
				break;
		}
	}
	os << "\n";
	os.close();

	already_used_external_includes_dict.reset();

	return false;
}


////////////////////////////////////////////////////////////////////////
//
// Pass 6 remove empty lines and redundant include stmts
//
////////////////////////////////////////////////////////////////////////

bool afterprocess_file(const char *inputfilename, const char *outputfilename)
{
//	cout << "afterprocess_file(\"" << inputfilename << "\", \"" << outputfilename << "\")\n";

	ifstream is (inputfilename, ios::in);
	if (!is)
	{
		cout << "can't write '" << inputfilename << "'\n";
		return true;
	}
	if (verbose_level >= PROCESS)
		cout << "Reading '" << inputfilename << "'\n";
	if (open_ofstream(os, outputfilename))
		return true;
	enum { max_line_len = 6000 };
	char *act_line = new char[max_line_len];
	char *last_line = new char[max_line_len];
	strcpy(act_line, "");
	strcpy(last_line, "");
	while (!is.eof())
	{
		is. getline(act_line, max_line_len - 1);
		for (;;)
		{
			int act_len = (int) strlen(act_line);
			if (act_len == 0) break;
			if (act_line[act_len-1] == ' ' || act_line[act_len-1] == '\t')
				act_line[act_len-1] = '\0';
			else break;
		}
		if (strlen(act_line) == 0) continue;
		if (strcmp(act_line, "#include\"" UNCOBF_HEADER "\"") == 0 &&
		    strcmp(last_line, "#include\"" COBF_HEADER "\"") == 0)
		{
			strcpy(act_line, "");
			strcpy(last_line, "");
		}
		if (strlen(last_line) > 0)
			os << last_line << "\n";
		strcpy(last_line, act_line);
	}
	os << last_line << "\n";
	os.close();
	is.close();
	delete [] act_line;
	delete [] last_line;
	return false;
}

