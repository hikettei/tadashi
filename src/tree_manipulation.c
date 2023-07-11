#include <isl/aff.h>
#include <isl/aff_type.h>
#include <isl/ctx.h>
#include <isl/id.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>
#include <isl/union_map.h>
#include <stdio.h>

#include <isl/options.h>
#include <pet.h>

struct options {
  struct isl_options *isl;
  struct pet_options *pet;
  char *schedule;
  char *code;
  unsigned tree;
};

ISL_ARGS_START(struct options, options_args)
ISL_ARG_CHILD(struct options, isl, "isl", &isl_options_args, "isl options")
ISL_ARG_CHILD(struct options, pet, NULL, &pet_options_args, "pet options")
ISL_ARG_ARG(struct options, schedule, "schedule", NULL)
ISL_ARG_ARG(struct options, code, "code", NULL)
ISL_ARG_BOOL(struct options, tree, 0, "tree", 0,
             "input schedule is specified as schedule tree")
ISL_ARGS_END

ISL_ARG_DEF(options, struct options, options_args)

struct Args {
  char *filename;
  char *schedule;
};

struct Args get_args(int argc, char *argv[]) {
  struct Args args;

  if (argc < 2) {
    printf("Usage: %s <C/C++ source file> <schedule>\n", argv[0]);
    exit(-1);
  }
  args.filename = argv[1];

  args.schedule = 0;
  if (argc >= 3) {
    args.schedule = argv[2];
  }
  return args;
}

#define PRN(label, type, obj) printf("%s : %s\n", label, type##_to_str(obj))

void tree_manipulation(isl_schedule *schedule) {
  // experimenting with ../examples/depnodep.cc

  isl_ctx *ctx = isl_schedule_get_ctx(schedule);
  isl_schedule_node *root, *in, *jn, *leaf;
  root = isl_schedule_get_root(schedule);
  PRN("root", isl_schedule_node, root);
  in = isl_schedule_node_get_child(root, 0);
  jn = isl_schedule_node_get_child(in, 0);
  isl_multi_union_pw_aff *mupa =
      isl_schedule_node_band_get_partial_schedule(jn);
  jn = isl_schedule_node_delete(jn);
  jn = isl_schedule_node_parent(jn);
  jn = isl_schedule_node_insert_partial_schedule(jn, mupa);
  PRN("jn", isl_schedule_node, jn);

  // isl_schedule_node *cut_jn = isl_schedule_node_cut(jn);
  // PRN("cutjn", isl_schedule_node, cut_jn);
  // PRN("jn", isl_schedule_node, jn);

  // isl_schedule_node *graft = isl_schedule_node_graft_after(in, root);
  // PRN("", isl_schedule_node, graft);

  leaf = isl_schedule_node_get_child(jn, 0);
  // leaf = isl_schedule_node_group(leaf, isl_id_read_from_str(ctx, "t"));
}

int main(int argc, char *argv[]) {
  struct Args args = get_args(argc, argv);
  printf("Input file: %s\n", args.filename);
  // printf("Input schedule %s\n", args.schedule);

  struct options *options = options_new_with_defaults();
  isl_ctx *ctx = isl_ctx_alloc_with_options(&options_args, options);
  /* printf("set: %d\n", pet_options_set_autodetect(ctx, 0)); */
  /* printf("get: %d\n", pet_options_get_autodetect(ctx)); */
  pet_scop *scop = pet_scop_extract_from_C_source(ctx, args.filename, 0);
  if (!scop) {
    printf("No scop found!\n");
    isl_ctx_free(ctx);
    return -1;
  } else {
    printf("Scop found\n");
  }

  /* printf("may_reads: %s\n",
   * isl_union_map_to_str(pet_scop_get_may_reads(scop))); */
  /* printf("may_writes: %s\n", */
  /*        isl_union_map_to_str(pet_scop_get_may_writes(scop))); */
  /* printf("must_writes: %s\n", */
  /*        isl_union_map_to_str(pet_scop_get_must_writes(scop))); */

  isl_schedule *schedule = pet_scop_get_schedule(scop);
  // printf("schedule: %s\n", isl_schedule_to_str(schedule));
  tree_manipulation(schedule);
  isl_schedule_free(schedule);

  pet_scop_free(scop);
  isl_ctx_free(ctx);
  printf("DONE!\n");
  return 0;
}