#include "main.h"


/**************************************************************************************************\
|****************************************** output stuff ******************************************|
\**************************************************************************************************/

/* generate the padding str
 ***/
char* set_padding( char* padding_str , uint32_t fu ){
	char* buf = calloc( strlen( padding_str ) , fu+1 );
	int i=fu;
	for( ; i > 0; i-- ){
		strcat( buf , padding_str );
	}
	return buf;
}

/* usefull for debugging, you know it too
 ***/
void print_context( context* p_context ){
	printf( "p_next:\t%08x\t\tretnd:\t%08x\t\tdepth:\t%08x\nr0:\t%08x\t\tr1:\t%08x\t\tr2:\t%08x\t\tr3:\t%08x\nr4:\t%08x\t\tr5:\t%08x\t\tr6:\t%08x\t\tr7:\t%08x\nr8:\t%08x\t\tr9:\t%08x\t\tr10:\t%08x\t\tr11:\t%08x\nlr:\t%08x\n\n",
			(unsigned int) p_context->p_next,
			(unsigned int) p_context->returned,
			(unsigned int) p_context->depth,
			(unsigned int) p_context->r0, (unsigned int) p_context->r1, (unsigned int) p_context->r2, (unsigned int) p_context->r3, 
			(unsigned int) p_context->r4, (unsigned int) p_context->r5, (unsigned int) p_context->r6, (unsigned int) p_context->r7, 
			(unsigned int) p_context->r8, (unsigned int) p_context->r9, (unsigned int) p_context->r10, (unsigned int) p_context->r11,
			(unsigned int) p_context->lr
	);
}

/**************************************************************************************************\
|*************************************** conf related stuff ***************************************|
\**************************************************************************************************/
/* handle loading of default configuration file
 ***/
void init_default_conf( iconf* opt ){
	opt->enabled = true;
	opt->verbose = VERBOSE_DEBUG;
	opt->output_stream = 0;
	opt->output_type = OTYPE_STDOUT;
	opt->output_format = OFORMAT_TXT;
	opt->hot_reload = true;

	opt->max_depth = 0;
	opt->whitelist = NULL;
	opt->blacklist = NULL;
}

one_node* get_node( xmlNode* cur_node ){
	if( cur_node ){
		if( xmlStrcasecmp( cur_node->name , OPT_COMMENT ) && xmlStrcasecmp( cur_node->name , OPT_TEXT )){
			one_node* node = calloc( sizeof( one_node ) , 1 );
			node->name = (char*) cur_node->name;
			node->value = xmlNodeGetContent( cur_node );
			node->next = cur_node->next;
			node->children = cur_node->children;
			return node;
		} else {
			return get_node( cur_node->next );
		}
	} else {
		return (one_node*) 0;
	}
}

one_node* get_next( one_node* node ){
	return get_node( node->next );
}

one_node* get_child( one_node* node ){
	return get_node( node->children );
}

/**************************************************************************************************\
|****************************************** filter stuff ******************************************|
\**************************************************************************************************/


char* dword2str( uint32_t value ){
	char* ret = calloc( 1 , 9 ); //4 byte long -> 8 char
	sprintf( ret , DWORD_FORMAT , value );
	return ret;
}

/* check if this entry match one of registred filter
 ***/
bool is_whitelisted( ifilter* filter , context* p_context, char* pstr_module , char* pstr_class , char* pstr_method ){
	int valid , count= 0;
	while( filter > 0 ){
		count, valid = 0;
		if( filter->re_module != 0 ){
			count++;
			if( regexec( filter->re_module, pstr_module , 0, NULL, 0 )) valid++;
		}
		if( filter->re_class != 0 ){
			count++;
			if( regexec( filter->re_class, pstr_class , 0, NULL, 0 )) valid++;
		}
		if( filter->re_method != 0 ){
			count++;
			if( regexec( filter->re_method, pstr_method , 0, NULL, 0 )) valid++;
		}
		if( filter->re_lr != 0 ){
			count++;
			if( regexec( filter->re_lr, dword2str( p_context->lr ) , 0, NULL, 0 )) valid++;
		}
		if( filter->re_r0 != 0 ){
			count++;
			if( regexec( filter->re_r0, dword2str( p_context->r0 ) , 0, NULL, 0 )) valid++;
		}
		if( filter->re_r1 != 0 ){
			count++;
			if( regexec( filter->re_r1, dword2str( p_context->r1 ) , 0, NULL, 0 )) valid++;
		}
		if( filter->re_r2 != 0 ){
			count++;
			if( regexec( filter->re_r2, dword2str( p_context->r2 ) , 0, NULL, 0 )) valid++;
		}
		if( filter->re_r3 != 0 ){
			count++;
			if( regexec( filter->re_r3, dword2str( p_context->r3 ) , 0, NULL, 0 )) valid++;
		}
		if( valid == count ){
			return 1;
		}
		filter = filter->p_next;
	}
	return 0;
}

/* fu optimization, go to hell fraking logic
 ***/
bool is_blacklisted( ifilter* filter , context* p_context, char* pstr_module , char* pstr_class , char* pstr_method ){
	while( filter > 0 ){
		/*//will gcc handle that properly ?
		if(    ( filter->re_module == 0 || ( filter->re_module != 0 && !regexec( filter->re_module, pstr_module , 0, NULL, 0 )))
			&& ( filter->re_class == 0 || ( filter->re_class != 0 && !regexec( filter->re_class, pstr_class , 0, NULL, 0 )))
			&& ( filter->re_method == 0 || ( filter->re_method != 0 && !regexec( filter->re_method, pstr_method , 0, NULL, 0 )))
			&& ( filter->re_lr == 0 || ( filter->re_lr != 0 && !regexec( filter->re_lr, dword2str( p_context->lr ) , 0, NULL, 0 )))
			&& ( filter->re_r0 == 0 || ( filter->re_r0 != 0 && !regexec( filter->re_r0, dword2str( p_context->r0 ) , 0, NULL, 0 )))
			&& ( filter->re_r1 == 0 || ( filter->re_r1 != 0 && !regexec( filter->re_r1, dword2str( p_context->r1 ) , 0, NULL, 0 )))
			&& ( filter->re_r2 == 0 || ( filter->re_r2 != 0 && !regexec( filter->re_r2, dword2str( p_context->r2 ) , 0, NULL, 0 )))
			&& ( filter->re_r3 == 0 || ( filter->re_r3 != 0 && !regexec( filter->re_r3, dword2str( p_context->r3 ) , 0, NULL, 0 )))){
			return 1;
		}*/
		if( filter->re_module != 0 && !regexec( filter->re_module, pstr_module , 0, NULL, 0 )){
			return 1;
		} else if( filter->re_class != 0 && !regexec( filter->re_class, pstr_class , 0, NULL, 0 )){
			return 1;
		} else if( filter->re_method != 0 && !regexec( filter->re_method, pstr_method , 0, NULL, 0 )){
			return 1;
		} else if( filter->re_lr != 0 && !regexec( filter->re_lr, dword2str( p_context->lr ) , 0, NULL, 0 )){
			return 1;
		} else if( filter->re_r0 != 0 && !regexec( filter->re_r0, dword2str( p_context->r0 ) , 0, NULL, 0 )){
			return 1;
		} else if( filter->re_r1 != 0 && !regexec( filter->re_r1, dword2str( p_context->r1 ) , 0, NULL, 0 )){
			return 1;
		} else if( filter->re_r2 != 0 && !regexec( filter->re_r2, dword2str( p_context->r2 ) , 0, NULL, 0 )){
			return 1;
		} else if( filter->re_r3 != 0 && !regexec( filter->re_r3, dword2str( p_context->r3 ) , 0, NULL, 0 )){
			return 1;
		}
		filter = filter->p_next;
	}
	return 0;
}

/* clean filter list
 ***/
void clear_filter_list( ifilter* filter ){
	ifilter* one_filter = filter;
	while( filter > 0 ){
		filter = one_filter->p_next;
		if( one_filter->re_class != 0 )
			regfree( one_filter->re_class );
		if( one_filter->re_method != 0 )
			regfree( one_filter->re_method );
		free( one_filter );
	}
}

regex_t* load_regexp( char* regexp ){
	regex_t* re = calloc( sizeof( regex_t ) , 1 );
	if( DEBUG ) printf("re: %s\n" , regexp );
	if( xmlStrcasecmp( regexp , OPT_NONE ) && strlen( regexp ) != 0 ){
		if( !regcomp( re , regexp , REGEXP_MODE )){
			return re;
		} else {
			fprintf( stderr , "[x] malformed regular expression '%s'\n" , regexp  );
			return (regex_t*) 0;
		}
	} else {
		fprintf( stderr , "[!] empty regexp found\n" );
	}
	return (regex_t*) 0;
}

void load_filter( ifilter* filter , one_node* node ){
	if( !xmlStrcasecmp( node->name , OPT_MODULE )){
		if( !xmlStrcasecmp( node->value , MODULE_SELF )){
			uint32_t maxpathlen = MAXPATHLEN;
			filter->re_module = calloc( 1 , maxpathlen );
			_NSGetExecutablePath( (char*) filter->re_module , &maxpathlen );
		} else {
			filter->re_module = load_regexp( node->value );
		}
	} else if( !xmlStrcasecmp( node->name , OPT_CLASS )){
		filter->re_class = load_regexp( node->value );
	} else if( !xmlStrcasecmp( node->name , OPT_METHOD )){
		filter->re_method = load_regexp( node->value );
	} else if( !xmlStrcasecmp( node->name , OPT_LR )){
		filter->re_lr = load_regexp( node->value );
	} else if( !xmlStrcasecmp( node->name , OPT_R0 )){
		filter->re_r0 = load_regexp( node->value );
	} else if( !xmlStrcasecmp( node->name , OPT_R1 )){
		filter->re_r1 = load_regexp( node->value );
	} else if( !xmlStrcasecmp( node->name , OPT_R2 )){
		filter->re_r2 = load_regexp( node->value );
	} else if( !xmlStrcasecmp( node->name , OPT_R3 )){
		filter->re_r3 = load_regexp( node->value );
	} else {
		if( DEBUG ) {
			fprintf( stderr , 
				"[x] malformed plist file, filter must contains module, method, class, lr, r0, r1, r2, or r3 regexp, this one is none of them\n" 
			);
		}
	}
}

/* handle filter node
 ***/
ifilter* load_filter_list( one_node* lst_node ){
	ifilter *first, *filter = 0;
	int count=0;
	regex_t* re = calloc( sizeof( regex_t ) , 1 );
	if( !xmlStrcasecmp( lst_node->name , OPT_FILTER )){
		filter = first = calloc( sizeof( ifilter ) , 1 );
		for( lst_node = get_child( lst_node ); lst_node ; lst_node = get_next( lst_node )){
			//printf( "1: %s %d %d %s\n" , lst_node->name , isspace( lst_node->value ) , strlen( lst_node->value ) , lst_node->value );
			if( ++count % 8 == 1 && ( filter->re_class || filter->re_method )){
				filter->p_next = calloc( sizeof( ifilter ) , 1 );
				filter = filter->p_next;
			} else {
				load_filter( filter , lst_node );
				if( DEBUG ) printf( "re_module: %08x re_lr: %08x\nre_class: %08x re_method: %08x\nre_r0: %08x re_r1: %08x\nre_r2: %08x re_r3: %08x\n" , (unsigned int) filter->re_module , (unsigned int) filter->re_lr , (unsigned int) filter->re_class , (unsigned int) filter->re_method , (unsigned int) filter->re_r0, (unsigned int) filter->re_r1, (unsigned int) filter->re_r2, (unsigned int) filter->re_r3 );
			}
		}
		if( !filter->re_class && !filter->re_method ){
			free( filter );
			return (ifilter*) 0;
		} else {
			return first;
		}
	} else {
		fprintf( stderr , 
			"[x] malformed plist file, this must be a filter node\n" 
		);
		return (ifilter*) -1;
	}
}

//messy mess, enjoy debugging
iconf* load_conf( iconf* opt,  char* fname ){
	FILE* fconf=0;
	int fsize = 0;
	char* fcontent = 0;
	xmlDoc *doc = NULL;
	one_node *root, *cur_node, *lst_node;

	if( !opt ){
		opt = calloc( sizeof( iconf) , 1 );
		init_default_conf( opt );
	}
	if( opt ){
#ifdef LIBXML_TREE_ENABLED
		fconf = fopen( fname , "r+" );
		if( fconf ){
			fseek( fconf , 0 , SEEK_END	);
			fsize = ftell( fconf );
			fseek( fconf , 0 , SEEK_SET );
			fcontent = calloc( 1 , fsize );
			fread( fcontent , 1 , fsize , fconf );
			fclose( fconf );
			//if( opt->chksm == 0 || opt->chksm != crc32( 0 , fcontent , fsize )){
			if( 1 ){
				//opt->chksm = crc32( 0 , fcontent , fsize );
		   		doc = xmlReadFile( fname , NULL , 0 );
				if( doc != NULL ){
					root = get_node( xmlDocGetRootElement(doc));
					for (cur_node = get_child( root ); cur_node; cur_node = get_next( cur_node )) {
						//printf( "node: %s (%d) %s\n" , cur_node->name , cur_node->type ,  xmlNodeGetContent (cur_node) );
						if( !xmlStrcasecmp( cur_node->name , OPT_ENABLED )){
							if( DEBUG ) printf( "%s %s\n" , cur_node->name , cur_node->value );
							if( !xmlStrcasecmp( cur_node->value , "true" )){
								opt->enabled = true;
							} else if( !xmlStrcasecmp( cur_node->value , "false" )){
								opt->enabled = false;
							} else {
								fprintf( stderr , 
									"[x] malformed plist file, enabled must contains true or false\n" 
								);
							}
						} else if( !xmlStrcasecmp( cur_node->name , OPT_HOT_RELOAD )){
							if( DEBUG ) printf( "%s %s\n" , cur_node->name , cur_node->value );
							if( !xmlStrcasecmp( cur_node->value , "true" )){
								opt->hot_reload = true;
							} else if( !xmlStrcasecmp( cur_node->value , "false" )){
								opt->hot_reload = false;
							} else {
								fprintf( stderr , 
									"[x] malformed plist file, hot_reload must contains true or false\n" 
								);
							}
						} else if( !xmlStrcasecmp( cur_node->name , OPT_CALLER)){
							if( DEBUG ) printf( "%s %s\n" , cur_node->name , cur_node->value );
							if( !xmlStrcasecmp( cur_node->value , MODULE_SELF )){
								uint32_t maxpathlen = MAXPATHLEN;
								char* path = calloc( 1 , maxpathlen );
								char* repath = calloc( 1 , maxpathlen + 2 );
								_NSGetExecutablePath( path , &maxpathlen );
								sprintf( repath , "^%s$" , path );
								opt->module_filter = load_regexp( repath );
							} else {
								opt->module_filter = load_regexp( cur_node->value );
							} 
						} else if( !xmlStrcasecmp( cur_node->name , OPT_MAX_DEPTH )){
							if( DEBUG ) printf( "%s %s\n" , cur_node->name , cur_node->value );
							int max_depth = atoi( cur_node->value );
							if( max_depth >= 0 && max_depth <= 65334 ){
								opt->max_depth = max_depth;
							} else {
								fprintf( stderr , 
									"[x] malformed plist file, max_depth must be a valid integer\n" 
								);
							}
						} else if( !xmlStrcasecmp( cur_node->name , OPT_WHITELIST )){
							if( DEBUG ) puts("whitelist processing..." );
							opt->whitelist = load_filter_list( get_child( cur_node ));
						} else if( !xmlStrcasecmp( cur_node->name , OPT_BLACKLIST )
								&& cur_node->children ){
							if( DEBUG ) puts("blacklist processing..." );
							opt->blacklist = load_filter_list( get_child( cur_node ));
						} else if( !xmlStrcasecmp( cur_node->name , OPT_OUTPUT )){
							if( !xmlStrcasecmp( cur_node->value , OUT_TYPE_FILE )){
								FILE* fout = fopen( cur_node->value , "wb" );
								if( fout ){
									opt->output_stream = fout;
									opt->output_type = OTYPE_FILE;
								} else {
									fprintf( stderr , 
										"[x] can not open output file '%s' for writing\n",
										fname
									);
								}
							} else if( !xmlStrcasecmp( cur_node->value , OUT_TYPE_NSLOG )){
								opt->output_type = OTYPE_NSLOG;
							} else {
								opt->output_type = OTYPE_STDOUT;
								opt->output_stream = stdout;
							}
						} else if( !xmlStrcasecmp( cur_node->name , OPT_OUT_FORMAT )){
							if( !xmlStrcasecmp( cur_node->value , OUT_FORMAT_XML )){
								opt->output_format = OFORMAT_XML;
							} else {
								opt->output_format = OFORMAT_TXT;
							}
						}else {
							if( DEBUG ){
								fprintf( stdout , "[!] unknown node option '%s'\n" , cur_node->name );
							}
						}
				   	}
					// free the document
					xmlFreeDoc(doc);
					xmlCleanupParser();
					return opt;
				} else {
					fprintf( stderr , "[x] failed to parse plist file\n" );
					return (iconf*) -4;
				}
			}
			//no error, just no modifications since last loading so no update :)
		} else {
			fprintf( stderr , "[x] failed to access conf file\n" );
			return (iconf*) -3;
		}
#endif
		return (iconf*) -2;
	} else {
		fprintf( stderr , "[x] can not allocate memory for options" );
		return (iconf*) -1;
	}
}