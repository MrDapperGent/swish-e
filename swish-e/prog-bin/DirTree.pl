#!/usr/local/bin/perl -w
use strict;

# This is a short example that basically does the same
# thing as the default file system access method by
# recursing directories, but also shows how to process different
# file types -- in this example pdf is converted to xml for indexing.

use File::Find;  # for recursing a directory tree
use pdf2xml;     # example module for pdf to xml conversion
                 # Not that you need IndexContents XML .pdf in the
                 # swish-e config file

# See perldoc File::Find for information on following symbolic links
# and other important topics.

use constant DEBUG => 0;


my $dir = shift || '.';

find(
    {
        wanted => \&wanted,
        no_chdir => 1,
    },
    $dir,
);

sub wanted {
    return if -d;

    if ( /\.pdf$/ ) {
        print STDERR "Indexing pdf $File::Find::name\n" if DEBUG;
        print ${ pdf2xml( $File::Find::name ) };

    } elsif ( /\.(txt|log|pl|html|htm)$/ ) {
        print STDERR "Indexing $File::Find::name\n" if DEBUG;
        print ${ get_content( $File::Find::name ) };

    } else {
        print STDERR "Skipping $File::Find::name\n" if DEBUG;
    }
}


sub get_content {
    my $path = shift;

    my ( $size, $mtime )  = (stat $path )[7,9];
    open FH, $path or die "$path: $!";

    my $content =  <<EOF;
Content-Length: $size
Last-Mtime: $mtime
Path-Name: $path

EOF
    local $/ = undef;
    $content .= <FH>;
    return \$content;
}

