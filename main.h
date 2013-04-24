/*
 *
 *
 *
 *
 *
 **********/

/*****************************************************************************\
|********************************** includes *********************************|
\*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <pthread.h>
#include <objc/objc.h>
#include <strings.h>
#include <regex.h>
#include <time.h>
#define  CLOCK_MONOTONIC      1

#include "substrate.h"
#include <libxml/tree.h>
#include <libxml/parser.h>


#include <mach-o/dyld.h>

/*****************************************************************************\
|******************************** structures *********************************|
\*****************************************************************************/

//don't need a drawback na ?
typedef struct {
    //compiled regular expressions, AND logic
    regex_t*    re_module;
    regex_t*    re_class;
    regex_t*    re_method;
    regex_t*    re_lr;
    regex_t*    re_r0;
    regex_t*    re_r1;
    regex_t*    re_r2;
    regex_t*    re_r3;
    //pointer to the next filter, OR logic, 0 if none
    void*       p_next;
} ifilter;

typedef struct {
    //to hook or not to hook
    bool        enabled;
    int         verbose;
    //output options
    void*       output_stream;
    int         output_type;
    int         output_format;
    //filter caller module
    regex_t*       module_filter;
    //hot reloading of configuration file
    bool        hot_reload;
    //file checksum for reloading purpose
    uint32_t    chksm;
    //depth limit
    int         max_depth;
    //pointers to ifilter chained list
    void*       whitelist;
    void*       blacklist;
} iconf;

typedef struct {
    xmlChar*    name;
    xmlChar*    value;
    xmlNode*    next;
    xmlNode*    children;
} one_node;

typedef struct {
    void*       p_next;
    void*       p_prev;
    uint32_t    returned;
    uint32_t    depth;
    uint32_t    lr;
    uint32_t    r0;
    uint32_t    r1;
    uint32_t    r2;
    uint32_t    r3;
    uint32_t    r4;
    uint32_t    r5;
    uint32_t    r6;
    uint32_t    r7;
    uint32_t    r8;
    uint32_t    r9;
    uint32_t    r10;
    uint32_t    r11;
    time_t      start_time;
    time_t      end_time;
} context;

typedef struct {
    pthread_key_t       tkey;
    pthread_t       thread_id;
    context*            p_context;
    struct threadctx*   p_next;
} threadctx;

/*****************************************************************************\
|******************************** prototypes *********************************|
\*****************************************************************************/

//mobile substrate wrappers
void init_hook();
void clean_hook( );
//real hook stuff
void hook_callback( id self , SEL op );
id itrace( id self, SEL op );
//thread specific funtions
void clean_per_thread( );
context* get_last_context( );
context* get_next_context( context* p_context );
uint32_t add_context( );
uint32_t create_trace( );
uint32_t close_trace( );
context* alloc_context( );


context* get_last_unended_trace( context* p_context );
context* get_last_trace( context* p_context );
context* init_thread( );
context* get_thread_context( );
threadctx* get_thread( );


char* set_padding( char* padding_str , uint32_t fu );
//configuration related stuff
bool is_filtered( int type ,  ifilter* filter , char* pstr_class , char* pstr_method );
regex_t* load_regexp( char* regexp );
ifilter* load_filter_list( one_node* lst_node );
void clear_filter_list( ifilter* filter );
iconf* load_conf( iconf* opt , char* fname );
uint32_t crc32(uint32_t crc, const void *buf, size_t size);
bool is_whitelisted( ifilter* filter , context* p_context, char* pstr_module, char* pstr_class , char* pstr_method );
bool is_blacklisted( ifilter* filter , context* p_context, char* pstr_module, char* pstr_class , char* pstr_method );


void itrace_output( char* output , ... );
void output_trace_start( context* p_contextfu , char* p_caller, char* p_classname , char* p_method );
void output_trace_end( context* p_contextfu );

//fufufu teh warning ;)
void NSLog( char* str, ... );

/*****************************************************************************\
|******************************* global vars *********************************|
\*****************************************************************************/
#ifdef __GLOBALS__
//usefull var
void* p_objc_msgSend = 0;
unsigned long original_msgSend=0;

uint32_t ret, align = 0;

pthread_key_t tkey = 0;
unsigned long tmp_lr = 0;
unsigned long p_class = 0;
char* p_classname = 0;
iconf* opt = 0;
context* p_context=0;
context* p_current_context=0;
uint32_t cur_depth=0;
threadctx* p_thread_base=0;
threadctx* p_thread = 0;
Dl_info* mod_info=0;

time_t itrace_start;
time_t itrace_end;

int show_trace = true;
char* p_caller = "nill";
#endif

/*****************************************************************************\
|********************************* constants *********************************|
\*****************************************************************************/

#define DEBUG           1
#define REGEXP_MODE     REG_ICASE
#define DWORD_FORMAT    "%08x"
#define REGEXP_FLAGS    ""
#define MODULE_SELF     "self"
#define MAXPATHLEN      2048

#define OPT_ENABLED     "enabled"
#define OPT_VERBOSE     "verbose²"
#define OPT_OUTPUT      "output"
#define OPT_CALLER      "module"
#define OPT_OUT_TYPE    "output_type"
    #define OUT_TYPE_STDOUT     "stdout"
    #define OUT_TYPE_FILE       "file"
    #define OUT_TYPE_NSLOG      "nslog"
#define OPT_OUT_FORMAT  "output_format"
    #define OUT_FORMAT_TXT    "txt"
    #define OUT_FORMAT_XML    "xml"
#define OPT_HOT_RELOAD  "hot_reload"
#define OPT_MAX_DEPTH   "max_depth"
#define OPT_WHITELIST   "whitelist"
#define OPT_BLACKLIST   "blacklist"
    #define OPT_FILTER      "filter"
        #define OPT_MODULE      "module"
        #define OPT_CLASS       "class"
        #define OPT_METHOD      "method"
        #define OPT_LR          "lr"
        #define OPT_R0          "r0"
        #define OPT_R1          "r1"
        #define OPT_R2          "r2"
        #define OPT_R3          "r3"
#define OPT_COMMENT     "comment"
#define OPT_TEXT        "text"
#define OPT_NONE        "none"

#define VERBOSE_DEBUG   1
#define VERBOSE_QUIET   0

#define OTYPE_STDOUT    1
#define OTYPE_FILE      2
#define OTYPE_NSLOG     3

#define OFORMAT_TXT     1
#define OFORMAT_XML     2

#define PADDING_TXT     "-"
#define PADDING_XML     "\t"

#define TXT_FORMAT_START    "0x%08x %s %s->%s\n"
#define TXT_FORMAT_END      "%08x\t%s %ldsec\n"
#define XML_FORMAT_START    "%s<trace>\n\
%s\t<caller_module>%s</caller_module>\n\
%s\t<class>%s</class>\n\
%s\t<method>%s</method>\n\
%s\t<depth>%d</depth>\n\
%s\t<lr>%08x</lr>\n\
%s\t<r0>%08x</r0>\n\
%s\t<r1>%08x</r1>\n\
%s\t<r2>%08x</r2>\n\
%s\t<r3>%08x</r3>\n\
%s\t<r4>%08x</r4>\n\
%s\t<r5>%08x</r5>\n\
%s\t<r6>%08x</r6>\n\
%s\t<r7>%08x</r7>\n\
%s\t<r8>%08x</r8>\n\
%s\t<r9>%08x</r9>\n\
%s\t<r10>%08x</r10>\n\
%s\t<r11>%08x</r11>\n\
%s\t<start_time>%ld</start_time>\n"
#define XML_FORMAT_END    "%s\t<end_time>%ld</endtime>\n\
%s</trace>\n"
