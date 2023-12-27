#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/error.h>
#include <mruby/internal.h>

MRB_API mrb_value
mrb_pm_load_exec(mrb_state *mrb, pm_parser_t *p, pm_node_t *node, mrb_ccontext *c)
{
  struct RClass *target = mrb->object_class;
  struct RProc *proc;
  mrb_value v;
  mrb_int keep = 0;

#if 0
  if (!p) {
    return mrb_undef_value();
  }
  if (!p->tree || p->nerr) {
    if (c) c->parser_nerr = p->nerr;
    if (p->capture_errors) {
      char buf[256];

      strcpy(buf, "line ");
      dump_int(p->error_buffer[0].lineno, buf+5);
      strcat(buf, ": ");
      strncat(buf, p->error_buffer[0].message, sizeof(buf) - strlen(buf) - 1);
      mrb->exc = mrb_obj_ptr(mrb_exc_new(mrb, E_SYNTAX_ERROR, buf, strlen(buf)));
      mrb_parser_free(p);
      return mrb_undef_value();
    }
    else {
      if (mrb->exc == NULL) {
        mrb->exc = mrb_obj_ptr(mrb_exc_new_lit(mrb, E_SYNTAX_ERROR, "syntax error"));
      }
      mrb_parser_free(p);
      return mrb_undef_value();
    }
  }
#endif
  proc = mrb_pm_generate_code(mrb, c, p, node);
#if 0
  mrb_parser_free(p);
#endif
  if (proc == NULL) {
    if (mrb->exc == NULL) {
      mrb->exc = mrb_obj_ptr(mrb_exc_new_lit(mrb, E_SCRIPT_ERROR, "codegen error"));
    }
    return mrb_undef_value();
  }
  if (c) {
    if (c->dump_result) mrb_codedump_all(mrb, proc);
    if (c->no_exec) return mrb_obj_value(proc);
    if (c->target_class) {
      target = c->target_class;
    }
    if (c->keep_lv) {
      keep = c->slen + 1;
    }
    else {
      c->keep_lv = TRUE;
    }
  }
  MRB_PROC_SET_TARGET_CLASS(proc, target);
  if (mrb->c->ci) {
    mrb_vm_ci_target_class_set(mrb->c->ci, target);
  }
  v = mrb_top_run(mrb, proc, mrb_top_self(mrb), keep);
  if (mrb->exc) return mrb_nil_value();
  return v;
}

MRB_API mrb_value
mrb_load_nstring_cxt(mrb_state *mrb, const char *s, size_t len, mrb_ccontext *c)
{
  pm_parser_t parser;
  pm_parser_init(&parser, (const uint8_t *)s, len, NULL);

  pm_node_t *node = pm_parse(&parser);

  pm_buffer_t buffer = { 0 };
  pm_prettyprint(&buffer, &parser, node);
  printf("%.*s\n", (int) buffer.length, buffer.value);
  pm_buffer_free(&buffer);

  mrb_p(mrb, mrb_pm_load_exec(mrb, &parser, node, c));

  pm_node_destroy(&parser, node);
  pm_parser_free(&parser);

  return mrb_load_exec(mrb, mrb_parse_nstring(mrb, s, len, c), c);
}
