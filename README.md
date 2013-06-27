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
output differs.
