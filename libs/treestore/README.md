libtreestore
============

Libtreestore is a simple C library for reading/writing hierarchical data in a
json-like text format, or a chunk-based binary format.

A better way to describe the text format is like XML without the CDATA, and with
curly braces instead of tags:

```
rootnode {
    some_attribute = "some_string_value"
    some_numeric_attrib = 10
    vector_attrib = [255, 128, 0]
    array_attrib = ["tom", "dick", "harry"]

    # you can have multiple nodes with the same name
    childnode {
        childattr = "whatever"
    }
    childnode {
        another_childattr = "xyzzy"
    }
}
```

License
-------
Copyright (C) 2016 John Tsiombikas <nuclear@member.fsf.org>

Libtreestore is free software. Feel free to use, modify, and/or redistribute
it, under the terms of the MIT/X11 license. See LICENSE for detauls.

Issues
------
At the moment only the text format has been implemented.

More info soon...
