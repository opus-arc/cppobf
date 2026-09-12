/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
File:           main.cpp/cc

Desc:           Part of COBF, a program for making C or C++
                sourcefiles obsfuscated.

Author:         Bernhard Baier 
				bernhard.baier@gmx.net
                http://home.arcor.de/bernhard.baier
 

History of Changes:
		        1994-10-21	created

				2003-01-01  V1.04
				2005-12-28  V1.05 Update to ANSI-C++
							new command line arguments '-di' and '-xn'
				2006-01-07	V1.06
							new command line arguments '-mi'  and '-dm'

				No warranties! For copyright see copyright.txt!

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

const char *version = "1.06 2006-01-07";

#include <cstdio>
#include <ctime>

#include "lib.h"
#include "b_tcont.h"
#include "b_str.h"
#include "scan.h"

using namespace std;

List<StringObject> arglist;     /* contains all command line arguments */

List<StringObject> searchpath_list; /* contains all search pathes for
                       source and header files
                       (specified in the command line with the -i option) */

const char *output_dir = NULL;      // output directory
const char *concat_sourcefilename = NULL;// filename for concatenated sourcefiles

bool save_passfiles = false;        // save temporary files

#define TMP_FILE "cobf.tmp"
#define COBF_STAT "cobf.log"

enum { MAX_PASS = 9999 };
int stop_after_pass = MAX_PASS;

const char *pp_script = NULL;           // C preprocessor script
int pp_flag = true;						// perform preprocessing

bool tokens_are_system_macros = false;

// prefix to be added to each identifier
const char *id_prefix = "l"; // default prefix

bool write_dictionary = false;
const char *write_dictionary_filename = "";


bool write_mapping = false;
const char *write_mapping_filename = "";

//////////////////////////////////////////////////////////////////////////



enum {line_bufflen = 400 };
static char line_buffer[line_bufflen];

// fills list "stringlist" with the strings in file "filename"

bool read_stringlist(const char *filename, List<StringObject> &stringlist, bool treat_whitespace_as_newline)
{
    ifstream fh(filename, ios::in);
    if (!fh)
    {
        cerr << "cobf: can't read '" << filename << "'!\n";
        return true;
    }
    while (!fh.eof())
    {
        if (!fh) break;
        get_string(fh, line_buffer, line_bufflen, treat_whitespace_as_newline);
        if (*line_buffer && *line_buffer != '#')
            stringlist.add(*new StringObject(line_buffer));
    }
    fh.close();
    if (verbose_level >= DEBUG)
    {
        cstr str (cout);
        str << "read_stringlist '" << filename << "':\n" << stringlist << "\n\n";
    }

    return false;
}

// fills dictionary "stringdict" with the strings in file "filename"

bool read_stringdict(const char *filename, StringDictionary &stringdict)
{
    int i;
    List<StringObject> stringlist;
    stringlist.ownsElements(true);
    if (read_stringlist(filename, stringlist, false))
        return true;
    for (i = 0; i < stringlist.size(); ++i)
        stringdict.registerString(stringlist[i]);
    return false;
}

bool read_mappingfile(const char *filename,StringMapList &mapping_list, StringMapList &inverse_mapping_list)
{
    int i;
    List<StringObject> stringlist;
    stringlist.ownsElements(true);
    if (read_stringlist(filename, stringlist, false))
        return true;
    for (i = 0; i < stringlist.size(); ++i)
	{
		char *pString;
		const char *mapname = "";
		strcpy(line_buffer, stringlist[i]);
		pString = strpbrk(line_buffer, " \t");
		bool err = false;
		if (pString != NULL)
		{
			*pString = '\0';
			mapname = pString + 1;
			while (*++pString != '\0')
			{
				if (*pString != ' ' && *pString != '\t')
					break;
				++mapname;
			}
			if (strlen(mapname) == 0)
				err = true;
		}
		else
			err = true;
		
		if (err)
		{
			cout << "Error when reading mapping file '" << filename << "' in line " << i+1 << ":\n" << line_buffer << "\n";
			exit (1);
		}
//		cout << "registerStringMapping '" << line_buffer << "', '" << mapname << "'\n";
        if (mapping_list.registerStringMapping(line_buffer, mapname))
		{
			cout << "Error when reading mapping file '" << filename << "' in line " << i+1 << ":\n'" << line_buffer << "' already defined\n";
			exit (1);
		}
        if (inverse_mapping_list.registerStringMapping(mapname, line_buffer))
		{
			cout << "Error when reading mapping file '" << filename << "' in line " << i+1 << ":\n'" << mapname << "' already defined\n";
			exit (1);
		}
	}
    return false;
}


/////////////////////////////////////////////////////////////////////////


// simple check whether a file is readable or not

static bool file_is_readable(const char *filename)
{
    ifstream str(filename, ios::in | ios::binary );
    if (!str)
        return false;
    str.close();
    return true;
}

// Try to find the "filename" in the global "searchpath_list" array
// "begin_with_act_path" = true: search in actual path as a 1st attempt
// return value: full pathname in "searchpathname" (leaves unchanged, when
// "filename" wasn't found!)

void get_searchpathname(const char *filename, bool begin_with_act_path, ostringstream &searchpathname)
{
    int i;
    if (begin_with_act_path && file_is_readable(filename))
    {
        searchpathname << filename;
        return;
    }
    for (i = 0; i < searchpath_list.size(); ++i)
    {
        ostringstream tmp_searchpathname;
        tmp_searchpathname << searchpath_list[i] << PATH_SEPARATOR << filename;
        if (file_is_readable(tmp_searchpathname.str().c_str()))
        {
            searchpathname << tmp_searchpathname.str().c_str();
            return;
        }
    }
}



static bool add_shroud_file(const char *filename)
{
    ostringstream obf_filename;
    if (arg_iheader_dict.isIn(filename))
    {
        cout << "'" << filename << "' already as internal header specified!\n";
        return true;
    }
    if (filename_preserving_flag || concat_sourcefiles_flag)
        obf_filename << get_filename(filename);
    else
    {
        const char *p;
        obf_filename << "a" << shroud_file_set.size();
        if ((p = strrchr(get_filename(filename), '.')) != NULL)
            obf_filename << p;
    }
    SourceFile *newSourceFile = new SourceFile(get_filename(filename), obf_filename.str().c_str());
    shroud_file_set.ownsElements(true);
    if (shroud_file_set.addTest(*newSourceFile))
    {
        cout << "'" << *newSourceFile << "' already in list of sourcefiles!\n";
        delete newSourceFile;
        return true;
    }
    shroud_file_list.add(*newSourceFile);
    return false;
}


/////////////////////////////////////////////////////////////////////////

/*
  Pass 0
  Concat / Copy the sourcefiles (located via the searchpathlist) to the output dir
*/

bool concat_to_output_dir(List<StringObject> &file_list)
{
    int i;
    ostringstream outputfilename;
    outputfilename << output_dir << PATH_SEPARATOR << get_filename(concat_sourcefilename);
    cout << "concat_sourcefiles: '" << concat_sourcefilename << "', '" << outputfilename.str().c_str() << "'\n";
    ofstream str;
    if (open_ofstream(str, outputfilename.str().c_str(), true))
        return true;

    for (i = 0; i < file_list.size(); ++i)
    {
        ostringstream inputfilename;
        get_searchpathname(file_list[i], true, inputfilename);
        if (*inputfilename.str().c_str() == '\0')
        {
            cout << file_list[i] << " not found!\n";
            return true;
        }
        cout << "Appending " << inputfilename.str().c_str() << " ...\n";
        if (append_file(inputfilename.str().c_str(), str))
            return true;
    }
    add_shroud_file(concat_sourcefilename);
    return false;
}

bool copy_to_output_dir(List<StringObject> &file_list)
{
    int i;
    for (i = 0; i < file_list.size(); ++i)
    {
        ostringstream inputfilename;
        get_searchpathname(file_list[i], true, inputfilename);
        if (*inputfilename.str().c_str() == '\0')
        {
            cout << file_list[i] << " not found!\n";
            return true;
        }
//      source_pathfilenamelist.add(* new StringObject(inputfilename) );
        cout << "Reading " << inputfilename.str().c_str() << " ...\n";
        ostringstream outputfilename;
        outputfilename << output_dir << PATH_SEPARATOR << get_filename(file_list[i]);
        if (copy_file(inputfilename.str().c_str(), outputfilename.str().c_str()))
        {
            // Sorry, but I don't know a portable (ANSI-)way to
            // create the output directory automaticly!
            cerr << "Does the output directory '" << output_dir << "' exist?\n";
            cerr << "If not, please create it!\n";
            return true;
        }
        add_shroud_file(file_list[i]);
    }
    return false;
}

bool prepare_working_dir()
{

    if (copy_to_output_dir(arg_sheader_list))
        return true;

    if (concat_sourcefiles_flag)
    {
        if (concat_to_output_dir(arg_sourcefile_list))
            return true;
    }
    else
    {
        if (copy_to_output_dir(arg_sourcefile_list))
            return true;
    }
    return false;
}

/*
   Pass 1
   resolve all #include-Stmts (without using the preprocessor!!!)
*/

bool process_includes()
{
    int i;
    for (i = 0; i < shroud_file_list.size(); ++i)
    {
        ostringstream inputfilename;
        inputfilename << output_dir << PATH_SEPARATOR << get_filename(shroud_file_list[i]);
        ostringstream outputfilename;
        outputfilename << output_dir << PATH_SEPARATOR << TMP_FILE;
        if (processinclude_file(inputfilename.str().c_str(), outputfilename.str().c_str()))
            return true;
        if (rename_file(outputfilename.str().c_str(), inputfilename.str().c_str(), output_dir, save_passfiles))
            return true;
    }
    return false;
}

/*
  Pass 2
  Prepare the sourcefiles for preprocessing
  - Build list of identifiers which should not be shrouded / preprocessed
  - hide identifiers which should not be preprocessed
*/


bool prepare_for_pp()
{
    int i;
    for (i = 0; i < shroud_file_list.size(); ++i)
    {
        ostringstream inputfilename;
        inputfilename << output_dir << PATH_SEPARATOR << get_filename(shroud_file_list[i]);
        ostringstream outputfilename;
        outputfilename << output_dir << PATH_SEPARATOR << TMP_FILE;

        if (ppp_file(inputfilename.str().c_str(), outputfilename.str().c_str(),
            arg_sheader_list.isIn(get_filename(shroud_file_list[i]))))
            return true;
        if (rename_file(outputfilename.str().c_str(), inputfilename.str().c_str(), output_dir, save_passfiles))
            return true;
    }
    return false;
}


/*
  Pass 3
  call preprocessor
*/

bool call_pp()
{
    int i;
    if (!pp_flag) return false;

    for (i = 0; i < shroud_file_list.size(); ++i)
    {
        ostringstream cmd;
        ostringstream inputfilename;
        inputfilename << output_dir << PATH_SEPARATOR << get_filename(shroud_file_list[i]);
        ostringstream outputfilename;
        outputfilename << output_dir << PATH_SEPARATOR << TMP_FILE;
        cmd << pp_script << " " <<
        output_dir << PATH_SEPARATOR << get_filename(shroud_file_list[i]) << " " <<
        output_dir << PATH_SEPARATOR << TMP_FILE;
        exec_cmd(cmd.str().c_str());
        if (rename_file(outputfilename.str().c_str(), inputfilename.str().c_str(), output_dir, save_passfiles))
        {
            cerr << "Problem with the command '" << cmd.str().c_str() << "'?\n";
            return true;
        }
    }
    return false;
}

/*
  Pass 4
  build identifier dictionary
*/

bool build_id_dictionary()
{
    int i;
    for (i = 0; i < shroud_file_list.size(); ++i)
    {
        ostringstream inputfilename;
        inputfilename << output_dir << PATH_SEPARATOR << get_filename(shroud_file_list[i]);

        if (scan_file(inputfilename.str().c_str()))
            return true;
    }

    scan_finished();

    return false;
}

/*
  Pass 5
  write shrouded sourcefiles
*/

static void set_reset_counter(Set<StringWithCount> &id_set)
{
    int i;
    for (i = 0; i < id_set.size(); ++i)
        id_set[i].resetCount();
}

bool cobf_filelist()
{
    int i;
    ostringstream outputcobfheader;
    outputcobfheader << output_dir << PATH_SEPARATOR << COBF_HEADER;

    ostringstream outputuncobfheader;
    outputuncobfheader << output_dir << PATH_SEPARATOR << UNCOBF_HEADER;


    if (token_dict.size() > 0)
        if (open_cobfheader(outputcobfheader.str().c_str(), outputuncobfheader.str().c_str()))
            return true;

    // reset counter in token_dict (used as flag in function "write_id")
    set_reset_counter(token_dict);

    for (i = 0; i < shroud_file_list.size(); ++i)
    {
        ostringstream inputfilename;
        inputfilename << output_dir << PATH_SEPARATOR << get_filename(shroud_file_list[i]);
        ostringstream outputfilename;
        outputfilename << output_dir << PATH_SEPARATOR << TMP_FILE;
        if (cobf_file(inputfilename.str().c_str(), outputfilename.str().c_str()))
            return true;
        if (rename_file(outputfilename.str().c_str(), inputfilename.str().c_str(), output_dir, save_passfiles))
            return true;
    }
    if (token_dict.size() > 0)
        close_cobfheader();
    return false;
}

/*
  Pass 6
  some kind of optimizations: remove empty lines and redundant include stmts
*/

bool afterprocess_filelist()
{
    int i;
    for (i = 0; i < shroud_file_list.size(); ++i)
    {
        ostringstream inputfilename;
        inputfilename << output_dir << PATH_SEPARATOR << get_filename(shroud_file_list[i]);
        ostringstream outputfilename;
        outputfilename << output_dir << PATH_SEPARATOR << TMP_FILE;
        if (afterprocess_file(inputfilename.str().c_str(), outputfilename.str().c_str()))
            return true;
        if (rename_file(outputfilename.str().c_str(), inputfilename.str().c_str(), output_dir, save_passfiles))
            return true;
    }
    return false;
}

bool rename_sourcefiles()
{
        int i;
    for (i = 0; i < shroud_file_list.size(); ++i)
    {
        SourceFile &actSourceFile = shroud_file_list[i];
        if (actSourceFile.has_obfname())
        {
            ostringstream oldfilename;
            oldfilename <<  output_dir << PATH_SEPARATOR << get_filename(actSourceFile);
            ostringstream newfilename;
            newfilename << output_dir << PATH_SEPARATOR << actSourceFile.get_obfname();
            if (rename_file(oldfilename.str().c_str(), newfilename.str().c_str(), output_dir, save_passfiles))
                return true;
        }
    }
    return false;
}


////////////////////////////////////////////////////////////////////////

// general function to print a sorted list

static void write_list(ostream &ostr, XContainer &list, const char *full_header, const char *empty_header)
{
    int i;
    cstr str(ostr);

//  str << list.size() << " elements\n";
    if (list.size() == 0)
    {
        if (*empty_header)
            str << empty_header << "\n";
        return;
    }

    XArray sorted_list;
    sorted_list += list;
    sorted_list.sort();

    str << full_header;

	for (i = 0; i < sorted_list.size(); ++i)
        str << sorted_list[i] << "\n";
    str << "\n";
}



void write_special_id_list(ostream &str)
{
    int i, j;
    Set<StringObject> special_id_set;
    for (i = 0; i < id_dict.size(); ++i)
    {
        const char *actId = id_dict[i];
        for (j = 0; j < concat_token_macro_dict.size(); ++j)
        {
            const char *actConcatId = concat_token_macro_dict[j];
            if (strlen(actConcatId) < 2) continue;
            if (strstr(actId, actConcatId) != NULL)
            {
                special_id_set.add(id_dict[i]);
                break;
            }
        }
    }

    write_list(str,
    special_id_set,
    "These identifiers were detected as substrings of identifiers with context of the '##' operator "
    "(and length > 1):\n",

    "");
}


static int write_stat(const char *outputname)
{
    int i;
    ofstream str(outputname);
    if (!str) return 1;

    if (verbose_level >= INFO)
        cout << "writing statistics to '" << outputname << "'!\n";

    time_t t;

    time(&t);
    str << "Logfile for shrouding at " << ctime(&t) << "\n";

    str << "output dirctory: " << output_dir << "\n";
    if (pp_flag)
        str << "shell script for performing preprocessing: " << pp_script << "\n";
    else
        str << "No preprocessing done!\n";
    if (stop_after_pass != MAX_PASS)
        str << "Warning: cobf stopped after pass " << stop_after_pass << "!\n";

    str << "\n";

    str <<
    "(P) preprocessed header\t\t(S) separately shrouded header\n"
    "(X) external header\n\n";

    SourceFile::printExtendedFlag = true;

    List<BObject> tmp_list;
    for (i = 0; i < arg_sourcefile_list.size(); ++i)
    {
        SourceFile *pSourceFile = shroud_file_set.get(get_filename(arg_sourcefile_list[i]));
        if (pSourceFile != NULL)
            tmp_list.add(*pSourceFile);
        else
            tmp_list.add(arg_sourcefile_list[i]);
    }

    write_list(str, tmp_list,
    "List of shrouded sourcefiles:\n\n",
    "");

    if (concat_sourcefiles_flag)
        str << "All sourcefiles were concatenated to the following file:\n" <<
            shroud_file_set.getSafe(concat_sourcefilename) << "\n\n";

    tmp_list.reset();
    for (i = 0; i < arg_sheader_list.size(); ++i)
        tmp_list.add(shroud_file_set.getSafe(get_filename(arg_sheader_list[i])));
    write_list(str, tmp_list,
    "List of shrouded headerfiles:\n",
    "");

    SourceFile::printExtendedFlag = false;


    write_list(str, arg_iheader_dict,
    "The following headerfiles were included by cobf:\n",

    "No headerfile was included by cobf!\n");


    write_list(str, external_include_dict,
    "The following external includefiles were found:\n"
    "(all external visible identifiers in this files MUST be defined as tokens for cobf!!)\n",

    "No external include files were found!\n");


    write_list(str, possibly_wrong_external_include_dict,
    "**** PROBLEM ****\n"
    "In the above list of external includefiles were these included with \"..\"! "
    "or are part of the list of sourcefiles. If this is not correct, "
    "do these headers specifiy via the -hx or -hi option!\n",

    "");

    write_list(str, macros_with_double_hashes_dict,
	"Info: The following macros contain directly or indirectly the '##' operator.\n",

    "");

    write_list(str,
    args_of_macros_with_double_hashes_dict,
    "The following identifiers were detected as arguments of the above macros which "
    "contain directly or indirectly the '##' operator. "
    "(Please check carefully whether these identifiers shouldn't be shrouded!):\n",

    "");


    write_list(str, macros_with_single_hashes_dict,
	"Info: The following macros contain directly or indirectly the '#' operator:\n",

    "");



    write_list(str,
    args_of_macros_with_single_hashes_dict,
    "The following identifiers were detected as arguments of the above macros which "
    "contain directly or indirectly the '#' operator. "
    "(Please check carefully whether these identifiers shouldn't be shrouded!):\n",

    "");

    write_list(str,
    concat_token_macro_dict,
    "These identifiers were detected in the context of the '##' operator "
    "(and might therefore [or identifiers with such substrings] not be shrouded!):\n",

    "");

    write_special_id_list(str);


    write_list(str,
    special_identifiers_dict,
    "These identifiers with the prefix \"__\" were detected:\n",

    "");


    str.close();

    return 0;
}

void write_statistics()
{
    ostringstream outputname;
    outputname << output_dir << PATH_SEPARATOR << COBF_STAT;

    if (write_stat(outputname.str().c_str()))
        cout << "can't write '" << outputname.str().c_str() << "'\n";
    else
        cout << "Statistics to '" << outputname.str().c_str() << "' written!\n";
}


bool cobf()
{
    int i;
    if (tokens_are_system_macros)
    {
        for (i = 0; i < token_dict.size(); ++i)
            system_macro_dict.registerString(token_dict[i]);
        token_dict.reset();
    }
        system_macro_dict.registerString(COBF_SYSTEM_MACRO);

    if (prepare_working_dir())
        return 1;
	
	sync_streams();

    if (verbose_level >= DEBUG)
        cout << "stop after pass " << stop_after_pass << "\n";
    if (stop_after_pass == 0) return 0;

	sync_streams();

    if (verbose_level >= PROCESS)
        cout << "*** PASS 1 ***\n";
    if (process_includes())
        return 1;
    if (stop_after_pass == 1) return 0;

    if (pp_flag)
    {
    if (verbose_level >= PROCESS)
        cout << "*** PASS 2 ***\n";
    if (prepare_for_pp())
        return 1;
    if (stop_after_pass == 2) return 0;
    }

    if (pp_flag)
    {
    if (verbose_level >= PROCESS)
        cout << "*** PASS 3 ***\n";
    if (call_pp())
        return 1;
    if (stop_after_pass == 3) return 0;
    }

    if (verbose_level >= PROCESS)
        cout << "*** PASS 4 ***\n";
    if (build_id_dictionary())
        return 1;
    if (stop_after_pass == 4) return 0;

    if (verbose_level >= PROCESS)
        cout << "*** PASS 5 ***\n";
    if (cobf_filelist())
        return 1;
    if (stop_after_pass == 5) return 0;

    if (verbose_level >= PROCESS)
        cout << "*** PASS 6 ***\n";
    if (afterprocess_filelist())
        return 1;
    if (stop_after_pass == 6) return 0;

    rename_sourcefiles();

    write_statistics();
    return 0;
}

/////////////////////////////////////////////////////////////////////////////

bool read_invocationfile(const char *filename, List<StringObject> &arglist)
{
    if (read_stringlist(filename, arglist, true))
        return true;
    arglist.add(*new StringObject("-"));
    return false;
}

bool create_arglist(char **argv, List<StringObject> &arglist)
{
    arglist.ownsElements(true);
    while (*argv)
    {
        char *arg = *argv;
        if (*arg == '@')
        {
            if (read_invocationfile(&arg[1], arglist))
                return true;
        }
        else
            arglist.add(*new StringObject(arg));
        ++argv;
    }
    // add dummy argument (simplifies end test of arglist)
    arglist.add(*new StringObject("-"));
    return false;
}

const char *check_arg(const char *arg)
{
    if (*arg == '-')
    {
        cerr << "cobf: Warning: expecting parameter instead of '" << arg << "'!\n";
        exit (1);
    }
    return arg;
}


void invocation()
{
    cout <<
    "This is COBF (" << version << ") the C/C++ Sourcecode Obfuscator/Shrouder\n"
    "(C) by Bernhard Baier\n"
    "email:\tbernhard.baier@gmx.net\n"
    "WWW:\thttp://home.arcor.de/bernhard.baier\n\n"
    "syntax: cobf options [@invocationfile] ... filename [...]\n\n"
    "Main options:\n"

    "-hi filename\theaderfile which will be not separately shrouded and\n"
        "\t\tinstead included by cobf\n"
    "-hs filename\theaderfile which will be separately shrouded\n"
    "-i path\t\tadd search path for source and header files\n"
    "-m filename\tfile with system macro list\n"
        "\t\t(will not be shrouded or preprocessed)\n"
	"-mi filename\tSpecifies a identifier mapping file\n"
    "-o outputdir\tdestination for shrouded files\n"
    "-p batchfile\tscript for preprocessor\n"
        "\t\t(1st argument input file, 2nd input file)\n"
    "-t filename\tfile with token list (only shrouded by preprocessor)\n"

    "\nOutput options:\n"
    "-b\t\tpreserve original filenames in output directory\n"
    "-c filename\tconcatenate all sourcefiles to filename\n"
	"-dd filename\tdump identifier dictionary to filename\n"
	"-dm filename\tdump identifier mapping list to filename\n"
    "-g\t\tdo not shroud strings\n"
    "-r right margin\tfor shrouded output files\n"
    "-u\t\ttreat token lists as system makro lists\n"
	    "\t\t(-> no shrouding on preprocessor level)\n"
	"-x prefix\tuse prefiX for shrouded identifiers (default is \'l\')\n"
	"-xn\t\tdo not use characters 'a' - 'z' after the prefix\n"
		"\t\tfor the most used identifiers\n"

    "\nDebug options:\n"
    "-a\t\tdo not delete tmp files generated by each pass\n"
    "-d\t\tinclude debug info in shrouded files\n"
	"-di\t\tadd original identifier to each shrouded identifier\n"
	"-f\t\tfilter mode (no shrouding)\n"
    "-n\t\tno preprocessing\n"
    "-s\t\tstop after pass 1 (optional -s0 to -s5)\n"
    "-v\t\tverbose (optional -v1 to -v9 to increase verbose level)\n"
    ;
}

int cobf_main(int argc, char **argv)
{
    int i;
    if (argc < 2)
    {
        invocation();
        return 1;
    }
    cout << "--- " << " COBF " << version << " ---\n";
    if (create_arglist(argv, arglist))
        return 1;
//  cout << "arglist: " << arglist << "\n\n";
    bool ret = 0;
    for (i = 1; i < arglist.size(); ++i)
    {
        const char *arg = arglist[i], *arg_old;
//        cout << i << ":" << arg << "\n";
        if (*arg == ';') continue; // treat it as a comment
        if (*arg != '-')
        {
            arg_sourcefile_list.add(*new StringObject(arg));
            arg_sourcefile_list.ownsElements(true);
            continue;
        }
        switch (arg[1])
        {
        case '\0':
            break;
        case 't':
            ++i;
            ret |= read_stringdict(check_arg(arglist[i]), token_dict);
            break;
        case 'm':
			if (arg[2] == 'i')
			{
				++i;
				ret |= read_mappingfile(check_arg(arglist[i]), id_mapping_list, id_inverse_mapping_list);
			}
			else
			{
				++i;
				ret |= read_stringdict(check_arg(arglist[i]), system_macro_dict);
			}
            break;
        case 'o':
            ++i;
            output_dir = check_arg(arglist[i]);
            break;
        case 'f':
            break;
        case 'd':
			if (arg[2] == 'i')
				debugid_mode = true;
			else if (arg[2] == 'd')
			{
				write_dictionary = true;
				++i;
				write_dictionary_filename = arglist[i];
			}
			else if (arg[2] == 'm')
			{
				write_mapping = true;
				++i;
				write_mapping_filename = arglist[i];
			}
			else
				debug_mode = true;
            break;
        case 'v':
            verbose_level = PROCESS;
            if (arg[2] != '\0')
                verbose_level = arg[2] - '0';
            break;
        case 's':
            stop_after_pass = 1;
            if (arg[2] != '\0')
                stop_after_pass = arg[2] - '0';
            break;
        case 'n':
            pp_flag = false;
            break;
        case 'g':
            stringshroud_flag = false;
            break;
        case 'p':
            ++i;
            pp_script = check_arg(arglist[i]);
            break;
        case 'h':
            arg_old = arg;
            ++i;
            arg = check_arg(arglist[i]);
            switch (arg_old[2])
            {
                case 's':
                    arg_sheader_list.add(*new StringObject(arg));
                    arg_sheader_list.ownsElements(true);
                    break;
                case 'i':
                    arg_iheader_dict.registerString(arg);
                    break;
                default:
                    cerr << "cobf: unknown option '" << arg << "'\n";
                    return 1;
            }
            break;
        case 'i':
            ++i;
            arg = check_arg(arglist[i]);
            searchpath_list.add(*new StringObject(arg));
            searchpath_list.ownsElements(true);
            break;
        case 'r':
            if (arg[2] != 0)
                sscanf(&arg[2], "%d", &right_margin);
            else
            {
                ++i;
                arg = check_arg(arglist[i]);
                sscanf(arg, "%d", &right_margin);
            }
            break;
        case 'b':
            filename_preserving_flag = true;
            break;
        case 'c':
            concat_sourcefiles_flag = true;
            ++i;
            concat_sourcefilename = check_arg(arglist[i]);
            break;
        case 'a':
            save_passfiles = true;
            break;
        case 'u':
            tokens_are_system_macros = true;
            break;
        case 'x':
			if (arg[2] == 'n')
			{
				do_not_use_a_z_for_shrouding = true;
			}
			else
			{
      			++i;
      			id_prefix = check_arg(arglist[i]);
			}
            break;
        default:
            cerr << "cobf: unknown option '" << arg << "'\n";
            return 1;
        }
    }

    if (verbose_level >= DEBUG)
    {
        cout << "arglist: " << arglist << "\n";
        cout << "arg_sourcefile_list: " << arg_sourcefile_list << "\n";
        cout << "arg_sheader_list: " << arg_sheader_list << "\n";
        cout << "arg_iheader_dict: " << arg_iheader_dict << "\n\n";
    }

    if (output_dir == NULL)
    {
        cerr << "cobf: -o option for output directory missing!\n";
        return 1;
    }

    if (pp_script == NULL && pp_flag)
    {
        cerr << "cobf: -p option for preprocessor script missing!\n";
        return 1;
    }
    if (arg_sourcefile_list.size() < 1)
    {
        cerr << "cobf: no sourcefiles specified!\n";
        return 1;
    }
    if (!ret)
        ret = cobf();
	sync_streams();
    return ret;
}

int main(int argc, char **argv)
{

#if __MEMCHECK > 0
    viewMemList();
#endif
    int ret = cobf_main(argc, argv);
#if 0
#if __MEMCHECK > 0
    cout << "before reset\n";
    viewMemList();
#endif
#endif
    // This reset stmts are normally redundant
    // But they help to detect memory leakage
    // if heap checking is turned on
    arglist.reset();
    arg_sourcefile_list.reset();
    arg_sheader_list.reset();
    arg_iheader_dict.reset();

    shroud_file_list.reset();
    shroud_file_set.reset();

    searchpath_list.reset();
    token_dict.reset();
    id_dict.reset();
    system_macro_dict.reset();
	id_mapping_list.reset();
#if __MEMCHECK > 0
    cout << "after reset\n";
    viewMemList();
#endif
    return ret;
}
