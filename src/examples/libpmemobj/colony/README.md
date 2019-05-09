Persistent Memory Development Kit

This is examples/libpmemobj/colony/README.

This directory contains an example application implemented using libpmemobj.
The colony example allows one to perform basic operations i.e. creating the
persistent colony, inserting a single element or elements from file, removing a
single item or the range, printing information about the content of the colony,
printing items belonging to the colony, deleting freed blocks and deleting the
colony. The user creating a colony determines the capacity the allocated blocks
will have. There are two types of items which are supported: int and PMEMoid.

The colony consists of:
	- a doubly linked list of memory blocks containing arrays that store
	items and skipfields (Bentley pattern) to "jump over" removed items and
	vacants (there is no need to reallocate upon insertion or removal)
	- a linked list of indexes of removed items (to reuse those indexes
	upon subsequent insertion)
	- a linked list of blocks in which all items have been removed (also for
	a reuse of indexes from these blocks)
	- additional structural metadata

The order of items is not important. Newly inserted ones are placed in the
following locations:
	- if it exists - the last free index from the list of indexes of removed
	items (LIFO)
	- if it exists - the first index from the last block from the list of
	blocks in which all items have been removed (LIFO), the rest of the
	indexes from this block is added to the list of free indexes
	- if it exists - the first unoccupied index in the colony, the
	unoccupied index means one that has not been used (insertion, removal)
	before, it is always in the newest block
	- a new block is created (with a new table), the first index is taken


To create a new colony using application run the following command:
```
	$ colony <filename> create-colony <colony-name> <element-type> <block-capacity>
```

Where `<filename>` is file where pool will be created or opened, `<colony-name>`
is the user's defined unique name, `<element-type>` is one of the two: int or
PMEMoid, `<block-capacity>` is a number of items that will be stored in a single
block.

To insert a single integer item into the colony run the following command:
```
	$ colony <filename> insert-int <colony-name> <item>
```

To insert a single PMEMoid item into the colony run the following command:
```
	$ colony <filename> insert-pmemoid <colony-name> <item-uuid> <item-off>
```

To insert integer items from a text file into the colony run the following
command:
```
	$ colony <filename> insert-int-from-file <colony-name> <path>
```

Where `<path>` is the path to the file from which the integer items are read.

To insert PMEMoid items from a text file into the colony run the following
command:
```
	$ colony <filename> insert-pmemoid-from-file <colony-name> <path>
```

Where `<path>` is the path to the file from which the values of the PMEMoid
structure (pool_uuid_lo and off) are read.

To remove a single item (if there is any) from the colony run the following
command:
```
	$ colony <filename> remove-item <colony-name> <index>
```

Where `<index>` is the index of an item in the colony.

To remove items from the range (if there are any) from the colony run the
following command:
```
	$ colony <filename> remove-range <colony-name> <index-from> <index-to>
```

Where `<index-from>` is the first to be removed, `<index-to>` is the last one.

To print information about the content of the colony run the following command:
```
	$ colony <filename> print-content <colony-name>
```

To print items belonging to the colony run the following command:
```
	$ colony <filename> print-colony <colony-name>
```

To delete freed blocks (previously inserted, then removed) from the colony run
the following command:
```
	$ colony <filename> delete-free-blocks <colony-name>
```

To delete the whole colony run the following command:
```
	$ colony <filename> delete-colony <colony-name>
```

Examples of usage:
```
	$ colony testfile create-colony coli int 23
	$ colony testfile create-colony colp pmemoid 5
	$ colony testfile insert-int coli 99
	$ colony testfile insert-pmemoid colp 326773440709551741 11134007090017
	$ colony testfile insert-int-from-file coli ints.txt
	$ colony testfile insert-pmemoid-from-file colp pmemoids.txt
	$ colony testfile remove-item coli 121
	$ colony testfile remove-range colp 14 49
	$ colony testfile print-content coli
	$ colony testfile print-colony colp
	$ colony testfile delete-free-blocks coli
	$ colony testfile delete-colony colp
```
