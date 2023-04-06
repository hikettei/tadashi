#!/usb/bin/env python
import functools
import re

import gdb.printing


def get_string(s):
    main = str(s).split()[1:]
    return " ".join(main)[1:-1]


def deref(val):
    return val.dereference() if bool(val) else ""


def sderef(val):
    return str(deref(val))


class PrinterBase:
    def __init__(self, val):
        self.val = val

    def get_array(self, key, nkey, fn):
        """Print an array like member.

        Arguments:

            key[str]: The member where the data is.

            nkey[str]: A string of the form 'key1->key2->...->keyN',
                which describes the number of data elements which will
                be read.

            fn[Callable]: The transformation performed on each data
                element.

        """
        arr = self.val[key]
        cur = self.val
        for k in nkey.split("->"):
            cur = cur[k]
        rng = range(int(cur))
        return [fn(arr[i]) for i in rng]

    def print_members(self, members):
        result = ""
        for member in members:
            if len(member) == 3:
                tag, key, fn = member
                if fn == deref and int(self.val[key]) == 0:
                    continue
                result += f"{tag}:{fn(self.val[key])}; "
            elif len(member) == 4:
                tag, key, nkey, fn = member
                result += f"{tag}:{self.get_array(key, nkey, fn)}; "
        return result


class PlutoProgPrinter(PrinterBase):
    def to_string(self):
        nstmts = int(self.val["nstmts"])
        stmts = self.val["stmts"]
        stmts = "\n" + "\n".join(
            self.get_array("stmts", "nstmts", lambda t: str(t.dereference()))
        )
        return f"PlutoProg ({nstmts}): {stmts}"


class PlutoAccessPrinter(PrinterBase):
    def to_string(self):
        """
        typedef struct pluto_access {
        int sym_id;
        char *name;

        /* scoplib_symbol_p symbol; */

        PlutoMatrix *mat;
        } PlutoAccess;
        """
        name = get_string(self.val["name"])
        mat = sderef(self.val["mat"])
        return f"{name}{mat}"


class PlutoConstraintPrinter(PrinterBase):
    def to_string(self):
        buf = self.val["buf"]
        ncols = int(self.val["ncols"])
        nrows = int(self.val["nrows"])
        names = self.get_array("names", "ncols", get_string)
        eqs = []
        for row in range(nrows):
            is_eq = "=" if bool(self.val["is_eq"][row]) else "<="
            base = row * int(self.val["alloc_ncols"])
            terms = [f"0{is_eq}"]
            terms += [
                f"{buf[base + i]}{names[i]}" for i in range(ncols) if int(buf[base + i])
            ]
            eqs.append("".join([(t if t[0] == "-" else f"+{t}") for t in terms]))

        return f"Cntr:{eqs}"


class PlutoMatrixPrinter(PrinterBase):
    def to_string(self):
        buf = self.val["val"]
        cols = range(int(self.val["ncols"]))
        rows = range(int(self.val["nrows"]))
        alloc_ncols = int(self.val["alloc_ncols"])
        rows_strs = []
        for row in rows:
            row = [str(buf[row][i]) for i in cols]
            rows_strs.append(",".join(row))
        return f"{{{'|'.join(rows_strs)}}}"


class StmtPrinter(PrinterBase):
    def to_string(self):
        members = [
            ("id", "id", int),  # OK!
            ("it", "iterators", "dim", get_string),  # check?!
            ("dom", "domain", deref),  # OK!
            ("t", "text", get_string),  # OK!
            ("iol", "is_orig_loop", "dim", int),  # OK!,
            # ("dim", "dim", int), # OK!
            ("do", "dim_orig", int),  # OK!
            ("t", "tile", int),  # OK!
            ("tr", "trans", deref),  # OK!
            ("eh", "evicted_hyp", deref),  # OK!
            ("ehp", "evicted_hyp_pos", int),  # OK!
            ("hty", "hyp_types", "trans->nrows", str),  # OK!
            ("ntl", "num_tiled_loops", int),  # OK!
            ("rx", "reads", "nreads", sderef),  # OK!
            # ("nr", "nreads", int),  # OK!
            ("wx", "writes", "nwrites", sderef),  # OK!
            # ("nw", "nwrites", int),  # OK!
            ("sid", "scc_id", int),  # OK!
            ("cid", "cc_id", int),  # OK!
            ("ftd", "first_tile_dim", int),  # OK!
            ("ltd", "last_tile_dim", int),  # OK!
            ("typ", "type", int),  # PlutoStmtType NEXT?!
            ("plid", "ploop_id", int),  # OK!
            ("pcs", "parent_compute_stmt", deref),  # OK!
            ("isd", "intra_stmt_dep_cst", int),  # PlutoConstraints*
            ("ps", "pstmt", int),  # pet_stmt*
        ]

        statement = self.print_members(members)
        return f"stmt: {statement}"


class PlutoHypTypePrinter(PrinterBase):
    def to_string(self):
        typs = ["UNKNOWN", "LOOP", "TILE_SPACE_LOOP", "SCALAR"]
        return typs[int(self.val)]


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("pluto_prog_printer")
    pp.add_printer("PlutoProg", "^plutoProg *$", PlutoProgPrinter)
    pp.add_printer("stmt", "^statement *$", StmtPrinter)
    pp.add_printer("pluto_constraint", "^pluto_constraints *$", PlutoConstraintPrinter)
    pp.add_printer("pluto_matrix", "^pluto_matrix *$", PlutoMatrixPrinter)
    pp.add_printer("pluto_access", "^pluto_access *$", PlutoAccessPrinter)
    pp.add_printer("hyptype", "^hyptype *$", PlutoHypTypePrinter)
    return pp