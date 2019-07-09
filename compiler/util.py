from typing import Optional, TypeVar
import parser

def get_flattened(nod: parser.Node) -> Optional[str]:
    if "flattened" in nod.xattrs:
        return nod.xattrs["flattened"]
    if nod.i("ident"):
        return nod.data
    elif nod.i("."):
        flattened_left = get_flattened(nod[0])
        flattened_right = get_flattened(nod[1])
        if flattened_left is None or flattened_right is None:
            return None
        ret = "%s.%s" % (flattened_left, flattened_right)
        nod.xattrs["flattened"] = ret
        return ret
    else:
        return None

NonNullTypeVar = TypeVar("NonNullTypeVar")
def nonnull(var: Optional[NonNullTypeVar], error: Optional[Exception]=None) -> NonNullTypeVar:
    if var is None:
        if error is not None:
            raise error
        else:
            raise ValueError("Unexpected null value")
    return var