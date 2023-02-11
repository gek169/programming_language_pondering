
# SEABASS

## The Penultimate programming language- Codename SEABASS

This is the repository of the penultimate programming language, SEABASS.

The success of the project thus far is a gift from the Lord.

If you want to see this project succeed, pray and ask the Lord to bless it.

Praise God!

The project is in pre-alpha, and is not ready for production.

## What does it look like?

See `vm_test.txt`. There should be more tests in the future.

In general, syntax is a mixture of lua, javascript, and C.

Here's the famous `fib` demo:

```
fn fib(int n)->int:
	if(n < 2)
		return 1;
	end
	int a
	int b
	int c
	c = b = a = 1;
	
	while(n)
		c = a + b;
		a = b;
		b = c;
		n--;
	end
	return c;
end
```

This obviously doesn't demonstrate even ten percent of the language, but, it should give you an idea of what it

feels like.



## Syntax oddities (Do you like them? Leave your thoughts)

* It is possible under some circumstances to use a variable before it is defined, inside of a scope:

```
fn demo_use_before_define():

	/*
		a is used, but, where is it defined? What type is it?
	*/
	a = 3;
	while(1)
		//WHERE ARE YOU?!?!
		a = 4;
		break
	end

	return;
	/*
		Oh! it's down here! How did that happen?
	*/
	sqword a
end
```

* There are numerous aliases for the primitive types, including my favorite and probably the most humorous, `sqword`.

(If you don't find sqword funny, then you probably don't live in a pineapple under the sea...)

The 'actual' type, followed by its aliases:
```
u8: char, uchar
i8: schar
u16: ushort
i16: sshort, short
u32: uint, ulong
i32: int, long, sint, slong
u64: qword, uqword, ullong
i64: sqword, llong, sllong
f32: float
f64: double 
```


## What is Seabass designed for?

General purpose portable programming, as its metaprogramming facilities should be strong enough to allow
totally arbitrary language constructs.

The base language syntax, though, is designed for systems implementation.

This means, for instance:

* Seabass does not have constructors or destructors.

* Seabass does not have `new` and `delete` or any similar functionality.

* Seabass does not have a standard template library, standard library, or any other set of
standard runtime functions, as such is not portable between platforms.

* Seabass does not have a default threading implementation. While it (will...) support `atomic` and `volatile` qualifiers,

they will do nothing on a target which does not utilize multithreading.

Multithreading must be achieved by using `asm` blocks, either directly, or wrapped in a function.

* Seabass does not  have garbage collection or array bounds checking.

## What makes it different from, say, C?

The major difference is that it's a metaprogramming language.

Dedicated compiletime/parsetime code can be written which directly manipulates the token list
passed to the parser (Not yet implemented...) or the AST itself (Implemented but unused and untested).

This would allow you, for instance, to write your own programming language syntaxes, and use them anywhere in your code.

You could write part of your program in typescript, and part of it in C, and they could be mixed together.

After that, you can write your own code generator, which takes the AST and spits out whatever you want to export.

This is all done without changing the code of the seabass 'compiler' (It's not really a whole compiler, is it?) itself.

## What makes Seabass different from C in terms of base language features?

Other than the obvious syntax changes? Well...

* No function pointers. I want the language to be compatible with platforms where jumping using the value in a register
is not supported. 

The lack of function pointers might sound distressing if you use them to provide callbacks or to have jump tables.

Use the switch statement instead!

* No variadic arguments. Yuck!

* switch does not do bounds checking, and automatically assumes a [0-n) (where n is the number of cases) integer value. 
it uses goto labels instead of `case` statements.

This makes it performance-optimized.

* More well-defined behavior. For instance, twos complement is assumed, and signed integer overflow is defined behavior.

* No early returns for && and ||. I'm on the fence about whether to change this or not.

* Methods. You can define functions that operate on structs and use a lua-style method invocation syntax.

`myStruct:myMethod(a,b,c)`

It works by creating a function with a specially mangled name that has `myStruct* this` as the firsta argument.

* You cannot pass or use structs (or arrays, for that matter) by value. This means you use pointers.

* Dedicated `memcpy` operator: `:=`

* Dedicated string comparison operators, `streq` and `strneq`.

* All types (except for arrays and structs) should fit into a register (on most platforms.)

* No struct or array initializers.

* Local variables may not have initial values (Due to how their allocation happens)

* No `const`. Can't really enforce it, anyway.

* No address-of operator. You cannot take the address of something.


Instead, you can take the address of struct members, with `myStruct.&myMember`

This was done to make writing register allocators easier.

if you want something to reside in memory so you can take its address, declare an array with one element.

* No C-style dereferencing. `*myPointer` does not work. `myPointer[0]` is the SEABASS way to do it.

## Is the language standardized?

Not yet. The language is planned to be standardized later, when it is more mature.

## Why should I use SEABASS?

at the moment, you probably shouldn't. It's not mature yet. When it is mature, I plan to move the repository

to a dedicated URL, probably SEABASS_LANG or something along those lines.

As for why you would use the final version? Power.

The language's metaprogramming facilities are, as far as I can tell, unmatched among statically compiled programming languages.

Raw power for the programmer, and ease of programming are the top priorities of the project.


## What License is the project under?

Every single jot,tittle,bit,byte, and file in the repository, from the least to the greatest, is under the CC0 license.

You can use everything in this repository and relicense it however you see fit.

No copyleft, no copyright. Just write code!

Let all that your things be done with charity.

If you want me to get paid back for what I have given you, then ask the Lord Jesus Christ to repay me.

Nothing I do is worth anything unless it yields something valuable in eternity. That means being rich toward God.

## May I contribute?

Submit an issue on the repository.

## Current implementation limitations:

* Double must be 8 bytes. No 10-byte floats.

* The parser and validator assume that the target is 64 bit. This is planned to be changed in the future, to allow
defining a target register size. 
Defining non-codegen variables beyond the size should be able to be prohibited by the parser,
and conditional/bitwise expressions will no longer insert implicit casts to `i64` but instead `i32` or `i16` or whatever
the register size is.

* The compiletime VM (which is used for executing `codegen` code) and in general the entire compiler
 requires a 64 bit machine. However, I will write the standard to allow different sizes.

* No 128 bit integers or floats. You can write your own code to manipulate u64's to do what they would do, if you want.






## TODO before alpha release:

[_] - Implement `volatile` qualifier for global variables.

[_] - Implement `atomic` qualifier for global variables.

[_] - Implement limited compatibility with 32 bit targets. At least for non-codegen code.

		(I'm fine with the actual compiler itself being 64-bit only.)

[_] - More `__builtin` functions for interacting with the environment and querying the implementation.

[_] - Implement @ syntax at the global and local scope levels (@myParseHook) to allow codegen code to take over parsing.

[_] - Write more tests.

[_] - Figure out how to do something akin to printf in the language. Probably involves metaprogramming.

[_] - Figure out how this language is going to do conditional compilation.

## Beta plan- implement C backend, port SEABASS to itself.

TODO: write beta TODOs.

## Release plan- write manuals, standard, and example suite.

TODO: write release TODOs.

