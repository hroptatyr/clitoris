---
title: clitoris
project: clitoris
layout: project
latest: https://github.com/hroptatyr/clitoris/releases/download/v0.2.2/clitoris-0.2.2.tar.xz
---

clitoris
========

A command-line tool to run automated tests for command-line
applications.  Much like [expect][1] but with a syntax that actually
resembles the command-lines you'd be otherwise executing in the shell.

Why?
----
Code snippets for command-line tools of the form below appear quite
frequently; in the docs, in bug reports, on Q&A sites, in examples on
blogs, etc.  They are easy to grasp, easy to produce and easily
interchangeable via copy'n'paste.

The idea to somehow parse those snippets and evaluate them automatically
isn't far-fetched.  This toolchain does exactly that, be it with a
slightly less free syntax, and turns those snippets into regression
tests.

How?
----
Tests are organised in files.  One test, one file.  The basic syntax is:

    $ COMMANDLINE
    [EXPECTED OUTPUT]
    $

where `COMMANDLINE` is anything you'd normally type in the shell.
And `EXPECTED OUTPUT` is either free text that would normally appear on
stdout, or of the form

    < FILE

which means the output of the test run is to be compared with the
contents of `FILE`.  This is useful when the reference output is already
in a file, or is otherwise generated and stored in file form.

The concluding `$` on its own demarks the end of the test file.
Anything beyond will be blissfully ignored.  Also anything before the
initial `$` line will be ignored.

One test file can issue more than one command line.  They will be
processed from top to bottom, just like in a real shell:

    $ COMMANDLINE1
    [OUTPUT1]
    $ COMMANDLINE2
    [OUTPUT2]
    $

and the whole test file is considered passing when every single command
line returns successfully.

While therefore technically it's possible to stuff several regressions
into one file, the idea was rather to allow for preparation and clean-up
work in the same file (where it actually belongs).

Practical examples
------------------
The following example, when run through clitoris, would produce a return
code of 0 and no output:

    $ echo "2 + 3 = $((2 + 3))"
    2 + 3 = 5
    $

whereas

    $ echo -n | md5sum
    68b329da9893e34099c7d8ad5cb9c940  -
    $

would produce a return code of 1 and the following output:

    --- expected
    +++ actual
    @@ -1 +1 @@
    -68b329da9893e34099c7d8ad5cb9c940  -
    +d41d8cd98f00b204e9800998ecf8427e  -

The diff-like output is only presented in case the output differs (much
like many `diff(1)` tools).  If run in test harnesses (like the automake
parallel tests), this instantly provides valuable feedback as to what
happened and what the author intended to happen.

If the `< FILE` syntax is chosen the diff output, if any, is of course
with respect to the contents `FILE`:

    $ cat shopping
    butter
    cheese
    marmite
    white bread
    $ sort shopping
    < shopping
    $

Return code would be 0 and no output, as the file `shopping` is already
sorted.  However,

    $ cat shopping
    butter
    cheese
    marmite
    brown bread
    $ sort shopping
    < shopping
    $

would return 1 and produce:

    --- expected
    +++ actual
    @@ -1,4 +1,4 @@
    +brown bread
     butter
     cheese
     marmite
    -brown bread

as now the shopping list isn't in sorted order.


Controlling behaviour
=====================

Like `make` clitoris breaks upon the first failing command, or if the
output differs (unless `--keep-going` is passed).

To fine-tune what breakage means, clitoris has some built-in operators:

    $ ignore FAILING_COMMAND
    $

will run FAILING_COMMAND but simply ignore the return code and/or the
fact that FAILING_COMMAND's output might differ.  Alternatively, if only
one of the two is meant to be suppressed `ignore-return` or
`ignore-output` should be used.

To really have a fine-grained control over what return code should be
considered successful use the built-in `!` (negate) or `?n` (expect)
operators:

     ## succeeds iff COMMAND fails
     $ ! COMMAND
     $

     ## succeeds iff COMMAND returns with code 2
     $ ?2 COMMAND
     $

Controlling output
==================

The ignore operator as introduced in the Controlling behaviour section
above will still compare the output to the reference and print a diff.
To gain a more subtle control over what appears in the output use the
(make-inspired) `@` built-in afront the command to bypass the diffing
altogether (this implies `ignore-output` of course).

A fabulous usecase is the included `hxdiff(1)` tool which itself prints
a diff (of the hexdumps of two files), and thus avoids the confusion of
a diff-of-diffs.

Imagine a binary file produced in the process of the regression test and
a reference file that contains zeroes only:

    $ @ hxdiff reference.bin actual.bin
    --- expected
    +++ actual
    @@ -1,2 +1,2 @@
     00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00   |................|
    -00000010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00   |................|
    +00000010  00 01 00 00 00 00 00 00  00 00 00 00 00 00 00 00   |................|

There is one more operator to aid generated prototypical output (the
stuff that is compared against the dynamic output).  For instance, if
the prototypical output, for some reason, has to contain values that
change from run to run, user to user, or machine to machine (e.g. the
current date, a user's home directory, or the hostname) you'll have no
choice but to materialise these beforehand into a file and use that file
as proto-output, as in:

    $ echo "${HOST}" > hn.file
    $ hostname-output-tool
    < hn.file
    $ -rm hn.file
    $

It would be easier if the proto-output could be expanded somehow like
POSIX shell here-documents.  This is where the expander built-in (`$`)
comes in handy, now you can write:

    $ $ hostname-output-tool
    ${HOST}
    $

and even though this looks slightly ugly it saves the hassle of
temporary files (along with races should test files be executed in
parallel) and pre-test/post-test actions that don't actually test
functionality.

The proto-output of a test that is flagged with the expander built-in
(`$`) is treatest as though you passed a here-document to the diff
process.


  [1]: http://expect.sourceforge.net/
