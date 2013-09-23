clitoris
========

A command-line tool to run automated tests for command-line
applications.  Much like [expect][1] but with a syntax that actually
resembles the command-lines you'd be otherwise executing in the shell.

Syntax
======

Syntax of clit files:

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

The ignore operator as introduced in the Syntax section above will still
compare the output to the reference and print a diff.  To gain a more
subtle control over what appears in the output use the (make-inspired)
`@` built-in afront the command to bypass the diffing altogether (this
implies `ignore-output` of course).

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

