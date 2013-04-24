/*
based on the work of Stefan Esser on his dumpdecrypted.dylib stuff 
and libsubjc from kennytm that i couldn't build from source nor use properly (maybe my bad dude)
all credz to them srsly, they've done teh work. i've just added some bullshit.

usage(until fronted functionnal): 
root# DYLD_INSERT_LIBRARIES=itrace.dylib /Applications/Calculator.app/Calculator

this dylib just hook objc_msgSend thanks to MobileSubstrate/interpos (just uncomment some 
commented line to switch method) & print out the class and methods called, white/blacklisti

last rev: 2013/02/17
Sogeti ESEC - sqwall
*/
#define __GLOBALS__
#include "main.h"


//replace objc_msgSend by itrace
const struct {void* n;void* o;} interposers[] __attribute((section("__DATA,__interpose" ))) = {
	{ (void*) itrace , (void*) objc_msgSend },
};

/* hook callback, do whatever you want to 14 DWORD on teh stack before optionnals variadic arguments
 ***/
void hook_callback( id self , SEL op ){
	if( !opt /*|| opt->hot_reload */){
		opt = load_conf( opt , "itrace.plist" );
	}
	if( opt ){
		if( opt->enabled ){
			p_thread = get_thread( );
			p_context = get_last_trace( p_thread->p_context );
			if( p_context != 0 ){
				//retrieve class name
				p_classname = (char*) object_getClassName( self );
				if( !p_classname ){
					p_classname = "nill";
				}
				//printf( "%08x %s->%s , module_filter: %08x is_wl: %d is_bl: %d\n" , p_context->lr, p_classname, op, opt->module_filter , is_whitelisted( opt->whitelist , p_context, p_caller, p_classname , (char*) op ) , is_blacklisted( opt->blacklist , p_context, p_caller, p_classname , (char*) op));
				//handle module path
				if( opt->module_filter != 0 ) {
					show_trace = false;
					mod_info = calloc( 1 , sizeof( Dl_info ));
					dladdr( (void*) p_context->lr , mod_info );
					p_caller = (char*) mod_info->dli_fname;
					if( regexec( opt->module_filter , p_caller , 0, NULL, 0 )){
						show_trace = true;
					}
				} else {
					p_caller = "nill";
					show_trace = true;
				}
				if( show_trace ) output_trace_start( p_context , p_caller, p_classname , (char*) op );
			}	
		}
	}
}

//create objc_msgSend call's trace
__attribute__((naked))
uint32_t create_trace( ){
	//base lr is in r12 to keep r0 safe
	__asm__ __volatile__( "stmdb sp!, {r0-r12,lr}\n" 
		"push {r0-r11}\n"
		"push {r12}\n"
		"blx _add_context\n"
		"mov r12, r0\n"
		"add r12, #16\n"
		"pop {r0}\n"
		"str r0, [r12]!\n"
		"add r12, #4\n"
		"pop {r0-r11}\n"
	// r9 = p_context->lr
		"stmia r12!, {r0-r11}\n" //fufu
	//prologue
		"ldmia sp!, {r0-r12,lr}\n"
		//"mov pc, lr\n"
	);
	__asm__ __volatile__( "ldr r0, %0\n" : : "m" (original_msgSend) : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r10", "r11", "r12", "lr" );
}

/* main hook wrapper
 ***/
__attribute__((naked))
id itrace( id self, SEL op ){
	__asm__ __volatile__( 
		//prologue
		"stmdb sp!, {r0-r8,r10-r12,lr}\n"
		//create an api call trace
		"mov r12, lr\n"
		"bl _create_trace\n" //return original_msgSend in r0
		"mov r9, r0\n"
		"ldmia sp!, {r0-r8,r10-r12,lr}\n"
		//call hook callback
		"stmdb sp!, {r0-r12,lr}\n"
		"bl _hook_callback\n"
		"ldmia sp!, {r0-r12,lr}\n"
		//call original objc_msgSend
		//this call for interposer method
			"blx _objc_msgSend\n"
		//this call for mobilesubstrate method
			//"blx r9\n"
		//close the last api call's trace

		"stmdb sp!, {r0-r12}\n"
		"bl _close_trace\n" //return saved lr in r0
		"mov lr, r0\n"
		//epilogue
		"ldmia sp!, {r0-r12}\n"
		//return to callee
		"bx lr\n"
	);
}

//not ran by __interposer method
//mobilesubstrate proper stuff
__attribute__((constructor))
void init_hook(int argc, const char **argv ){	
	//is libobjc loaded ?
	time( &itrace_start );
	p_objc_msgSend = dlsym( RTLD_DEFAULT , "objc_msgSend" );
	if( !p_objc_msgSend ){
		puts( "[x] failed to find objc_msgSend address" );
		exit( 1 );
	} else {
		//load options
		opt = load_conf( opt , "itrace.plist" );
		//comment/uncomment next line for interposer/mobilesubstrate method
		//MSHookFunction( objc_msgSend , itrace , (void**) &original_msgSend );
		printf( "objc_msgSend function substrated from 0x%08x to 0x%08x with thread key %08x\n" , 
			(unsigned int) original_msgSend , 
			(unsigned int) itrace , 
			(unsigned int) tkey 
		);
	}
}

/* clean teh hook, and free per-thread allocated data 
 ***/
//mobilesubstrate proper stuff
__attribute__((destructor))
void clean_hook( ){
	if( !original_msgSend ){
		puts( "[x] no messy hook to clean up" );
	} else {
		clean_per_thread( );
		MSHookFunction( objc_msgSend , &original_msgSend , (void**) &original_msgSend );
		time( &itrace_end );
		printf( "itrace finished in %ld sec\n" , itrace_end - itrace_start );
		//puts( "hook & stack cleared, kthxbye" );
	}
}

/**************************************************************************************************\
|************************************* thread-specific stuff **************************************|
\**************************************************************************************************/
context* init_thread( ){
	context* p_contextfu;
	threadctx* p_thread = get_thread( );
	if( !p_thread) {
		p_thread = calloc( 1 , sizeof( threadctx ));
		p_thread->thread_id = pthread_self( );
		//create thread chained list
		if( !p_thread_base ){
			p_thread_base = p_thread;
		} else {
			threadctx* p_tmp_thread = p_thread_base;
			while( p_tmp_thread->p_next != 0 ){
				p_tmp_thread = (threadctx*) p_tmp_thread->p_next;
			}
			p_tmp_thread->p_next = (void*) p_thread;
		}
	} 
	p_contextfu = alloc_context( );
	p_thread->p_context = p_contextfu;
	return p_contextfu;
}

/* free the thread-specific buffer
 ***/
void clean_per_thread( ){
	threadctx* p_thread = get_thread( );
	if( p_thread > 0 ){
		context* p_context = p_thread->p_context;
		if( !p_context && DEBUG ){
			fprintf( stderr , 
				"[x] could not find allocated memory for this thread's data, thread key: %08x\n", 
				(unsigned int) tkey 
			);
		} else {
			fflush( opt->output_stream );
			if( opt->output_stream != stdout ){
				fclose( opt->output_stream );
			}
			clear_filter_list( opt->whitelist );
			clear_filter_list( opt->blacklist );
			context* tmp = 0;
			while( p_context->p_next > 0 ){
				p_context = tmp = p_context->p_next;
				free( tmp );
			}
		}
	}
}

/* allocate thread context, 8 byte aligned
 ***/
context* alloc_context( ){
	context* p_context = calloc( sizeof( context ) + 7 , 1 );
	memset( p_context , sizeof( p_context ) , 0 );
	if( !p_context ){
		if( DEBUG ){
			fprintf( stderr , 
				"[x] could not allocate memory for this thread's context, thread key: %08x\n", 
				(unsigned int) tkey 
			);
		}
		p_context = (context*) -1;
	} else { 
		if((align = (uint32_t) p_context % 8 )){
			p_context = (context*)((uint32_t) p_context + align);
		}
	}
	return p_context;
}

/* get current thread ptr from pthread_self
 ***/
threadctx* get_thread( ){
	threadctx* p_thread = p_thread_base;
	pthread_t tid = pthread_self( );
	while( p_thread > 0 && p_thread->p_next > 0 && p_thread->thread_id != tid ){
		p_thread = (threadctx*) p_thread->p_next;
	}
	if( p_thread > 0 && p_thread->thread_id == tid )
		return p_thread;
	else
		return (threadctx*) 0;
}

/* wrapper
 ***/
context* get_thread_context( ){
	threadctx* p_thread = get_thread( );
	if( p_thread > 0 ){
		return p_thread->p_context;
	} else {
		return (context*) 0;
	}
}

/* so, take a beer and enjoy useless and ugly code
 ***/
context* get_last_trace( context* p_context ){
	while( p_context > 0 && p_context->p_next > 0 ){
		//print_context( p_context );
		p_context = p_context->p_next;
	}
	//printf( "-> " );print_context( p_context );
	return p_context;
}

/* have a join please, relax
 ***/
context* get_last_unended_trace( context* p_context ){
	p_context = get_last_trace( p_context );
	while( p_context > 0 && p_context->p_prev > 0 ){
		if( p_context->returned == 0 ){
			break;
		} else {
			p_context = p_context->p_prev;
		}
	}
	return p_context;
}

/* close current objc_msgSend call's trace
 ***/
uint32_t close_trace( ){
	context* p_contextfu = get_last_unended_trace( get_thread_context( ));
	context* p_prevcontext = 0;
	context* p_nextcontext = 0;
	uint32_t dalr = 0;
	if( !p_contextfu ){
		fprintf( stderr , "[x] could not retrieve thread's context" );
	}
	if( p_contextfu > 0 ){
		p_contextfu->returned = 1;
		time( &p_contextfu->end_time );
		dalr = p_contextfu->lr;
		if( p_contextfu->p_prev > 0 ){
			p_prevcontext = p_contextfu->p_prev;
			if( p_contextfu->p_next > 0 ){
				p_prevcontext->p_next = p_contextfu->p_next;
				p_nextcontext = p_contextfu->p_next;
				p_nextcontext->p_prev = p_prevcontext;
			} else {
				p_prevcontext->p_next = 0;
			}
			
		} else {
			threadctx* p_thread = get_thread( );
			if( p_contextfu->p_next > 0 ){
				p_nextcontext = p_contextfu->p_next;
				p_nextcontext->p_prev = 0;
				p_thread->p_context = p_nextcontext;
			} else {
				p_thread->p_context = 0;
			}
		}
		//if( show_trace ) output_trace_end( p_contextfu );
		free( p_contextfu );
		show_trace = false;
		cur_depth--;
		return dalr;
	} else {
		printf( "failed retreiving trace\n" );
		return -1;
	}
}

/* add a new context and return handle in r0
 ***/
uint32_t add_context( ){
	context* p_contextfu, *p_prevcontext = 0;
	cur_depth++;
	threadctx* p_thread = get_thread( );
	if( !p_thread ){
		p_contextfu = init_thread( );
	} else {
		p_contextfu = p_thread->p_context;
	}
	if( !p_contextfu ){
		printf( "[!] error retrieving thread context, try switching thread\n" );
		p_contextfu = init_thread( );
	} else {	
		p_contextfu = get_last_trace( p_contextfu );
		p_contextfu->p_next = alloc_context( );
		p_prevcontext = p_contextfu;
		p_contextfu = p_contextfu->p_next;
		p_contextfu->p_prev = p_prevcontext;
		if( p_contextfu <= 0 ){
			p_contextfu = (context*) -1;
		}
	}	if( p_contextfu > 0 ){
		time( &p_contextfu->start_time );
		p_contextfu->depth = cur_depth;
		p_contextfu->returned = 0;
	}
	return (uint32_t) p_contextfu;
}

/**************************************************************************************************\
|****************************************** output stuff ******************************************|
\**************************************************************************************************/

/* output specific function, formating's there, maybe it's more a 'utils' thing but whatever
 ***/
void itrace_output( char* output , ... ){
	if( strlen( output )){
		va_list vl;
		va_start( vl , output );
		switch( opt->output_type ){
			default:
			case OTYPE_STDOUT:
				printf( output , vl );
				break;
			case OTYPE_NSLOG:
				NSLog( output , vl );
				break;
			case OTYPE_FILE:
				if( opt->output_stream ) fprintf( opt->output_stream , output , vl );
				break;
		}
		va_end( vl );
	} else {
		fprintf( stderr , "[x] empty string ? srsly ?\n" );
	}
}

/* output a trace's start
 ***/
void output_trace_start( context* p_contextfu , char* p_caller, char* p_classname , char* p_method ){
	char *msg = 0;
	char* padding = 0;
	switch( opt->output_format ){
		default:
		case OFORMAT_TXT:
			padding = set_padding( PADDING_TXT , p_contextfu->depth );
			printf( TXT_FORMAT_START , p_contextfu->lr , padding, p_classname , p_method);
			break;
		case OFORMAT_XML:
			padding = set_padding( PADDING_XML , p_contextfu->depth );
			printf( XML_FORMAT_START , padding , 
				padding , p_caller ,
				padding , p_classname ,
				padding , p_method ,
				padding , (int) p_contextfu->depth ,
				padding , p_contextfu->lr ,
				padding , p_contextfu->r0 ,
				padding , p_contextfu->r1 ,
				padding , p_contextfu->r2 ,
				padding , p_contextfu->r3 ,
				padding , p_contextfu->r4 ,
				padding , p_contextfu->r5 ,
				padding , p_contextfu->r6 ,
				padding , p_contextfu->r7 ,
				padding , p_contextfu->r8 ,
				padding , p_contextfu->r9 ,
				padding , p_contextfu->r10 ,
				padding , p_contextfu->r11 ,
				padding , p_contextfu->start_time
			);
			break;
	}
}

/* output a trace's end
 ***/
void output_trace_end( context* p_contextfu ){
	char *msg = 0;
	char* padding = 0;
	switch( opt->output_format ){
		default:
		case OFORMAT_TXT:
			padding = set_padding( PADDING_TXT , p_contextfu->depth );
			msg = calloc( 1 , strlen( TXT_FORMAT_END ) + strlen( padding ) + 4);
			snprintf( msg , TXT_FORMAT_END , p_contextfu->lr , padding , p_contextfu->end_time - p_contextfu->start_time );
			break;
		case OFORMAT_XML:
			padding = set_padding( PADDING_XML , p_contextfu->depth );
			msg = calloc( 1 , strlen( XML_FORMAT_END ) + strlen( padding ) * 2  + 21*1);
			snprintf( msg , XML_FORMAT_START , 
				padding , p_contextfu->end_time , 
				padding 
			);
			break;
	}
	itrace_output( msg );
}