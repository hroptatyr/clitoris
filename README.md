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
contents of `FILE`.

Like `make` clitoris breaks upon the first failing command, or if the
output differs (unless `--keep-going` is passed).

To fine-tune what breakage means, clitoris has some built-in operators:

    $ ignore FAILING_COMMAND
    $

will run FAILING_COMMAND but simply ignore the return code and/or the
fact that FAILING_COMMAND's output might differ.

To really have a fine-grained control over what return code should be
considered successful use the built-in `!` (negate) or `?n` (expect)
operators:

     ## succeeds iff COMMAND fails
     $ ! COMMAND
     $

     ## succeeds iff COMMAND returns with code 2
     $ ?2 COMMAND
     $
