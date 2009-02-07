#include <ruby.h>
#ifdef RUBY_19_COMPATIBILITY
#include <ruby/st.h>
#else
#include <env.h>
#include <node.h>
#include <st.h>
#endif
#include <stdlib.h>

static char callsite_hook_set_p;

typedef struct {
        const char *sourcefile;
        unsigned int sourceline;
        VALUE curr_meth;
} type_def_site;       
static VALUE caller_info = 0;
static VALUE method_def_site_info = 0;

static int caller_stack_len = 1;

/*
 *
 * callsite hook and associated functions
 *
 * */

static VALUE
record_callsite_info(VALUE args)
{
  VALUE caller_ary;
  VALUE curr_meth;
  VALUE count_hash;
  VALUE count;
  VALUE *pargs = (VALUE *)args;

  caller_ary = pargs[0];
  curr_meth = pargs[1];
  count_hash = rb_hash_aref(caller_info, curr_meth);
  if(TYPE(count_hash) != T_HASH) { 
          /* Qnil, anything else should be impossible unless somebody's been
           * messing with ObjectSpace */
          count_hash = rb_hash_new();
          rb_hash_aset(caller_info, curr_meth, count_hash);
  }
  count = rb_hash_aref(count_hash, caller_ary);
  if(count == Qnil) 
          count = INT2FIX(0);
  count = INT2FIX(FIX2UINT(count) + 1);
  rb_hash_aset(count_hash, caller_ary, count);
  /* * /
  printf("CALLSITE: %s -> %s   %d\n", RSTRING_PTR(rb_inspect(curr_meth)),
                  RSTRING_PTR(rb_inspect(caller_ary)), FIX2INT(count));
  / * */

  return Qnil;
}


static VALUE
record_method_def_site(VALUE args)
{
  type_def_site *pargs = (type_def_site *)args;
  VALUE def_site_info;
  // VALUE hash;

  if( RTEST(rb_hash_aref(method_def_site_info, pargs->curr_meth)) )
          return Qnil;
  def_site_info = rb_ary_new();
  rb_ary_push(def_site_info, rb_str_new2(pargs->sourcefile));
  rb_ary_push(def_site_info, INT2NUM(pargs->sourceline+1));
  rb_hash_aset(method_def_site_info, pargs->curr_meth, def_site_info);
  /* * /
  printf("DEFSITE: %s:%d  for %s\n", pargs->sourcefile, pargs->sourceline+1,
                  RSTRING_PTR(rb_inspect(pargs->curr_meth)));
  / * */
  
  return Qnil;
}

static VALUE
callsite_custom_backtrace(int lev)
{
#ifdef RUBY_19_COMPATIBILITY
  ID id;
  VALUE klass;
  VALUE klass_path;
  VALUE eval_string;
  
  rb_frame_method_id_and_class(&id, &klass);
  if (id == ID_ALLOCATOR)
    return Qnil;
  if (klass) {
    if (TYPE(klass) == T_ICLASS) {
      klass = RBASIC(klass)->klass;
    }
    else if (FL_TEST(klass, FL_SINGLETON)) {
      klass = rb_iv_get(klass, "__attached__");
    }
  }
  // rb_sprintf("\"#<Class:%s>\"", RSTRING_PTR(klass_path))
  
  /*
  klass = class << klass; self end unless klass === eval("self", binding)
  */
  
  klass_path = rb_class_path(klass);
  VALUE reciever = rb_funcall(rb_binding_new(), rb_intern("eval"), 1, rb_str_new2("self"));
  if (rb_funcall(klass, rb_intern("=="), 1, reciever) == Qtrue) {
    klass_path = rb_sprintf("\"#<Class:%s>\"", RSTRING_PTR(klass_path));
    OBJ_FREEZE(klass_path);
  }
  
  eval_string = rb_sprintf("caller[%d, 1].map do |line|\nmd = /^([^:]*)(?::(\\d+)(?::in `(?:block in )?(.*)'))?/.match(line)\nraise \"Bad backtrace format\" unless md\n[%s, md[3] ? md[3].to_sym : nil, md[1], (md[2] || '').to_i]\nend", lev, RSTRING_PTR(klass_path));
  return rb_eval_string(RSTRING_PTR(eval_string));
#else
  VALUE ary;
  struct FRAME *frame = ruby_frame;
  NODE *n;
  VALUE level;
  VALUE klass;

  ary = rb_ary_new();
  if (frame->last_func == ID_ALLOCATOR) {
          frame = frame->prev;
  }
  for (; frame && (n = frame->node); frame = frame->prev) {
          if (frame->prev && frame->prev->last_func) {
                  if (frame->prev->node == n) continue;
                  level = rb_ary_new();
                  klass = frame->prev->last_class ? frame->prev->last_class : Qnil;
                  if(TYPE(klass) == T_ICLASS) {
                          klass = CLASS_OF(klass);
                  }
                  rb_ary_push(level, klass);
                  rb_ary_push(level, ID2SYM(frame->prev->last_func));
                  rb_ary_push(level, rb_str_new2(n->nd_file));
                  rb_ary_push(level, INT2NUM(nd_line(n)));
          }
          else {
                  level = rb_ary_new();
                  rb_ary_push(level, Qnil);
                  rb_ary_push(level, Qnil);
                  rb_ary_push(level, rb_str_new2(n->nd_file));
                  rb_ary_push(level, INT2NUM(nd_line(n)));
          }
          rb_ary_push(ary, level);
          if(--lev == 0)
                  break;
  }

  return ary;
#endif
}

static void
#ifdef RUBY_19_COMPATIBILITY
coverage_event_callsite_hook(rb_event_flag_t event, VALUE node, 
                VALUE self, ID mid, VALUE klass)
#else
coverage_event_callsite_hook(rb_event_t event, NODE *node, VALUE self, 
                ID mid, VALUE klass)
#endif
{
  VALUE caller_ary;
  VALUE curr_meth;
  VALUE args[2];
  int status;

  caller_ary = callsite_custom_backtrace(caller_stack_len);

#ifdef RUBY_19_COMPATIBILITY
  VALUE klass_path;
  curr_meth = rb_ary_new();
   
  rb_frame_method_id_and_class(&mid, &klass);
  
  if (mid == ID_ALLOCATOR)
    return Qnil;
  if (klass) {
    if (TYPE(klass) == T_ICLASS) {
      klass = RBASIC(klass)->klass;
    }
    else if (FL_TEST(klass, FL_SINGLETON)) {
      klass = rb_iv_get(klass, "__attached__");
    }
  }
  
  /*
  klass = class << klass; self end unless klass === eval("self", binding)
  */
  
  klass_path = rb_class_path(klass);
  VALUE reciever = rb_funcall(rb_binding_new(), rb_intern("eval"), 1, rb_str_new2("self"));
  if (rb_funcall(klass, rb_intern("=="), 1, reciever) == Qtrue) {
    klass_path = rb_sprintf("#<Class:%s>", RSTRING_PTR(klass_path));
   OBJ_FREEZE(klass_path);
  }
    
  rb_ary_push(curr_meth, klass_path);
  rb_ary_push(curr_meth, ID2SYM(mid));

  args[0] = caller_ary;
  args[1] = curr_meth;
  rb_protect(record_callsite_info, (VALUE)args, &status);

  if(!status) {
    type_def_site args;

    args.sourcefile = rb_sourcefile();
    args.sourceline = rb_sourceline();
    args.curr_meth = curr_meth;
    rb_protect(record_method_def_site, (VALUE)&args, NULL);
  }
#else
  if(TYPE(klass) == T_ICLASS) {
    klass = CLASS_OF(klass);
  }
  curr_meth = rb_ary_new();
  rb_ary_push(curr_meth, klass);
  rb_ary_push(curr_meth, ID2SYM(mid));

  args[0] = caller_ary;
  args[1] = curr_meth;
  rb_protect(record_callsite_info, (VALUE)args, &status);
  if(!status && node) {
    type_def_site args;        

    args.sourcefile = node->nd_file;
    args.sourceline = nd_line(node) - 1;
    args.curr_meth = curr_meth;
    rb_protect(record_method_def_site, (VALUE)&args, NULL);
  }

#endif
  if(status)
    rb_gv_set("$!", Qnil);
}


static VALUE
cov_install_callsite_hook(VALUE self)
{
  if(!callsite_hook_set_p) {
          if(TYPE(caller_info) != T_HASH)
                  caller_info = rb_hash_new();
          callsite_hook_set_p = 1;
#ifdef RUBY_19_COMPATIBILITY
          VALUE something = 0;
          rb_add_event_hook(coverage_event_callsite_hook, 
                  RUBY_EVENT_CALL, something);
#else 
          rb_add_event_hook(coverage_event_callsite_hook, 
                  RUBY_EVENT_CALL);
#endif          
          return Qtrue;
  } else
          return Qfalse;
}


static VALUE
cov_remove_callsite_hook(VALUE self)
{
 if(!callsite_hook_set_p) 
         return Qfalse;
 else {
         rb_remove_event_hook(coverage_event_callsite_hook);
         callsite_hook_set_p = 0;
         return Qtrue;
 }
}


static VALUE
cov_generate_callsite_info(VALUE self)
{
  VALUE ret;

  ret = rb_ary_new();
  rb_ary_push(ret, caller_info);
  rb_ary_push(ret, method_def_site_info);
  return ret;
}


static VALUE
cov_reset_callsite(VALUE self)
{
  if(callsite_hook_set_p) {
	  rb_raise(rb_eRuntimeError, 
		  "Cannot reset the callsite info in the middle of a traced run.");
	  return Qnil;
  }

  caller_info = rb_hash_new();
  method_def_site_info = rb_hash_new();
  return Qnil;
}

void
Init_rcov_callsite()
{
 VALUE mRcov;
 VALUE mRCOV__;
 ID id_rcov = rb_intern("Rcov");
 ID id_coverage__ = rb_intern("RCOV__");
 // ID id_script_lines__ = rb_intern("SCRIPT_LINES__");

 if(rb_const_defined(rb_cObject, id_rcov)) 
         mRcov = rb_const_get(rb_cObject, id_rcov);
 else
         mRcov = rb_define_module("Rcov");

 if(rb_const_defined(mRcov, id_coverage__))
         mRCOV__ = rb_const_get_at(mRcov, id_coverage__);
 else
         mRCOV__ = rb_define_module_under(mRcov, "RCOV__");

 callsite_hook_set_p = 0;
 caller_info = rb_hash_new();
 method_def_site_info = rb_hash_new();
 rb_gc_register_address(&caller_info);
 rb_gc_register_address(&method_def_site_info);
 
 rb_define_singleton_method(mRCOV__, "install_callsite_hook", 
                 cov_install_callsite_hook, 0);
 rb_define_singleton_method(mRCOV__, "remove_callsite_hook", 
                 cov_remove_callsite_hook, 0);
 rb_define_singleton_method(mRCOV__, "generate_callsite_info", 
		 cov_generate_callsite_info, 0);
 rb_define_singleton_method(mRCOV__, "reset_callsite", cov_reset_callsite, 0);
}
/* vim: set sw=8 expandtab: */
