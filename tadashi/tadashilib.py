#!/usr/bin/env python

# import yaml

from node import Scops


def compare(sch_tree, sched):
    print("ok")


def main():
    input_path = "./examples/depnodep.c"
    output_path = "./examples/depnodep.tadashilib.c"
    scops = Scops(input_path)
    schedules_trees = []
    for scop in scops:
        # sched = get_schedule_yaml(i).decode()
        # sched = yaml.load(sched, Loader=yaml.SafeLoader)
        sch_tree = scop.get_schedule_tree()
        # compare(sch_tree, sched)
        schedules_trees.append(sch_tree)

    scop = schedules_trees[0]  # select_scop()
    node = scop[1]  # model.select_node(scop)
    print(f"{node=}")
    node.tile(10)
    scops.generate_code(output_path)
    print(f"PYTHON DONE")


if __name__ == "__main__":
    main()
