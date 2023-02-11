
# SEABASS

## The Penultimate programming language- Codename SEABASS

This is the repository of the penultimate programming language, SEABASS.

The success of the project thus far is a gift from the Lord.

If you want to see this project succeed, pray and ask the Lord to bless it.

Praise God!

## What is Seabass designed for?

systems implementation. It is purpose-built for writing operating systems,
kernels, bootloaders, compilers, game engines, and any situation under which manual memory management
and obvious control flow are required.

This means, for instance:

* Seabass does not have constructors or destructors.

* Seabass does not have `new` and `delete` or any similar functionality.

* Seabass does not have a standard template library, standard library, or any other set of
standard runtime functions, as such is not portable between platforms.

* Seabass does not  have garbage collection or array bounds checking.

## What makes it different from, say, C?

Other than the obvious syntax changes? Well...

* No function pointers.

* switch does not do bounds checking, and automatically assumes a 0-n integer value. it uses goto labels.

* More well-defined behavior.

* No early returns for && and ||. Control flow is always explicit.

* Methods. You can define functions that operate on structs.

* You cannot pass or  use structs by value.

* Dedicated `memcpy` operator: `:=`






