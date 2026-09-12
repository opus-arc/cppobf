//
// COBF global interface file
// Author: BB
// History:
// 1995-01-26	Created
// 2005-12-20	Update to ANSI-C++



#if !defined __SCAN_H

#define __SCAN_H

#include "b_str.h"

typedef enum
{ undef_include, prep_include, sep_include, ext_include }
include_type;

class SourceFile: public StringObject
{
	RTTIBObject(SourceFile, StringObject)

	static bool printExtendedFlag;

	Set<SourceFile> includefiles;
	include_type includetype;
	char *obfname;


	SourceFile(const char *filename, const char *obfname, include_type incltype = undef_include);
	~SourceFile();
	void add_include_file(const char *filename, include_type incltype);
	include_type get_include_type()
				{ return includetype; }
	Set<SourceFile> &get_include_set()
				{ return includefiles; }
	void print(std::ostream &aOstream);
	void printExt(std::ostream &aOstream);

	const char *get_obfname()
		{ return obfname; }
	bool has_obfname()
		{ return (bool) (strcmp(getKey(), obfname) != 0); }
};

class StringMapObject: public StringObject
{

	RTTIBObject(StringMapObject, StringObject)

	char *mapname;
public:
	StringMapObject(const char *name, const char *mapname);
	~StringMapObject();

	const char *getMapName()			{ return mapname; }
};

class StringMapList: public Set <StringMapObject>
{
	RTTIBObject(StringMapList, Set <StringMapObject>)

public:
	StringMapList( int initsize= INIT_SIZE):
		      Set<StringMapObject>(initsize)
		      { ownsElements(true); }
	bool registerStringMapping(const char *key, const char *mapname);
	const char*getStringMapping(const char *key);

};



extern const char *version;
extern bool debug_mode;
extern bool debugid_mode;
extern int verbose_level;
extern bool filter_mode;
extern bool concat_sourcefiles_flag;
extern bool filename_preserving_flag;
extern bool stringshroud_flag;
extern bool multiple_include_flag;
extern bool do_not_use_a_z_for_shrouding;

extern int right_margin;

extern const char *id_prefix;

extern StringDictionary token_dict;
extern StringDictionary header_dict;

extern List<StringObject> arg_sourcefile_list;
extern List<StringObject> arg_sheader_list;
extern StringDictionary arg_iheader_dict;

extern List<SourceFile> shroud_file_list;
extern Set<SourceFile> shroud_file_set;


extern List<StringObject> searchpath_list;

extern StringDictionary system_macro_dict;
extern StringDictionary id_dict;



extern StringDictionary macros_with_double_hashes_dict;
extern StringDictionary macros_with_single_hashes_dict;
extern StringDictionary args_of_macros_with_double_hashes_dict;
extern StringDictionary args_of_macros_with_single_hashes_dict;
extern StringDictionary concat_token_macro_dict;

extern StringDictionary external_include_dict;
extern StringDictionary possibly_wrong_external_include_dict;
extern StringDictionary special_identifiers_dict;
extern StringDictionary no_processing_macro_dict;

extern StringMapList id_mapping_list, id_inverse_mapping_list;

extern bool write_dictionary;
extern const char *write_dictionary_filename;

extern bool write_mapping;
extern const char *write_mapping_filename;


#define COBF_HEADER	"cobf.h"
#define UNCOBF_HEADER "uncobf.h"

#define COBF_SYSTEM_MACRO "__COBF__"

enum { HASH = 257, IDENTIFIER, STRING, NEWLINE, WHITESPACE,
       NUMBER, CHAR, OP, QUOTE_HASH, CONCAT_HASH };

extern int YYX_LINENO;

#define YY_NEVER_INTERACTIVE 1
#ifdef yywrap
#undef yywrap
#endif
extern "C" int yywrap(void);
void yyerror(char *fmt, ...);

void skip_comment1(void);
void skip_comment2(void);

void skip_string(void);
void skip_tpp_id(void);

void white_space();

bool is_used_as_includefile(const char *sourcefile);

bool processinclude_file(const char *inputfilename,
			const char *outputfilename);
bool ppp_file(const char *filename, const char *ouput_filename, bool sheader_flag);
bool scan_file(const char *filename);
void scan_finished();

bool open_ofstream(std::ofstream &str, const char *filename, bool binary = false);

bool open_cobfheader(const char *outputcobfheader, const char *outputuncobfheader);
bool close_cobfheader();
bool cobf_file(const char *input_filename, const char *output_filename);
bool afterprocess_file(const char *input_filename, const char *output_filename);

enum { QUIET, PROCESS, INFO, DEBUG }; // verbose_level

void get_searchpathname(const char *filename, bool begin_with_act_path, std::ostringstream& searchpathname);

#endif
