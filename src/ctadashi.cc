/** @file */
// Date: 2024, January
// Author: Emil VATAI, Riken
//
// This file is the "C side" between the C and Python code of tadashi.

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <isl/aff_type.h>
#include <isl/space_type.h>
#include <pet.h>
#include <sstream>
#include <string>
#include <vector>

#include <isl/aff.h>
#include <isl/ast.h>
#include <isl/ctx.h>
#include <isl/id.h>
#include <isl/printer.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/val.h>

#include "codegen.h"
#include "legality.h"
#include "transformations.h"

extern "C" {
#include "ctadashi.h"

/******** scops constructor/descructor **********************/

struct scop_t {
  pet_scop *scop;
  isl_union_map *dependency;
  isl_schedule_node *current_node;
  isl_schedule_node *tmp_node;
  int modified;
  std::vector<std::string> strings;
  const char *
  add_string(char *str) {
    this->strings.push_back(str);
    free(str);
    return this->strings.back().c_str();
  };
};

// const char *
//  scop_t::add_string(char *str)

typedef std::vector<struct scop_t> scops_t;
std::vector<scops_t> SCOPS_POOL;

class Scop {
private:
  pet_scop *scop;
  isl_union_map *dependency;
  isl_schedule_node *current_node;
  isl_schedule_node *tmp_node;
  int modified;
  std::vector<std::string> strings;

public:
  Scop(pet_scop *scop) : scop(scop), tmp_node(nullptr), modified(false) {
    dependency = get_dependencies(scop);
    isl_schedule *schedule = pet_scop_get_schedule(scop);
    current_node = isl_schedule_get_root(schedule);
    schedule = isl_schedule_free(schedule);
  }
  const char *
  add_string(char *str) {
    this->strings.push_back(str);
    free(str);
    return this->strings.back().c_str();
  };
};

__isl_give isl_printer *
get_scop_callback(__isl_take isl_printer *p, pet_scop *scop, void *user) {
  std::vector<scop_t> *scops = (scops_t *)user;
  isl_schedule *sched = pet_scop_get_schedule(scop);
  struct scop_t tmp = {.scop = scop,
                       .dependency = get_dependencies(scop),
                       .current_node = isl_schedule_get_root(sched),
                       .tmp_node = nullptr,
                       .modified = false};
  isl_schedule_free(sched);
  scops->push_back(tmp);
  return p;
}

class Scops {
private:
  isl_ctx *ctx;
  std::vector<scop_t> scops;

public:
  Scops(char *input) : ctx(isl_ctx_alloc_with_pet_options()) {
    FILE *output = fopen("/dev/null", "w");
    // pet_options_set_autodetect(ctx, 1);
    // pet_options_set_signed_overflow(ctx, 1);
    // pet_options_set_encapsulate_dynamic_control(ctx, 1);
    pet_transform_C_source(ctx, input, output, get_scop_callback, &scops);
    fclose(output);
  };
  int
  num_scops() {
    return scops.size();
  }
  ~Scops() {};
};

/// Entry point.
int
init_scops(char *input) { // Entry point

  isl_ctx *ctx = isl_ctx_alloc_with_pet_options();
  FILE *output = fopen("/dev/null", "w");
  // pet_options_set_autodetect(ctx, 1);
  // pet_options_set_signed_overflow(ctx, 1);
  // pet_options_set_encapsulate_dynamic_control(ctx, 1);
  SCOPS_POOL.emplace_back();
  pet_transform_C_source(ctx, input, output, get_scop_callback, &SCOPS_POOL[0]);
  fclose(output);
  return SCOPS_POOL[0].size();
}

void
free_scops() {
  if (SCOPS_POOL[0].size() == 0)
    return;
  isl_set *set = pet_scop_get_context(SCOPS_POOL[0][0].scop);
  isl_ctx *ctx = isl_set_get_ctx(set);
  isl_set_free(set);
  for (size_t i = 0; i < SCOPS_POOL[0].size(); ++i) {
    scop_t *si = &SCOPS_POOL[0][i];
    isl_union_map_free(si->dependency);
    isl_schedule_node_free(si->current_node);
    if (si->tmp_node != NULL)
      isl_schedule_node_free(si->tmp_node);
    pet_scop_free(si->scop);
    si->strings.clear();
  }
  SCOPS_POOL[0].clear();
  isl_ctx_free(ctx);
}

/******** node info *****************************************/

int
get_type(size_t scop_idx) {
  return isl_schedule_node_get_type(SCOPS_POOL[0][scop_idx].current_node);
}

size_t
get_num_children(size_t scop_idx) {
  return isl_schedule_node_n_children(SCOPS_POOL[0][scop_idx].current_node);
}

const char *
get_expr(size_t idx) {
  scop_t *si = &SCOPS_POOL[0][idx];
  if (isl_schedule_node_get_type(si->current_node) != isl_schedule_node_band)
    return "";
  isl_multi_union_pw_aff *mupa =
      isl_schedule_node_band_get_partial_schedule(si->current_node);
  char *tmp = isl_multi_union_pw_aff_to_str(mupa);
  mupa = isl_multi_union_pw_aff_free(mupa);
  return si->add_string(tmp);
}

const char *
get_loop_signature(size_t scop_idx) {
  scop_t *si = &SCOPS_POOL[0][scop_idx];
  if (isl_schedule_node_get_type(si->current_node) != isl_schedule_node_band)
    return "[]";
  std::stringstream ss;
  isl_multi_union_pw_aff *mupa;
  mupa = isl_schedule_node_band_get_partial_schedule(si->current_node);
  assert(isl_multi_union_pw_aff_dim(mupa, isl_dim_out) == 1);
  // TODO save name
  isl_union_set *domain = isl_multi_union_pw_aff_domain(mupa);
  isl_size num_sets = isl_union_set_n_set(domain);
  isl_set_list *slist = isl_union_set_get_set_list(domain);
  ss << "[";
  for (isl_size set_idx = 0; set_idx < num_sets; set_idx++) {
    if (set_idx)
      ss << ", ";
    isl_set *set = isl_set_list_get_at(slist, set_idx);
    isl_size num_params = isl_set_dim(set, isl_dim_param);
    ss << "{'params' : [";
    for (isl_size di = 0; di < num_params; di++) {
      if (di)
        ss << ", ";
      ss << "'" << isl_set_get_dim_name(set, isl_dim_param, di) << "'";
    }
    ss << "], 'vars' :[";
    isl_size num_vars = isl_set_dim(set, isl_dim_set);
    for (isl_size di = 0; di < num_vars; di++) {
      if (di)
        ss << ", ";
      ss << "'" << isl_set_get_dim_name(set, isl_dim_set, di) << "'";
    }
    ss << "]}";
    isl_set_free(set);
  }
  ss << "]";
  isl_set_list_free(slist);
  isl_union_set_free(domain);
  si->strings.push_back(ss.str());
  return si->strings.back().c_str();
}

const char *
print_schedule_node(size_t scop_idx) {
  isl_schedule_node *node = SCOPS_POOL[0][scop_idx].current_node;
  return isl_schedule_node_to_str(node);
}

/******** current node manipulation *************************/

void
goto_root(size_t scop_idx) {
  SCOPS_POOL[0][scop_idx].current_node =
      isl_schedule_node_root(SCOPS_POOL[0][scop_idx].current_node);
}

void
goto_parent(size_t scop_idx) {
  SCOPS_POOL[0][scop_idx].current_node =
      isl_schedule_node_parent(SCOPS_POOL[0][scop_idx].current_node);
}

void
goto_child(size_t scop_idx, size_t child_idx) {
  SCOPS_POOL[0][scop_idx].current_node =
      isl_schedule_node_child(SCOPS_POOL[0][scop_idx].current_node, child_idx);
}

/******** transformations ***********************************/

scop_t *
pre_transform(size_t scop_idx) {
  // Set up `tmp_node` as a copy of `current_node` because we don't
  // want to mess with the current node if the transformation is not
  // legal.
  //
  // However now that I think about it, this approach may be wrong,
  // since we might wanna get to illegal states, temporarily of course
  // - the only requirement is that we're in a legal state at the
  // final output.
  scop_t *si = &SCOPS_POOL[0][scop_idx]; // Just save some typing.
  if (si->tmp_node != NULL)
    si->tmp_node = isl_schedule_node_free(si->tmp_node);
  si->tmp_node = isl_schedule_node_copy(si->current_node);
  return si;
}

int
post_transform(size_t scop_idx) {
  scop_t *si = &SCOPS_POOL[0][scop_idx]; // Just save some typing.
  isl_union_map *dep = isl_union_map_copy(si->dependency);
  isl_schedule *sched = isl_schedule_node_get_schedule(si->tmp_node);
  // Got `dep` and `sched`.
  isl_ctx *ctx = isl_schedule_get_ctx(sched);
  isl_bool legal = tadashi_check_legality(ctx, sched, si->dependency);
  isl_schedule_free(sched);
  si->modified = true;
  isl_schedule_node *node = si->current_node;
  si->current_node = si->tmp_node;
  si->tmp_node = node;
  return legal;
}

void
rollback(size_t scop_idx) {
  scop_t *si = &SCOPS_POOL[0][scop_idx];
  isl_schedule_node *tmp = si->tmp_node;
  si->tmp_node = si->current_node;
  si->current_node = tmp;
}

int
tile(size_t scop_idx, size_t tile_size) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_tile(si->tmp_node, tile_size);
  return post_transform(scop_idx);
}

int
interchange(size_t scop_idx) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_interchange(si->tmp_node);
  return post_transform(scop_idx);
}

int
fuse(size_t scop_idx, int idx1, int idx2) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_fuse(si->tmp_node, idx1, idx2);
  return post_transform(scop_idx);
}

int
full_fuse(size_t scop_idx) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_full_fuse(si->tmp_node);
  return post_transform(scop_idx);
}

int
partial_shift_var(size_t scop_idx, int pa_idx, long coeff, long var_idx) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node =
      tadashi_partial_shift_var(si->tmp_node, pa_idx, coeff, var_idx);
  return post_transform(scop_idx);
}

int
partial_shift_val(size_t scop_idx, int pa_idx, long val) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_partial_shift_val(si->tmp_node, pa_idx, val);
  return post_transform(scop_idx);
}

int
full_shift_var(size_t scop_idx, long coeff, long var_idx) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_full_shift_var(si->tmp_node, coeff, var_idx);
  return post_transform(scop_idx);
}

int
full_shift_val(size_t scop_idx, long val) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_full_shift_val(si->tmp_node, val);
  return post_transform(scop_idx);
}

int
full_shift_param(size_t scop_idx, long coeff, long param_idx) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_full_shift_param(si->tmp_node, coeff, param_idx);
  return post_transform(scop_idx);
}

int
partial_shift_param(size_t scop_idx, int pa_idx, long coeff, long param_idx) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node =
      tadashi_partial_shift_param(si->tmp_node, pa_idx, coeff, param_idx);
  return post_transform(scop_idx);
}

int
set_parallel(size_t scop_idx) {
  scop_t *si = pre_transform(scop_idx);
  si->tmp_node = tadashi_set_parallel(si->tmp_node);
  isl_union_map *dep = isl_union_map_copy(si->dependency);
  isl_schedule_node *node = isl_schedule_node_copy(si->tmp_node);
  isl_ctx *ctx = isl_schedule_node_get_ctx(node);
  node = isl_schedule_node_first_child(node);
  isl_bool legal = tadashi_check_legality_parallel(ctx, node, si->dependency);
  node = isl_schedule_node_free(node);
  si->modified = true;
  node = si->current_node;
  si->current_node = si->tmp_node;
  si->tmp_node = node;
  return legal;
}

int
set_loop_opt(size_t scop_idx, int pos, int opt) {
  isl_schedule_node *node = SCOPS_POOL[0][scop_idx].current_node;
  node = isl_schedule_node_band_member_set_ast_loop_type(
      node, pos, (enum isl_ast_loop_type)opt);
  SCOPS_POOL[0][scop_idx].current_node = node;
  return 1;
}

static __isl_give isl_printer *
generate_code_callback(__isl_take isl_printer *p, struct pet_scop *scop,
                       void *user) {
  isl_ctx *ctx;
  isl_schedule *sched;
  size_t *scop_idx = (size_t *)user;
  struct scop_t *scop_info = &SCOPS_POOL[0][*scop_idx];

  if (!scop || !p)
    return isl_printer_free(p);
  if (!scop_info->modified) {
    p = pet_scop_print_original(scop, p);
  } else {
    sched = isl_schedule_node_get_schedule(scop_info->current_node);
    p = codegen(p, scop_info->scop, sched);
  }
  pet_scop_free(scop);
  (*scop_idx)++;
  return p;
}

int
generate_code(const char *input_path, const char *output_path) {
  int r = 0;
  isl_ctx *ctx = isl_schedule_node_get_ctx(SCOPS_POOL[0][0].current_node);
  size_t scop_idx = 0;

  //   isl_options_set_ast_print_macro_once(ctx, 1);
  //   pet_options_set_encapsulate_dynamic_control(ctx, 1);

  FILE *output_file = fopen(output_path, "w");
  r = pet_transform_C_source(ctx, input_path, output_file,
                             generate_code_callback, &scop_idx);
  fclose(output_file);
  return r;
}

} // extern "C"
