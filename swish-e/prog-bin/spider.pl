#!/usr/local/bin/perl -w
use strict;

# $Id$
#
# "prog" document source for spidering web servers
#
# For documentation, type:
#
#       perldoc spider.pl
#
# Apr 7, 2001 -- added quite a bit of bulk for easier debugging


use LWP::RobotUA;
use HTML::LinkExtor;

use vars '$VERSION';
$VERSION = sprintf '%d.%02d', q$Revision$ =~ /: (\d+)\.(\d+)/;

use constant DEBUG_ERRORS   => 1;  # program errors
use constant DEBUG_URL      => 2;  # print out every URL processes
use constant DEBUG_FAILED   => 3;  # failed to return a 200
use constant DEBUG_SKIPPED  => 4;  # didn't index for some reason
use constant DEBUG_INFO     => 5;  # more verbose
use constant DEBUG_LINKS    => 6;  # prints links as they are extracted




    
#-----------------------------------------------------------------------

    use vars '@servers';

    my $config = shift || 'SwishSpiderConfig.pl';
    do $config or die "Failed to read $0 configuration parameters '$config' $!";

    print STDERR "$0: Reading parameters from '$config'\n";

    my $abort;
    local $SIG{HUP} = sub { $abort++ };

    my $depth = 0;
    my %visited;  # global -- I suppose would be smarter to localize it per server.

    process_server($_) for @servers;


#-----------------------------------------------------------------------


sub process_server {
    my $server = shift;

    # set defaults

    $server->{debug} ||= 0;
    $server->{debug} = 0 unless $server->{debug} =~ /^\d+$/;

    $server->{max_size} ||= 1_000_000;
    $server->{max_size} = 1_000_000 unless $server->{max_size} =~ /^\d+$/;

    $server->{link_tags} = ['a'] unless ref $server->{link_tags} eq 'ARRAY';
    my %seen;
    $server->{link_tags} = [ grep { !$seen{$_}++} map { lc } @{$server->{link_tags}} ];

    for ( qw/ test_url test_response filter_content / ) {
        next unless $server->{$_};
        $server->{$_} = [ $server->{$_} ] unless ref $server->{$_} eq 'ARRAY';
        my $n;
        for my $sub ( @{$server->{$_}} ) {
            $n++;
            die "Entry number $n in $_ is not a code reference\n" unless ref $sub eq 'CODE';
        }
    }
    

    my $start = time;

    if ( $server->{skip} ) {
        print STDERR "Skipping: $server->{base_url}\n";
        return;
    }

    require "HTTP/Cookies.pm" if $server->{use_cookies};
    require "Digest/MD5.pm" if $server->{use_md5};
            

    # set starting URL, and remove any specified fragment
    my $uri = URI->new( $server->{base_url} );
    $uri->fragment(undef);

    

    # set the starting server name (including port) -- will only spider on server:port
    
    $server->{authority} = $uri->authority;
    $server->{same} = [ $uri->authority ];
    push @{$server->{same}}, @{$server->{same_hosts}} if ref $server->{same_hosts};


    # set time to end
    
    $server->{max_time} = $server->{max_time} * 60 + time
        if $server->{max_time};


    # set default agent for log files

    $server->{agent} ||= 'swish-e spider 2.2 http://sunsite.berkeley.edu/SWISH-E/';



    # get a user agent object
    
    my $ua;
    if ( $server->{ignore_robots_file} ) {
        $ua = LWP::UserAgent->new();
        return unless $ua;
        $ua->agent( $server->{agent} );
        $ua->from( $server->{email} );

    } else {
        $ua = LWP::RobotUA->new( $server->{agent}, $server->{email} );
        return unless $ua;
        $ua->delay( $server->{delay_min} || 0.1 );
    }
        
    $server->{ua} = $ua;  # save it for fun.


    $ua->cookie_jar( HTTP::Cookies->new ) if $server->{use_cookies};



    eval {
        process_link( $server, $uri );
    };
    print STDERR $@ if $@;

    $start = time - $start;
    $start++ unless $start;

    my $max_width = 0;
    for ( keys %{$server->{counts}} ) {
        $max_width = length if length > $max_width;
    }

    printf STDERR "\n$0: Summary for: $server->{base_url}\n";

    for ( sort keys %{$server->{counts}} ) {
        printf STDERR "%${max_width}s: %6d  (%0.1f/sec)\n",
            $_,
            $server->{counts}{$_},
            $server->{counts}{$_}/$start;
    }
}    
        

my $parent;
#----------- Process a url and recurse -----------------------
sub process_link {
    my ( $server, $uri ) = @_;

    die if $abort;


    thin_dots( $uri, $server ) if $server->{thin_dots};


    if ( $visited{ $uri->canonical }++ ) {
        $server->{counts}{Skipped}++;
        $server->{counts}{Duplicates}++;
        return;
    }

    $server->{counts}{'Unique URLs'}++;

    die "$0: Max files Reached\n"
        if $server->{max_files} && $server->{counts}{'Unique URLs'} > $server->{max_files};

    die "$0: Time Limit Exceeded\n"
        if $server->{max_time} && $server->{max_time} < time;


    return unless check_user_function( 'test_url', $uri, $server );

    

    # make request
    my $ua = $server->{ua};
    my $request = HTTP::Request->new('GET', $uri );

    my $content = '';

    $server->{no_contents} = 0;

    my $first;
    my $callback = sub {
        unless ( $first++ ) {
            die "skipped\n" unless check_user_function( 'test_response', $uri, $server, $_[1], \$_[0]  );
        }

        if ( length( $content ) + length( $_[0] ) > $server->{max_size} ) {
            print STDERR "-Skipped $uri: Document exceeded $server->{max_size} bytes\n" if $server->{debug} >= DEBUG_ERRORS;
            die "too big!\n";
        }

        $content .= $_[0];

    };

    my $response = $ua->simple_request( $request, $callback, 4096 );



    # skip excluded by robots.txt
    
    if ( !$response->is_success && $response->status_line =~ 'robots.txt' ) {
        print STDERR "-Skipped $depth $uri: ", $response->status_line,"\n" if $server->{debug} >= DEBUG_SKIPPED;
        $server->{counts}{'robots.txt'}++;
        return;
    }



    if ( ( $server->{debug} >= DEBUG_URL ) || ( $server->{debug} >= DEBUG_FAILED && !$response->is_success)  ) {
        print STDERR '>> ',
          join( ' ',
                ( $response->is_success ? '+Fetched' : '-Failed' ),
                $depth,
                $response->request->uri->canonical,
                $response->code,
                $response->content_type,
                $response->content_length,
           ),"\n";

        if ( !$response->is_success && $parent ) {
            print STDERR "   Found on page: ", $parent,"\n";
        }
           
    }

    unless ( $response->is_success ) {

        # look for redirect
        if ( $response->is_redirect && $response->header('location') ) {
            my $u = URI->new_abs( $response->header('location'), $response->base );
            process_link( $server, $u );
        }
        return;
    }

    return unless $content;  # $$$ any reason to index empty files?
    

    # make sure content is unique - probably better to chunk into an MD5 object above

    if ( $server->{use_md5} ) {
        my $digest =  Digest::MD5::md5($content);

        if ( $visited{ $digest } ) {

            print STDERR "-Skipped $uri has same digest as $visited{ $digest }\n"
                if $server->{debug} >= DEBUG_SKIPPED;
        
            $server->{counts}{Skipped}++;
            $server->{counts}{'MD5 Duplicates'}++;
            return;
        }
        $visited{ $digest } = $uri;
    }
    


    return unless check_user_function( 'filter_content', $uri, $server, $response, \$content );

    output_content( $server, \$content, $response );

    $server->{Spidered}++;
    my $links = extract_links( $server, \$content, $response );

    # Now spider
    my $last_page = $parent || '';
    $parent = $uri;
    $depth++;
    process_link( $server, $_ ) for @$links;
    $depth--;
    $parent = $last_page;

}


sub check_user_function {
    my ( $fn, $uri, $server ) = ( shift, shift, shift );
    
    return 1 unless $server->{$fn};

    my $tests = ref $server->{$fn} eq 'ARRAY' ? $server->{$fn} : [ $server->{$fn} ];

    my $cnt;

    for my $sub ( @$tests ) {
        $cnt++;
        print STDERR "?Testing '$fn' user supplied function #$cnt\n" if $server->{debug} >= DEBUG_INFO;

        my $ret = $sub->( $uri, $server, @_ );
        next if $ret;
        
        print STDERR "-Skipped $uri due to '$fn' user supplied function #$cnt\n" if $server->{debug} >= DEBUG_SKIPPED;
        $server->{counts}{Skipped}++;
        return;
    }
    print STDERR "+Passed all $cnt tests for '$fn' user supplied function\n" if $server->{debug} >= DEBUG_INFO;
    return 1;
}

    
sub extract_links {
    my ( $server, $content, $response ) = @_;

    return [] unless $response->header('content-type') &&
                     $response->header('content-type') =~ m[^text/html];

    my @links;


    my $base = $response->base;

    my $p = HTML::LinkExtor->new;
    $p->parse( $$content );

    my %skipped_tags;
    for ( $p->links ) {
        my ( $tag, %attr ) = @$_;

        # which tags to use ( not reported in debug )

        print STDERR " ?? Looking at extracted tag '$tag'\n" if $server->{debug} >= DEBUG_LINKS;
        
        unless ( grep { $tag eq $_ } @{$server->{link_tags}} ) {

            # each tag is reported only once per page
            print STDERR
                " ?? <$tag> skipped because not one of (",
                join( ',', @{$server->{link_tags}} ),
                ")\n"
                     if $server->{debug} >= DEBUG_LINKS && !$skipped_tags{$tag}++;
                     
            next;
        }
        

        my $links = $HTML::Tagset::linkElements{$tag};
        $links = [$links] unless ref $links;

        my $found;
        my %seen;
        for ( @$links ) {
            if ( $attr{ $_ } ) {  # ok tag

                my $u = URI->new_abs( $attr{$_},$base );
                $u->fragment( undef );

                return if $seen{$u}++;  # this won't report duplicates

                unless ( $u->scheme =~ /^http$/ ) {  # no https at this time.
                    print STDERR qq[ ?? <$tag $_="$u"> skipped because not http\n] if $server->{debug} >= DEBUG_LINKS;
                    next;
                }

                unless ( $u->host ) {
                    print STDERR qq[ ?? <$tag $_="$u"> skipped because not no host name\n] if $server->{debug} >= DEBUG_LINKS;
                    next;
                }

                unless ( grep { $u->authority eq $_ } @{$server->{same}} ) {
                    print STDERR qq[ ?? <$tag $_="$u"> skipped because different authority (server:port)\n] if $server->{debug} >= DEBUG_LINKS;
                    $server->{counts}{'Off-site links'}++;
                    next;
                }
                
                $u->authority( $server->{authority} );  # Force all the same host name

                push @links, $u;
                print STDERR qq[ ++ <$tag $_="$u"> Added to list of links to follow\n] if $server->{debug} >= DEBUG_LINKS;
                $found++;

            }
        }

        if ( !$found && $server->{debug} >= DEBUG_LINKS ) {
            my $s = "<$tag";
            $s .= ' ' . qq[$_="$attr{$_}"] for sort keys %attr;
            $s .= '>';
                
            print STDERR " ?? tag $s did not include any links to follow\n";
        }
        
    }

    print STDERR "! Found ", scalar @links, " links in ", $response->base, "\n" if $server->{debug} >= DEBUG_INFO;


    return \@links;
}

sub output_content {
    my ( $server, $content, $response ) = @_;

    $server->{indexed}++;


    my $headers = join "\n",
        'Path-Name: ' .  $response->request->uri,
        'Content-Length: ' . length $$content,
        '';

    $headers .= 'Last-Mtime: ' . $response->last_modified . "\n"
        if $response->last_modified;


    $headers .= "No-Contents: 1\n" if $server->{no_contents};
    print "$headers\n$$content";
}
        

# This is not very good...

sub thin_dots {
    my ( $uri, $server ) = shift;


    my $p = $uri->path;

    my $c = 0;
    my @new;

    my $trailing_slash = substr( $p, -1, 1 ) eq '/';

    foreach ( split m[/+], $p ) {
        /^\.\.$/ && do {
            pop @new;
            $c--;
            next;
        };

            $new[$c++] = $_;
    }
    $uri->path( join '/', @new );

    $uri->path( $uri->path . '/' ) if $trailing_slash;  # cough, hack, cough.

    if ( $server->{debug} >= DEBUG_INFO && $p ne $uri->path ) {
        print STDERR "thin_dots: $p -> ", $uri->path, "\n";
    }
}

__END__

=head1 NAME

spider.pl - Example Perl program to spider web servers

=head1 SYNOPSIS

 ~/swish-e/docsource > ../src/swish-e -S prog -i ./spider.pl

=head1 DESCRIPTION

This is a swish-e "prog" document source program for spidering
web servers.  It can be used instead of the C<http> method for
spidering with swish.

You define "servers" to spider, set a few parameters,
create callback routines, and start indexing as the synopsis above shows.
All this setup is currently done in a loaded configuration file.
The associated example is called C<SwishSpiderConfig.pl> and is
included in this C<prog-bin> directory along with this script.

The available configuration parameters are discussed below.

This script will not spider files blocked by robots.txt, unless
explicitly overridden -- something you probably should not do.
This script only spiders one file at a time, so load on the web server is not that great.
Still, discuss spidering with a site's administrator before beginning.

The spider program keeps track of URLs visited, so a file is only indexed
one time.  There are two features that can be enabled (disabled by default)
to try to avoid indexing the same file that can be found by different URLs.

First, the C<thin_dots> feature tries to make the following URLs the same by
removing the dots:

    http://localhost/path/to/some/file.html
    http://localhost/path/to/../to/some/file.html

Second, the Digest::MD5 module can be used to create a "fingerprint" of every page
indexed and this fingerprint is used in a hash to find duplicate pages.
For example, MD5 will prevent indexing these as two different documents:

    http://localhost/path/to/some/index.html
    http://localhost/path/to/some/

But note that this may have side effects you don't want.  If you want this
file indexed under this URL:

    http://localhost/important.html

But the spider happens to find the exact content in this file first:

    http://localhost/developement/test/todo/maybeimportant.html

Then only that URL will be indexed.    

MD5 may slow down indexing a tiny bit, so test with and without if speed is an
issue (which it probably isn't since you are spidering in the first place).

Note: the "prog" document source in swish bypasses many configuration settings.
For example, you cannot use the C<IndexOnly> directive with the "prog" document
source.  This is by design to limit the overhead when using an external program
for providing documents to swish; it's assumed that the program can filter
as well as swish can internally.

So, for spidering, if you do not wish to index images, for example, you will
need to either filter by the URL or by the content-type returned from the web
server.  See L<CALLBACK FUNCTIONS|CALLBACK FUNCTIONS> below for more information.

=head1 REQUIREMENTS

Perl 5 (hopefully 5.00503) or later.

You must have the LWP Bundle on your computer.  Load the LWP::Bundle via the CPAN.pm shell,
or download libwww-perl-x.xx from CPAN (or via ActiveState's ppm utility).
Also required is the the HTML-Parser-x.xx bundle of modules also from CPAN
(and from ActiveState for Windows).

You will also need Digest::MD5 if you wish to use the MD5 feature.


=head1 CONFIGURATION FILE

Configuration is not very fancy.  The spider.pl program simply does a
C<do "path";> to read in the parameters and create the callback subroutines.
The C<path> is the first parameter passed to the script, which is set
by the Swish-e configuration setting C<SwishProgParameters>.

For example, if in your swish-e configuration file you have

    SwishProgParameters /path/to/config.pl
    IndexDir /home/moseley/swish-e/prog-bin/spider.pl

And then run swish as

    swish-e -c swish.config -S prog

swish will run C</home/moseley/swish-e/prog-bin/spider.pl> and the spider.pl
program will receive as its first parameter C</path/to/config.pl>, and
spider.pl will read C</path/to/config.pl> to get the spider configuration
settings.  If C<SwishProgParameters> is not set, the program will try to
use C<SwishSpiderConfig.pl>.

The configuration file must set a global variable C<@servers> (in package main).
Each element in C<@servers> is a reference to a hash.  The elements of the has
are described next.  More than one server hash may be defined -- each server
will be spidered in order listed in C<@servers>, although currently a I<global> hash is
used to prevent spidering the same URL twice.

Examples:

    my %serverA = (
        base_url    => 'http://sunsite.berkeley.edu/SWISH-E/',
        same_hosts  => [ qw/www.sunsite.berkeley.edu/ ],
        email       => 'my@email.address',
    );
    @servers = ( \%serverA, \%serverB, );

=head1 CONFIGURATION OPTIONS

This describes the required and optional keys in the server configuration hash.

=over 4

=item base_url

This required setting is the starting URL for spidering.

=item same_hosts

This optional key sets equivalent B<authority> name(s) for the site you are spidering.
For example, if your site is C<www.mysite.edu> but also can be reached by
C<mysite.edu> (with or without C<www>) and also C<web.mysite.edu> then:


Example:

    $serverA{base_url} = 'http://www.mysite.edu/index.html';
    $serverA{same_hosts} = ['mysite.edu', 'web.mysite.edu'];

Now, if a link is found while spidering of:

    http://web.mysite.edu/path/to/file.html

it will be considered on the same site, and will actually spidered and indexed
as:

    http://www.mysite.edu/path/to/file.html

Note: This should probably be called B<same_authority> because it compares the URI C<authority>
against the list of host names in C<same_hosts>.  So, if you specify a port name in you will
probably want to specify the port name in the the list of hosts in C<same_hosts>:

    my %serverA = (
        base_url    => 'http://sunsite.berkeley.edu:4444/',
        same_hosts  => [ qw/www.sunsite.berkeley.edu:4444/ ],
        email       => 'my@email.address',
    );


=item email

This required key sets the email address for the spider.  Set this to
your email address.

=item agent

This optional key sets the name of the spider.

=item link_tags

This optional tag is a reference to an array of tags.  Only links found in these tags will be extracted.
The default is to only extract links from C<a> tags.

For example, to extract tags from C<a> tags and from C<frame> tags:

    my %serverA = (
        base_url    => 'http://sunsite.berkeley.edu:4444/',
        same_hosts  => [ qw/www.sunsite.berkeley.edu:4444/ ],
        email       => 'my@email.address',
        link_tags   => [qw/ a frame /],
    );


=item delay_min

This optional key sets the delay in minutes to wait between requests.  See the
LWP::RobotUA man page for more information.  The default is 0.1 (6 seconds),
but in general you will probably want it much smaller.  But, check with
the webmaster before using too small a number.

To Do: how to completely disable and spider as fast as possible?

=item max_time

This optional key will set the max minutes to spider.   Spidering
for this host will stop after C<max_time> minutes, and move on to the
next server, if any.  The default is to not limit by time.

=item max_files

This optional key sets the max number of files to spider before aborting.
The default is to not limit by number of files.

=item max_size

This optional key sets the max size of a file read from the web server.
This B<defaults> to 1,000,000 bytes.  If the size is exceeded the resource is
skipped (and a message is written to STDERR if debug level is > 1.

=item skip

This optional key can be used to skip the current server.  It's only purpose
is to make it easy to disable a server in a configuration file.

=item debug

Set this to a number to display different amounts of info while spidering.  Writes info
to STDOUT.  Zero is not debug output.  The higher the number, the more verbose the output.
The levels are inclusive, so debug level 3 includes debug levels 1 and 2.

Here are basically the levels:

    0   => no debug output
    1   => program errors (not currently used)
    2   => report every URL processed
    3   => request failed from remote server (doesn't included skipped due to robots.txt)
    4   => report when a URL is skipped
    5   => report misc info
    6   => report info on links as they are extracted


=item ignore_robots_file

If this is set to true then the robots.txt file will not be checked when spidering
this server.  Don't use this option unless you know what you are doing.

=item thin_dots

If this optional setting is true then the spider will attempt to remove dots from
URLs.  For example,

    http://localhost/path/to/../to/../to/my/file.html

will be indexed and spidered as:

    http://localhost/path/to/my/file.html

This should prevent relative URLs in A href tags from looking like different
pages, when they point to the same document.

=item use_cookies

If this is set then a "cookie jar" will be maintained while spidering.  Some
(poorly written ;) sites require cookies to be enabled on clients.

This requires the HTTP::Cookies module.

=item use_md5

If this setting is true, then a MD5 digest "fingerprint" will be made from the content of every
spidered document.  This digest number will be used as a hash key to prevent
indexing the same content more than once.  This is helpful if different URLs
generate the same content.

Obvious example is these two documents will only be indexed one time:

    http://localhost/path/to/index.html
    http://localhost/path/to/

This option requires the Digest::MD5 module.  Spidering with this option might
be a tiny bit slower.

=back

=head1 CALLBACK FUNCTIONS

Three callback functions can be defined in your parameter hash.
These optional settings are I<callback> subroutines that are called while
processing URLs.

The first routine allows you to skip processing of urls based on the url before the request
to the server is made.  The second function allows you to filter based on the response from the
remote server (such as by content-type).  And the third function allows you to filter the returned
content from the remote server.


The first two parameters passed are a URI object (to have access to the current URL), and
a reference to the current server hash.  Other parameters may be passes, as described below.

Each callback function B<must> return true to continue processing the URL.  Returning false will
cause processing of this URL to be skipped.

Note that you can create your own counters to display when processing is done by adding a value to
the hash pointed to by C<$server->{counts}>.  See the example below.

=over 4

=item test_url

Test URL is called before a request is made to the server for this URL.
Your callback function is passed two parameters, a URI object and a reference
to the server parameter hash.  If this function returns true the URL will be
requested from the web server.  Returning false will cause the spider to skip this
URL.

This callback function allows you to filter out URLs before taking the time to
request the resource (document, image, whatever) from the web server.  This might
be a good place, for example, to filter out requests for images.

For example:

    $server{test_url} = sub {
        my ( $uri, $server ) = @_;
        return $uri->path !~ /\.(gif|jpeg|jpg|png)$/;
    }

=item test_response

C<test_response> is similar to C<test_url>, but it is called I<after> the URI
has been requested from the web server (and some content has been returned).
In addition to the URI, and server hash passed, the HTTP::Response object is passed as
a third parameter.
Again, returning false will cause the spider to ignore this resource.

The spider requests a document in "chunks" or 4096 bytes.  4096 is only a suggestion
of how many bytes to return in each chunk.  The C<test_response> routine is
called when the first chunk is received only.  This allows ignoring (aborting)
reading of a very large file, for example, without having to read the entire file.
Although not much use, a reference to this chunk is passed as the forth parameter.

This subroutine is useful for filtering documents based on data returned in the
headers of the response.  For example, if you have lots of large images on your
web server, and they cannot be easily identified by the URL (as above), then
you could use this callback subroutine to check the Content-Type: header.

    $server{test_response} = sub {
        my ( $uri, $server, $response ) = @_;
        return $response->content_type !~ m[^image/];
    }

Swish has a configuration directive C<NoContents> that will instruct swish to
index only the file name, and not the contents.  This is often used when
indexing binary files such as image files, but can also be used with html
files to index only the document titles.

You can turn this feature on for specific documents by setting a flag in
the server hash passed into the C<test_response> subroutine:

    $server{test_response} = sub {
        my ( $uri, $server, $response ) = @_;
        $server->{no_contents} =
            $response->content_type =~ m[^image/];
        return 1;  # ok to spider, but pass the index_no_contents to swish
    }

The entire contents of the resource is still read from the web server, and passed
on to swish, but swish will also be passed a C<No-Contents> header which tells
swish to enable the NoContents feature for this document only.

NoContents only works with HTML type of files (IndexContents) in swish 2.2.


=item filter_content

This callback subroutine can filter the contents before it gets
passed to swish for indexing.  For example, you could add meta tags to HTML
documents or convert PDF or Word documents into text, HTML, or XML.

Again, your routine must return true to continue processing this resource.

The HTTP::Response object is passed as the third parameter, and a reference to the
content is passed as the forth parameter.

For example:

    use pdf2xml;  # included example pdf converter module
    $server{filter_content} = sub {
       my ( $uri, $server, $response, $content_ref ) = @_;

       return 1 if $response->content_type eq 'text/html';
       return 0 unless $response->content_type eq 'application/pdf';

       # for logging counts
       $server->{counts}{'PDF transformed'}++;

       $$content_ref = ${pdf2xml( $content_ref )};
       $$content_ref =~ tr/ / /s;  # squash extra space
       return 1;
    }

=back

=head1 SEE ALSO

L<URI> L<LWP::RobotUA> L<WWW::RobotRules> L<Digest::MD5>

=head1 COPYRIGHT

Copyright 2001 Bill Moseley

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SUPPORT

Send all questions to the The SWISH-E discussion list.

See http://sunsite.berkeley.edu/SWISH-E.

=cut   

