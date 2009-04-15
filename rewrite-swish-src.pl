#!/usr/bin/perl -w 
#
# converts swish-e source code to all 64-bit integers.
# --> in progress.
# Copyright 2007-2009 Josh Rabinowitz


use strict;
use Getopt::Long; 
use File::Basename;
use File::Slurp qw(read_file write_file);
use List::MoreUtils qw(any);

my $prog = basename($0);
my $verbose;
my $debug;
my $refresh;

# Usage() : returns usage information
sub Usage {
    "$prog [--verbose] [--debug] [--refresh]\n" . 
    "  Tries to convert source code to 64-bit friendly.\n" .
    "  Expects to be run the base SVN directory of a swish-e checkout.\n" .
    "  --refresh removes and repulls files matching src/*.[ch] before rewrite\n";
}

# call main()
main();

# main()
sub main {
    GetOptions(
        "verbose!" => \$verbose,
        "debug!" => \$debug,
        "refresh!" => \$refresh,
    ) or die Usage();
     
     $|++;

     my @files = glob( "src/*.c src/*.h");
     #my @files = glob( "src/*.c src/*.h src/*/*.c src/*/*.h src/*/*/*.h src/*/*/*.c");
     if ($refresh) {
         system( "rm -f @files" );
         system( "svn up" ); 
     }
     # reglob since we might have deleted and refetched files.
     @files = glob( "src/*.c src/*.h");     # don't try to handle perl/API.xs yet


     # alter swish.h to contain SWINT_T and SWUINT_T typedefs.
     # lines with no_rw don't have replacements done by @regexes below.
     print "$prog: Inserting lines into files...\n";
     my @inserts = (
        # in file,       line,  unless file contains,   insert at mentioned line
        #[ "src/swish.h",   78, 'typedef.*SWINT_T', qq{typedef long long SWINT_T; // no rw64 \ntypedef unsigned long long SWUINT_T; // no rw64 \n}, ],
        [ "src/swish-e.h", 35, 'swishtypes\.h',         qq{#include "swishtypes.h"\n}, ],
        [ "src/swish.h",   78, 'swishtypes\.h',         qq{#include "swishtypes.h"\n}, ],
        [ "src/stemmer.h", 35, 'swishtypes\.h',         qq{#include "swishtypes.h"\n}, ],
        [ "src/mem.h",     53, 'swishtypes\.h',         qq{#include "swishtypes.h"\n}, ],
        [ "src/libtest.c", 43, 'swishtypes\.h',         qq{#include "swishtypes.h"\n}, ],
            # force crash if num goes large in compress3()
        [ "src/compress.c",147, 'abort',           '   if (num > 10000000) {printf(" in compress3: num is %lld\n", num ); abort(); } ' . "\n", ], 
        #[ "perl/API.xs",    8,  'swishtypes\.h',   qq{#include "../src/swishtypes.h"\n}, ],
     );
     for my $insert (@inserts) {
         insert_at_line_unless_has_regex( @$insert );
     }

     my @regexes = (
        #  replace everything that looks anything like a 'long' or an 'int'.
        #  specifically leave alone anything about chars or floats/doubles.
        q(s/ \bunsigned\s+long\s+long\s+int\b     /SWUINT_T/gx),
        q(s/ \blong\s+long\s+unsigned\s+int\b     /SWUINT_T/gx),
        q{s/ \bunsigned\s+long\s+int\b /SWUINT_T/gx},
        q{s/ \bunsigned\s+long\b       /SWUINT_T/gx},
        q(s/ \bunsigned\s+int\b        /SWUINT_T/gx),
        q(s/ \blong\s+long\s+int\b     /SWINT_T/gx),
        q(s/ \blong\s+long\b           /SWINT_T/gx),
        q(s/ \blong\s+int\b            /SWINT_T/gx),
        q(s/ \blong\b                  /SWINT_T/gx),
        q(s/ \bint\b                   /SWINT_T/gx),

        # format strings. All get converted to 'long long' versions.
        q{s/ %d                        /%lld/gx },
        q{s/ %ld                       /%lld/gx },
        q{s/ %lu                       /%llu/gx },
        q{s/ %(\d+)d                   /%$1lld/gx },
        q{s/ %x                        /%llx/gx   },
        q{s/ %lx                       /%llux/gx },
        q{s/ %lX                       /%llux/gx },
        q{s/ %(\d+)X                   /%$1llX/gx },

        # remove SET_POSDATA and GET_POSITION macros from swish.h
        # if you do this, make sure to alter files to include swishtypes.h as needed.
        # I think this was a wild goose chase.  What would be useful for our purposes
        # is a unit test for compress.c.
        #q{s/ #define\s+SET_POSDATA.*   //gx },
        #q{s/ #define\s+GET_POSITION.*   //gx },
     );
     my @exceptions  = (
         # don't replace on lines matching this fileregex and lineregex
         { f=>'\.c$',          s=>'int\s+main' },           # never replace main's return val
         { f=>'src/http\.c$',  s=>'int\s+status' },         # status must be int for wait()
         #{ f=>'',              s=>'waitpid\s*\(' },         # leave waitpid lines alone
         { f=>"",              s=>'^\s+extern.*printf' },   # leave return codes of exern printfs alone.
         { f=>'src/compress\.[ch]$', s=>'(void|int)\s+(un)?compress.*\*f_(putc|getc)' },   # don't change f_getc/f_putc def
         { f=>"",              s=>'int.*_(put|get)c' },    # preserve anything that looks like 'getc/putc'
         { f=>"",              s=>'_(put|get)c.*int' },    # preserve anything that looks like 'getc/putc'
     );

     my @replacements = (
         #  in File,               search for,                replace with
         { f=>'src/swish_qsort.c', s=>'#include <stdlib\.h>', r=>'#include "swish.h"' },
     );
     for my $file (@files) {
         #print "$file\n";
         my @file_exceptions = grep { $file =~ /$_->{f}/ } @exceptions;
         rewrite_file( $file, \@regexes, \@file_exceptions, \@replacements );
     }
}

#================================================================
# rewrite_file( $file, @search_and_replace_regexes )
# backs up $file to $file.bak, and
# applies supplied regexes to the lines of a file,
sub rewrite_file {
     my ($file, $regexes, $exceptions, $replacements) = @_;
     # changes a file by applying the supplied regexes to each line
     my $tmpfile = "$file.tmp";
     open(my $rfh, "<", $file)    || die "$0: Can't open $file: $!";
     open(my $wfh, ">", $tmpfile) || die "$0: Can't open $tmpfile: $!";  # clobber old $file.tmp
     print "$prog: Applying regexes to file $file\n";
     if ($debug) {
         print join("\n", map { "regex: $_" } @$regexes) . "\n";
     }
     LINE: while(<$rfh>) {
         chomp();
         if (m{//.*no rw64} || m{/\*.*no rw64}) {   # if it has a comment with 'no rw64'...
             print "$prog: Preserving line '$_' from $file\n";
             print $wfh "$_\n";
             next LINE;
         }
         for my $e (@$exceptions) { # we only get ones relevant to our file
             #if ($file =~ m/$e->{f}/ && $_ =~ m/$e->{s}/) { # if the file and the regex match...
			 if ($_ =~ m/$e->{s}/) { # if the file and the regex match...
				 print "$prog: Preserving line '$_' from $file\n";
				 print $wfh "$_\n";
				 next LINE;
			 }
		 }
		 for my $e (@$replacements) {
			 if ($file =~ m/$e->{f}/) {                     # if the file matches...
                 s/$e->{s}/$e->{r}/g;                       # do the search&replace, and continue.
             }
         }
         for my $r (@$regexes) {
             # $r should operate on $_ !
             eval $r;   # operates on $_, which is the current line
             die "$0: Error in regex: $r: $@" if $@;
         }
         print $wfh "$_\n";
     }
     close($rfh) || die "$0: Can't open $file: $!";
     close($wfh) || die "$0: Can't close $tmpfile: $!";
     rename( $file, "$file.bak" );
     rename( $tmpfile, $file ) || die "$0: Can't rename $tmpfile to $file: $!";
}


sub insert_at_line_unless_has_regex {
    my ($filename, $linenum, $regex, @addlines) = @_;
    die "$prog: file does not exist: $filename\n" unless -e $filename;
    die "$prog: file is empty: $filename\n" unless -s $filename;
    my @lines = read_file( $filename );
    if( any { m/$regex/ } @lines ) { 
        print "$prog: not adding line(s) to $filename, contains $regex\n" if $verbose;
        return 0;     # has regex, leave alone
    }
    print "$prog: inserting lines into $filename\n";
    splice( @lines, $linenum-1, 0, @addlines );     # otherwise insert lines...
    write_file( $filename, @lines );                # and rewrite file
}

=pod

=head1 NAME

rewrite-swish-src.pl -- tries to rewrite swish-e source to be portable

=head1 SYNOPSIS

This script converts a swish-e source code tree to be 64bit friendly,
mostly by replacing various int types with known 64bit (or possibly larger) 
versions.

It can be used like so:

   % svn co http://svn.swish-e.org/swish-e/trunk swish-e-portable
       (this fetches trunk into swish-e-portable/ )
   % cd swish-e-portable
   % ./configure
   % ./rewrite-swish-src.pl
   % make

It's intended that we'll make the 64bit changes to rewrite-swish-src.pl, not to the source
tree directly. So after you've made some changes to rewrite-swish-src.pl, you can retest your
changes with:

   % ./rewrite-swish-src.pl -refresh
       (removes src/*.c and src/*.h, refetches them from svn, and rewrites them)
   % make 

Also, note that to turn compiler optimization off, and debug symbols on, 
use configure this way:
   
   % ./configure CFLAGS="-IO0 -g'
     (that's an O letter and a zero).  

Note that any line with a comment matching the regex m{# no rw64} or m{/\* no rw64 .*}
is passed through unaltered. So you can add that comment to lines to have their ints
preserved.

=head1 DESCRIPTION

rewrites swish-e source to be portable between 32bit and 64bit architectures.

=head1 OPTIONS

Overall view of the options.

=over 4

=item --verbose/--noverbose

Turns on/off verbose mode. (off by default)

=item --debug/--nodebug

Turns on/off debug mode. (off by default)

=item --refresh/--norefresh

Turns on/off refresh mode, which deletes all files matching src/*.c and src/*.h, and
refetches them from SVN before performing 64bit alterations. (off by default)

=back

=head1 TO DO

Note that while this will make indexes portable between current (linux) 32bit and 64bit 
architectures, it's possible that some systems will implement a 
'long long' type larger than 64bits  and thus not produce binary 
compatible indexes with 64bit int systems. It's intended that this
be handled in a later revision.

=head1 COPYRIGHT

Copyright (c) 2009 Josh Rabinowitz, All Rights Reserved.

=head1 AUTHORS

Josh Rabinowitz

=cut    

