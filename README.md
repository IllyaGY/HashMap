# HashMap

Generic hashmap in C using separate chaining for collisions.

The table stores copies of arbitrary byte keys and values:

- buckets are stored in an array
- collisions are handled with singly linked lists
- hashes are stored per entry
- resize happens automatically when the load factor exceeds `0.75`

Public API:

- `create_hm`
- `delete_hm`
- `insert_hm`
- `get_hm`
- `remove_hm`
- `contains_hm`
- `foreach_hm`
- `is_empty_hm`
- `get_size_hm`
- `get_capacity_hm`

`hash_bytes()` is intentionally separate so you can swap in a stronger hash later without rewriting the table logic.

Example build:

```sh
make test
./test.out
```

The current `test.c` is an `ncurses` demo:

- it continuously refreshes the table view
- it shows each entry's bucket and chain position
- press `i` to prompt for a name to insert
- press `r` to prompt for a name to remove
- press `q` to quit

Available `make` targets:

- `make static`: build `libhm.a`
- `make shared`: build `libhm.so`
- `make dynamic`: build `libhm.dylib`
- `make test`: build the ncurses demo linked against the static library
- `make test-dynamic`: build the ncurses demo linked against `libhm.dylib`
- `make all`: build all library formats and both demo binaries
- `make clean`: remove generated files

Versioned releases:

```sh
./release_version.sh 1.0.0
./release_version.sh 1.0.0 --git-tag
```

That creates versioned library artifacts under `dist/<version>/`, along with an optional annotated Git tag.
