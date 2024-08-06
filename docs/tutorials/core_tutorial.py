import gc


def types_found_in_tuples():
    elem_types = set()

    for obj in gc.get_objects():
        if isinstance(obj, tuple):
            elem_types.update(map(type, obj))

    for elem_type in elem_types:
        yield elem_type


# printing all results with multiple calls to print
for t in types_found_in_tuples():
    print(t)

# printing all results with one call to print
print(*types_found_in_tuples(), sep="\n")
