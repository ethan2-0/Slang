def get_flattened(nod):
    if "flattened" in nod.xattrs:
        return nod.xattrs["flattened"]
    if nod.i("ident"):
        return nod.data
    elif nod.i("."):
        ret = "%s.%s" % (get_flattened(nod[0]), get_flattened(nod[1]))
        nod.xattrs["flattened"] = ret
        return ret
    else:
        raise ValueError()
