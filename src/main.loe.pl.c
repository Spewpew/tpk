#include "config.h"
#include <SDL.h>
#include <SDL_image.h>

#define TOSTR2(x) #x
#define TOSTR(x) TOSTR2(x)
#define elog(fmt,args...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,\
SDL_FILE"["TOSTR(SDL_LINE)"]:%s->"fmt,SDL_FUNCTION,##args)
#define wlog(fmt,args...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,\
SDL_FILE"["TOSTR(SDL_LINE)"]:%s->"fmt,SDL_FUNCTION,##args)
#define ilog(fmt,args...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,\
SDL_FILE"["TOSTR(SDL_LINE)"]:%s->"fmt,SDL_FUNCTION,##args)
#define clog(fmt,args...) SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,\
SDL_FILE"["TOSTR(SDL_LINE)"]:%s->"fmt,SDL_FUNCTION,##args)
#define esdl elog("SDL_GetError(): \"%s\".",SDL_GetError())
#define eimg elog("IMG_GetError(): \"%s\".",IMG_GetError())
#define typemax(t) ((t)((t)-1 < 0 ? ((t)1 << (sizeof (t) * 8 - 2)) - 1 + ((t)1 << (sizeof (t) * 8 - 2)) : -1))
#define critmalloc(sz) clog("Failed to allocate memory(%zu).",sz)
#define critrealloc(sz) clog("Failed to re-allocate memory(%zu).",sz)

loe::replace({SET_INI_INT64},{Sint64});
loe::replace({SET_INI_UINT64},{Uint64});
loe::replace({SET_INI_ASSERT},{SDL_assert});
loe::replace({SET_INI_BOOLEAN},{int});
loe::replace({SET_INI_FALSE},{0});
loe::replace({SET_INI_TRUE},{1});

set::ini_works;

static SET_INI_BOOLEAN
	opt_syntax_error=SET_INI_FALSE,opt_show_help=SET_INI_FALSE,
	opt_show_version=SET_INI_FALSE,opt_log_loaded_images_and_sizes=SET_INI_FALSE;

static Uint32
	maxwidth=1024;

static struct blob{
	size_t size;
	char data[];
} *outimagepath=NULL,*outinifilepath=NULL;

enum blob_alloc_result{
	blob_alloc_ok,blob_alloc_overflow_error,blob_alloc_critical_malloc_error
};

static enum blob_alloc_result blob_alloc(struct blob**ppb,size_t sz){
	size_t rsz;
	if(__builtin_add_overflow(sz,sizeof(struct blob),&rsz)){
		elog("Data size is too large(%zu). The maximum data size is %zu bytes",sz,typemax(size_t)-sizeof(struct blob));
		return blob_alloc_overflow_error;
	}
	struct blob*pb=SDL_malloc(rsz);
	if(!pb){
		critmalloc(rsz);
		return blob_alloc_critical_malloc_error;
	}
	pb->size=sz;
	*ppb=pb;
	return blob_alloc_ok;
}

static enum blob_alloc_result blob_alloc_cstr(struct blob**ppb,size_t sz){
	size_t rsz;
	if(__builtin_add_overflow(sz,sizeof(struct blob)+1,&rsz)){
		elog("Data size is too large(%zu). The maximum data size is %zu bytes",sz,typemax(size_t)-sizeof(struct blob)-1);
		return blob_alloc_overflow_error;
	}
	enum blob_alloc_result e=blob_alloc(ppb,sz+1);
	if(e==blob_alloc_ok)
		(*ppb)->data[sz]='\0';
	return e;
}

static enum blob_alloc_result blob_dup(struct blob**b,const char*ndat,size_t sz){
	if(*b){
		if((*b)->size>=sz){
			SDL_memcpy((*b)->data,ndat,sz);
			(*b)->size=sz;
			return blob_alloc_ok;
		}
		struct blob*pb;
		switch(blob_alloc(&pb,sz)){
			case blob_alloc_ok:{
				SDL_free(*b);
				SDL_memcpy(pb->data,ndat,sz);
				*b=pb;
				return blob_alloc_ok;
			}
			case blob_alloc_overflow_error:
				return blob_alloc_overflow_error;
			case blob_alloc_critical_malloc_error:
				return blob_alloc_critical_malloc_error;
		}
	}else{
		struct blob*pb;
			switch(blob_alloc(&pb,sz)){
				case blob_alloc_ok:{
					SDL_memcpy(pb->data,ndat,sz);
					*b=pb;
					return blob_alloc_ok;
				}
				case blob_alloc_overflow_error:
					return blob_alloc_overflow_error;
				case blob_alloc_critical_malloc_error:
					return blob_alloc_critical_malloc_error;
			}
	}
}

static enum blob_alloc_result blob_dup_text_to_cstr(struct blob**b,const char*ndat,size_t sz){
	if(*b){
		if((*b)->size>sz){
			SDL_memcpy((*b)->data,ndat,sz);
			(*b)->size=sz+1;
			(*b)->data[sz]='\0';
			return blob_alloc_ok;
		}
		struct blob*pb;
		switch(blob_alloc_cstr(&pb,sz)){
			case blob_alloc_ok:{
				SDL_free(*b);
				SDL_memcpy(pb->data,ndat,sz);
				//~ pb->data[sz]='\0';
				*b=pb;
				return blob_alloc_ok;
			}
			case blob_alloc_overflow_error:
				return blob_alloc_overflow_error;
			case blob_alloc_critical_malloc_error:
				return blob_alloc_critical_malloc_error;
		}
	}else{
		struct blob*pb;
			switch(blob_alloc_cstr(&pb,sz)){
				case blob_alloc_ok:{
					SDL_memcpy(pb->data,ndat,sz);
					//~ pb->data[sz]='\0';
					*b=pb;
					return blob_alloc_ok;
				}
				case blob_alloc_overflow_error:
					return blob_alloc_overflow_error;
				case blob_alloc_critical_malloc_error:
					return blob_alloc_critical_malloc_error;
			}
	}
}

//~ static enum blob_alloc_result blob_dup_text_to_cstr(struct blob**b,const char*str,size_t sz){
	//~ size_t rsz;
	//~ if(__builtin_add_overflow(sz+sizeof(**b)+1,1,&rsz)){
		//~ elog("Data size is too large(%zu). The maximum data size is %zu bytes",sz,typemax(size_t)-1-sizeof(**b));
		//~ return blob_alloc_overflow_error;
	//~ }
	//~ enum blob_alloc_result e=blob_dup(b,str,sz+1);
	//~ if(e==blob_alloc_ok)
		//~ (*b)->data[sz]='\0';
	//~ return e;
//~ }

static Uint32 addrol13(const char*data,size_t sz){
	register Uint32 hash=0;
	for(register size_t l=0;l<sz;++l){
		hash=((hash<<13)|(hash>>19))+(unsigned char)data[l];
		//~ elog("l=%zu,%"PRIu32,l,hash);
	}
	return hash;
}

loe::replace({LOE_STACK_MALLOC},{SDL_malloc});
loe::replace({LOE_STACK_REALLOC},{SDL_realloc});
loe::replace({LOE_STACK_FREE},{SDL_free});
loe::replace({LOE_STACK_LOG_CRITICAL_MALLOC_ERROR},{critmalloc(rsz)});
loe::replace({LOE_STACK_LOG_CRITICAL_REALLOC_ERROR},{critrealloc(rsz)});
loe::replace({LOE_STACK_LOG_OVERFLOW_ERROR},{clog("Overflow.")});
loe::replace({LOE_STACK_ASSERT},{SDL_assert});

loe::stack(files){
	::index_type{Uint32};
	::length{n_records};
	::queue{q_records};
	::array{{struct files_record}{records}};
	::allstatic;
	Uint32 n_records,q_records,n_loaded_surfaces;
	struct files_record{
		Uint32 file_id_hash;
		struct blob*file_id,*file_path;
		Uint32 dst_x;
		Uint32 ladder_level;
		SDL_Surface*surface;
	} records[];
}

enum files_join_record_result{
	files_join_record_ok,files_join_record_overflow_error,
	files_join_record_overriden,files_join_record_exist,
	files_join_record_critical_malloc_error,
	files_join_record_critical_realloc_error
};

static enum files_join_record_result files_join_record(struct files**ppool,
const char*id,size_t idsz,struct blob*path){
	Uint32 hash=addrol13(id,idsz);
	for(typeof(ppool[0]->n_records) l=0;l<ppool[0]->n_records;++l){
		if(hash==ppool[0]->records[l].file_id_hash && idsz==ppool[0]->records[l].file_id->size-1 &&
			SDL_memcmp(id,ppool[0]->records[l].file_id->data,idsz)==0){
			if(ppool[0]->records[l].file_path->size==path->size &&
				SDL_memcmp(ppool[0]->records[l].file_path->data,path->data,path->size)==0){
				wlog("Repeated declaration '[%.*s] %.*s'.",(int)idsz,id,(int)path->size-1,path->data);
				return files_join_record_exist;
			}
			wlog("'%.*s' is overridden, it was '%.*s' became '%.*s'.",(int)idsz,id,(int)ppool[0]->records[l].file_path->size-1,ppool[0]->records[l].file_path->data,(int)path->size-1,path->data);
			SDL_free(ppool[0]->records[l].file_path);
			ppool[0]->records[l].file_path=path;
			return files_join_record_overriden;
		}
	}
	struct blob*pb=NULL;
	switch(blob_dup_text_to_cstr(&pb,id,idsz)){
		case blob_alloc_ok:{
			switch(files_push(ppool,(struct files_record){
				.file_id_hash=hash,
				.file_id=pb,
				.file_path=path
			}
			)){
				case files_occupy_ok:
					return files_join_record_ok;
				case files_occupy_overflow_error:{
					elog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)path->size-1,path->data);
					return files_join_record_overflow_error;
				}
				case files_occupy_critical_realloc_error:{
					clog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)path->size-1,path->data);
					return files_join_record_critical_realloc_error;
				}
			}
		}
		case blob_alloc_overflow_error:{
			elog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)path->size-1,path->data);
			return files_join_record_overflow_error;
		}
		case blob_alloc_critical_malloc_error:{
			clog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)path->size-1,path->data);
			return files_join_record_critical_malloc_error;
		}
	}
}

static enum files_join_record_result files_join_record2(struct files**ppool,
const char*id,size_t idsz,const char*path,size_t pathsz){
	Uint32 hash=addrol13(id,idsz);
	for(typeof(ppool[0]->n_records) l=0;l<ppool[0]->n_records;++l){
		//~ ilog("[%.*s] ,path=%.*s,hash=%"PRIu32"=?%"PRIu32",%zu=?%zu",(int)idsz,id,(int)pathsz,path,hash,ppool[0]->records[l].file_id_hash,idsz,ppool[0]->records[l].file_id->size-1);
		if(hash==ppool[0]->records[l].file_id_hash && idsz==ppool[0]->records[l].file_id->size-1 &&
			SDL_memcmp(id,ppool[0]->records[l].file_id->data,idsz)==0){
			if(ppool[0]->records[l].file_path->size-1==pathsz &&
				SDL_memcmp(ppool[0]->records[l].file_path->data,path,pathsz-1)==0){
				wlog("Repeated declaration '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
				return files_join_record_exist;
			}
			wlog("Trying to change '%.*s' from '%.*s' to '%.*s'.",(int)idsz,id,(int)ppool[0]->records[l].file_path->size-1,ppool[0]->records[l].file_path->data,(int)pathsz,path);
			switch(blob_dup_text_to_cstr(&ppool[0]->records[l].file_path,path,pathsz)){
				case blob_alloc_ok:{
					return files_join_record_overriden;
				}
				case blob_alloc_overflow_error:{
					elog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
					return files_join_record_overflow_error;
				}
				case blob_alloc_critical_malloc_error:{
					clog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
					return files_join_record_critical_malloc_error;
				}
			}
		}
	}
	//~ ilog("Hit,%zu",idsz);
	struct blob*ppath=NULL;
	switch(blob_dup_text_to_cstr(&ppath,path,pathsz)){
		case blob_alloc_ok:{
			struct blob*pb=NULL;
			switch(blob_dup_text_to_cstr(&pb,id,idsz)){
				case blob_alloc_ok:{
					switch(files_push(ppool,(struct files_record){
						.file_id_hash=hash,
						.file_id=pb,
						.file_path=ppath
					}
					)){
						case files_occupy_ok:
							return files_join_record_ok;
						case files_occupy_overflow_error:{
							SDL_free(ppath);
							elog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
							return files_join_record_overflow_error;
						}
						case files_occupy_critical_realloc_error:{
							SDL_free(ppath);
							clog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
							return files_join_record_critical_realloc_error;
						}
					}
				}
				case blob_alloc_overflow_error:{
					SDL_free(ppath);
					elog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
					return files_join_record_overflow_error;
				}
				case blob_alloc_critical_malloc_error:{
					SDL_free(ppath);
					clog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
					return files_join_record_critical_malloc_error;
				}
			}
		}
		case blob_alloc_overflow_error:{
			elog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
			return files_join_record_overflow_error;
		}
		case blob_alloc_critical_malloc_error:{
			clog("While adding a file record '[%.*s] %.*s'.",(int)idsz,id,(int)pathsz,path);
			return files_join_record_critical_malloc_error;
		}
	}
}

static SET_INI_BOOLEAN opt_report_key(const struct SET_INI_GROUP*g,
const char*gn,size_t gz,const char*kn,size_t kz,const char*v,size_t vz,
SET_INI_INT64 i,enum SET_INI_TYPE t,const void*udata){
	SET_INI_BOOLEAN e=SET_INI_FALSE;
	if(g){
		elog("Unknown option '%.*s'.",(int)kz,kn);
		opt_syntax_error=SET_INI_TRUE;
	}else{
		if(t==SET_INI_TYPE_BOOLEAN){
			switch(files_join_record2((struct files**)udata,gn,gz,kn,kz)){
				case files_join_record_ok:
				case files_join_record_overriden:
				case files_join_record_exist:{
					e=SET_INI_TRUE;
					break;
				}
				default:{
					break;
				}
			}
		}else{
			struct blob*pb=NULL;
			switch(blob_alloc_cstr(&pb,kz+vz+1)){
				case blob_alloc_ok:{
					SDL_memcpy(pb->data,kn,kz);
					SDL_memcpy(pb->data+kz+1,v,vz);
					//~ pb->data[vz+kz+1]='\0';
					pb->data[kz]='=';
					switch(files_join_record((struct files**)udata,gn,gz,pb)){
						case files_join_record_exist:
							SDL_free(pb);
						case files_join_record_ok:
						case files_join_record_overriden:{
							e=SET_INI_TRUE;
							break;
						}
						default:{
							SDL_free(pb);
							break;
						}
					}
					break;
				}
				case blob_alloc_overflow_error:{
					elog("Just in case(%zu).",kz+vz);
					break;
				}
				case blob_alloc_critical_malloc_error:{
					clog("While scanning parameters.");
					break;
				}
			}
		}
	}
	return e;
}

static SET_INI_BOOLEAN opt_report_group(const char*gn,size_t gz,
const void*udata){
	return SET_INI_TRUE;
}

static const struct opt_group*opt_in_word_set(register const char*,register size_t);

#define memcmp SDL_memcmp

set::ini_info(opt){
	empty{
		names ""
		keys{
			setbool{
				names "--help" "h" "--version" "v" "--log-loaded" "l"
				decl "SET_INI_BOOLEAN*pbool;"
				atts ".setbool={&opt_show_help}" ".setbool={&opt_show_help}"
					".setbool={&opt_show_version}" ".setbool={&opt_show_version}"
					".setbool={&opt_log_loaded_images_and_sizes}" ".setbool={&opt_log_loaded_images_and_sizes}"
				onload{
					if(t==SET_INI_TYPE_BOOLEAN){
						k->setbool.pbool[0]=SET_INI_TRUE;
					}else{
						elog("'%.*s': The option does not accept values.",(int)kz,kn);
						opt_syntax_error=SET_INI_TRUE;
					}
					return SET_INI_TRUE;
				}
			}
			setstring{
				names "--out-image" "o" "--out-ini" "i"
				decl "struct blob**pblob;"
				atts ".setstring={&outimagepath}" ".setstring={&outimagepath}"
					".setstring={&outinifilepath}" ".setstring={&outinifilepath}"
				onload{
					if(t!=SET_INI_TYPE_BOOLEAN && vz>0){
						if(blob_dup_text_to_cstr(k->setstring.pblob,v,vz)!=blob_alloc_ok)
							return SET_INI_FALSE;
					}else{
						elog("'%.*s': Option requires a non-empty value.",(int)kz,kn);
						opt_syntax_error=SET_INI_TRUE;
					}
					return SET_INI_TRUE;
				}
			}
			setmaxwidth{
				names "--max-width" "w"
				onload{
					if(t==SET_INI_TYPE_SINT64){
						if(i>0 && ((SET_INI_UINT64)i)<=typemax(typeof(maxwidth))){
							maxwidth=i;
						}else{
							elog("'%.*s=%"SDL_PRIs64"'; The specified value is out of range [1..%"PRIu32"]",(int)kz,kn,i,typemax(Uint32));
							opt_syntax_error=SET_INI_TRUE;
						}
					}else{
						elog("'%.*s=%.*s'; Invalid type of the specified value.",(int)kz,kn,(int)vz,v);
						opt_syntax_error=SET_INI_TRUE;
					}
					return SET_INI_TRUE;
				}
			}
			inisettings{
				names "--in-conf" "c"
				onload{
					SET_INI_BOOLEAN e=SET_INI_FALSE;
					if(t!=SET_INI_TYPE_BOOLEAN){
						struct blob*pb=NULL;
						if(blob_dup_text_to_cstr(&pb,v,vz)==blob_alloc_ok){
							pb->data[vz]='\0';
							SDL_RWops *rw=SDL_RWFromFile(pb->data,"rb");
							if(!rw){
								SDL_free(pb);
								esdl;
							}else{
								SDL_free(pb);
								Sint64 sz=SDL_RWsize(rw);
								if(sz<0){
									elog("Failed to get the size of '%.*s' file. SDL_GetError(): '%s'.",(int)vz,v,SDL_GetError());
								}else{
									struct blob*pini;
									switch(blob_alloc_cstr(&pini,sz)){
										case blob_alloc_ok:{
											if(SDL_RWread(rw,pini->data,sz,1)!=1){
												elog("Failed to read file '%.*s'. SDL_GetError(): '%s'.",(int)vz,v,SDL_GetError());
											}else{
												pini->data[sz]='\0';
												switch(set_ini_parse_string((SET_INI_GROUP_IN_WORD_SET)opt_in_word_set,
													pini->data,opt_report_group,opt_report_key,udata)){
													case SET_INI_PARSER_OK:{
														e=SET_INI_TRUE;
														break;
													}
													case SET_INI_PARSER_UTF8_ERROR:{
														elog("While parsing the '%.*s' file, a UTF-8 encoding error was detected.",(int)vz,v);
														break;
													}
													case SET_INI_PARSER_CANCELLED:{
														clog("While scanning parameters from the file '%.*s'.",(int)vz,v);
														break;
													}
												}
											}
											SDL_free(pini);
											break;
										}
										case blob_alloc_overflow_error:{
											elog("File '%.*s' is too large(%"SDL_PRIs64"). The maximum file size allowed is %zu bytes.",(int)vz,v,sz,typemax(size_t)-sizeof(*pb)-1);
											break;
										}
										case blob_alloc_critical_malloc_error:{
											break;
										}
									}
								}
								SDL_RWclose(rw);
							}
						}
					}else{
						elog("The parameter '%.*s' is specified without a value.",(int)kz,kn);
						opt_syntax_error=SET_INI_TRUE;
					}
					return e;
				}
			}
		}
	}
}

static int images_load_and_sort_by_height(struct files*pool){
	typeof(pool->n_records) l=pool->n_loaded_surfaces=0;
	do{
		SDL_Surface*sur=IMG_Load(pool->records[l].file_path->data);
		if(!sur){
			eimg;
			return 0;
		}
		if(opt_log_loaded_images_and_sizes==SET_INI_TRUE){
			ilog("[%s] '%s' %ix%i.",pool->records[l].file_id->data,pool->records[l].file_path->data,sur->w,sur->h);
		}
		pool->records[l].surface=sur;
		pool->n_loaded_surfaces++;
		if(sur->w>maxwidth){
			elog("The width of %ipx of the '%s' image is larger than the maximum allowable width of %"PRIu32"px.",sur->w,pool->records[l].file_path->data,maxwidth);
			return 0;
		}
		if(sur->w<=0){
			elog("The width of the image '%s'(%ipx) is less than or equal to zero.",pool->records[l].file_path->data,sur->w);
			return 0;
		}
		if(sur->h<=0){
			elog("The height of the image '%s'(%ipx) is less than or equal to zero.",pool->records[l].file_path->data,sur->h);
			return 0;
		}
		for(typeof(l) x=0;x<l;++x){
			if(sur->h>pool->records[x].surface->h){
				struct files_record r=pool->records[l];
				pool->records[l]=pool->records[x];
				pool->records[x]=r;
				break;
			}
		}
	}while(++l<pool->n_records);
	return 1;
}

loe::stack(levels){
	::index_type{typeof(((struct files*)0)->n_records)};
	::length{n_records};
	::queue{n_queue};
	::array{{struct levels_record}{records}};
	::allstatic;
	typeof(((struct files*)0)->n_records) n_records,n_queue;
	struct levels_record{
		typeof(maxwidth) end_x,end_y,floor_y;
	}records[];
}

loe::stack(string){
	::index_type{size_t};
	::length{size};
	::queue{queue};
	::array{{char}{data}};
	::allstatic;
	size_t size,queue;
	char data[];
}

enum string_push_text_result{
	string_push_text_ok,string_push_text_overflow_error,
	string_push_text_critical_realloc_error,
	string_push_text_too_large_error
};

static enum string_push_text_result string_push_text(struct string**s,
const char*text,size_t size){
	size_t rsz;
	if(__builtin_add_overflow(size,1,&rsz)){
		elog("While increasing the string '%s'. The maximum allowed text size is %zu bytes.",s[0]->data,typemax(typeof(size))-1);
		return string_push_text_too_large_error;
	}
	typeof((*s)->size) l=s[0]->size-1;
	switch(string_occupy(s,rsz)){
		case string_occupy_ok:{
			SDL_memcpy(&(s[0]->data[l]),text,size);
			s[0]->data[l+size]='\0';
			return string_push_text_ok;
		}
		case string_occupy_overflow_error:{
			elog("The string '%s' can not be increased anymore.",s[0]->data);
			return string_push_text_overflow_error;
		}
		case string_occupy_critical_realloc_error:{
			clog("While increasing the string '%s'.",s[0]->data);
			return string_push_text_critical_realloc_error;
		}
	}
}

enum string_push_snprintf_result{
	string_push_snprintf_ok,string_push_snprintf_overflow_error,
	string_push_snprintf_critical_realloc_error,string_push_snprintf_output_error
};

static enum string_push_snprintf_result string_push_snprintf(struct string**s,
const char*fmt,...){
	va_list list;
	va_start(list,fmt);
	int len=SDL_vsnprintf(NULL,0,fmt,list);
	if(len<0){
		va_end(list);
		elog("An output error is encountered.");
		return string_push_snprintf_output_error;
	}
	va_end(list);
	typeof(s[0]->size) l=s[0]->size-1;
	switch(string_occupy(s,len)){
		case string_occupy_ok:{
			va_start(list,fmt);
			SDL_vsnprintf(&(s[0]->data[l]),len+1,fmt,list);
			va_end(list);
			return string_push_snprintf_ok;
		}
		case string_occupy_overflow_error:{
			elog("The string '%s' can not be increased anymore(fmt='%s').",s[0]->data,fmt);
			return string_push_snprintf_overflow_error;
		}
		case string_occupy_critical_realloc_error:{
			clog("While increasing the string '%s'(fmt='%s').",s[0]->data,fmt);
			return string_push_snprintf_critical_realloc_error;
		}
	}
}

static int first_fit_level(struct files*pool,SDL_Surface**pt,
struct string**ps){
	struct levels*ladder=levels_new(127);
	if(!ladder)
		return 0;
	struct string*s=string_new(127);
	if(!s){
		elog("While creating a texture and a string with data for the INI file.");
		levels_free(ladder);
		return 0;
	}
	string_push(&s,'\0');
	typeof(maxwidth) maxw=0;
	typeof(pool->n_records) l=0;
	typeof(ladder->n_records) i=0;
	while(1){
		switch(levels_push(&ladder,(typeof(struct levels_record)){
			.end_x=pool->records[l].surface->w,
			.end_y=pool->records[l].surface->h
		})){
			case levels_occupy_ok:{
				break;
			}
			case levels_occupy_critical_realloc_error:{
				string_free(s);
				levels_free(ladder);
				clog("Could not re-allocate memory to store a new level.");
				return 0;
			}
			case levels_occupy_overflow_error:{
				string_free(s);
				levels_free(ladder);
				clog("Overflow.");
				return 0;
			}
		}
		pool->records[l].ladder_level=i;
		pool->records[l].dst_x=0;
		if(maxw<pool->records[l].surface->w)
			maxw=pool->records[l].surface->w;
		if(++l==pool->n_records)
			break;
		i=0;
		do{
l_con:;		typeof(maxwidth) ex;
			if(!__builtin_add_overflow(ladder->records[i].end_x,pool->records[l].surface->w,&ex) &&
				ex<=maxwidth){
				pool->records[l].ladder_level=i;
				pool->records[l].dst_x=ladder->records[i].end_x;
				if(maxw<(ladder->records[i].end_x=ex))
					maxw=ex;
				if(pool->records[l].surface->h>ladder->records[i].end_y)
					ladder->records[i].end_y=pool->records[l].surface->h;
				if(++l==pool->n_records)
					goto l_break;
				i=0;
				goto l_con;
			}
		}while(++i<ladder->n_records);
	}
l_break:i=0;
	typeof(maxwidth) floor_y=0;
	do{
		ladder->records[i].floor_y=floor_y;
		if(__builtin_add_overflow(floor_y,ladder->records[i].end_y,&floor_y)){
			elog("Failed to fit the images into a %"PRIu32"-by-%"PRIu32" zone.",maxwidth,typemax(typeof(maxwidth)));
			string_free(s);
			levels_free(ladder);
			return 0;
		}
	}while(++i<ladder->n_records);
	SDL_Surface*sur=SDL_CreateRGBSurface(0,maxw,floor_y,32,0xff,0xff00,
		0xff0000,0xff000000);
	if(!sur){
		esdl;
		string_free(s);
		levels_free(ladder);
		return 0;
	}
	l=0;
	do{
		SDL_Rect rc={
			.x=pool->records[l].dst_x,
			.y=ladder->records[pool->records[l].ladder_level].floor_y,
			.w=pool->records[l].surface->w,
			.h=pool->records[l].surface->h
		};
		if(SDL_BlitSurface(pool->records[l].surface,NULL,sur,&rc)<0){
			esdl;
			string_free(s);
			levels_free(ladder);
			return 0;
		}
		switch(string_push_snprintf(&s,"[%s]\nx=%i\ny=%i\nw=%i\nh=%i\n",
			pool->records[l].file_id->data,rc.x,rc.y,rc.w,rc.h)){
			case string_push_snprintf_ok:
				break;
			default:{
				elog("While creating a texture and a string with data for the INI file.");
				string_free(s);
				levels_free(ladder);
				return 0;
			}
		}
	}while(++l<pool->n_records);
	levels_free(ladder);
	ps[0]=s;
	pt[0]=sur;
	return 1;
}

int main(int i,char**v){
	int exit_code=1;
	if(SDL_Init(0)<0){
		esdl;
	}else{
		struct files*pool=files_new(127);
		switch(set_ini_parse_cmd((SET_INI_GROUP_IN_WORD_SET)opt_in_word_set,
		i-1,(const char*const*)v+1,opt_report_group,opt_report_key,&pool)){
			case SET_INI_PARSER_OK:{
				if(opt_show_help || opt_show_version){
					if(opt_show_version==SET_INI_TRUE){
						ilog(PACKAGE_VERSION);
					}
					if(opt_show_help==SET_INI_TRUE){
						ilog("Usage:$ %s options [imageid1] filepath1 [imageidN] filepathN [] options\nExample: %s --max-width=512 w=1024 --log-loaded l --out-image=out.png o=overout.png --out-ini=out.ini i=over.ini [hello_sprite_id_1] hellosprite.png overridehello.png [hello_sprite_id_2] hisprite.png [] w=2048 --in-conf=read_as_cmd_params.ini\n+---------------+-------+---------------+-------------+\n|  The long     |  The  |               |             |\n|  options      | short |  Type/Range   |Description  |\n|               |options|               |             |\n+---------------+-------+---------------+-------------+\n|    --help     |   h   |    Boolean    | Show this   |\n|               |       |               |   help.     |\n+---------------+-------+---------------+-------------+\n|   --version   |   v   |    Boolean    |Show version |\n+---------------+-------+---------------+-------------+\n|               |       |               |   Log the   |\n| --log-loaded  |   l   |    Boolean    |loaded images|\n|               |       |               |  and their  |\n|               |       |               |   sizes.    |\n+---------------+-------+---------------+-------------+\n|               |       |               | The path to |\n|  --out-image  |   o   |    String     |  save the   |\n|               |       |               |  resulting  |\n|               |       |               |   image.    |\n+---------------+-------+---------------+-------------+\n|               |       |               |The path to  |\n|   --out-ini   |   i   |    String     |  save the   |\n|               |       |               | resulting   |\n|               |       |               | INI file.   |\n+---------------+-------+---------------+-------------+\n|               |       |               |The path of  |\n|   --in-conf   |   c   |    String     |the INI      |\n|               |       |               |configuration|\n|               |       |               |file.        |\n+---------------+-------+---------------+-------------+\n|               |       |               |Maximum      |\n|               |       |   Uint32 /    |allowable    |\n|  --max-width  |   w   |[1..4294967295]|width of the |\n|               |       |               |resulting    |\n|               |       |               |image.       |\n+---------------+-------+---------------+-------------+",v[0],v[0]);
					}
					if(pool->n_records>0){
						wlog("The specified file entries will be ignored.");
					}
				}else if(opt_syntax_error!=SET_INI_TRUE){
					if(pool->n_records>0){
						if(outimagepath){
							if(outinifilepath){
								if(!IMG_Init(-1)){
									elog("No supported image formats or SDL_image library error: '%s'.",IMG_GetError());
								}else{
									if(images_load_and_sort_by_height(pool)){
										SDL_Surface*sur;
										struct string*inistring;
										if(first_fit_level(pool,&sur,&inistring)){
											if(IMG_SavePNG(sur,outimagepath->data)<0){
												eimg;
											}else{
												SDL_RWops *rw=SDL_RWFromFile(outinifilepath->data,"wb");
												if(!rw){
													esdl;
												}else{
													if(SDL_RWwrite(rw,inistring->data,inistring->size-1,1)!=1){
														esdl;
													}else{
														exit_code=0;
													}
													if(SDL_RWclose(rw)<0){
														esdl;
														exit_code=1;
													}
												}
											}
											SDL_FreeSurface(sur);
										}
									}
									for(typeof(pool->n_loaded_surfaces) l=0;l<pool->n_loaded_surfaces;++l)
										SDL_FreeSurface(pool->records[l].surface);
								}
								IMG_Quit();
							}else{
								wlog("The path to save the resulting INI file is not specified.");
							}
						}else{
							wlog("The path to save the resulting image is not specified.");
						}
					}else{
						wlog("No identifier and file path bindings are specified.");
					}
					exit_code=0;
				}
				break;
			}
			case SET_INI_PARSER_CANCELLED:{
				break;
			}
			case SET_INI_PARSER_UTF8_ERROR:{
				elog("While scanning the command line, a UTF-8 encoding error was detected.");
				break;
			}
		}
		for(typeof(pool->n_records) l=0;l<pool->n_records;++l){
			SDL_free(pool->records[l].file_id);
			SDL_free(pool->records[l].file_path);
		}
		files_free(pool);
	}
	SDL_Quit();
	return exit_code;
}
