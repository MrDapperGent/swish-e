package SWISH::Filters::Doc2txt;
use vars qw/ @ISA $VERSION /;

$VERSION = '0.01';
@ISA = ('SWISH::Filter');
sub new {
    my ( $pack, %params ) = @_;

    my $self = bless {
        name => $params{name} || $pack,
    }, $pack;


    # check for helpers
    for my $prog ( qw/ catdoc / ) {
        my $path = $self->find_binary( $prog );
        unless ( $path ) {
            $self->mywarn("Can not use Filter $pack -- need to install $prog");
            return;
        }   
        $self->{$prog} = $path;
    }

    return $self;

}

sub name { $_->{name} || 'unknown' };



sub filter {
    my ( $self, $filter) = @_;

    # Do we care about this document?
    return unless $filter->content_type =~ m!application/msword!;

    # We need a file name to pass to the catdoc program
    my $file = $filter->fetch_filename;
    
    # Grab output from running program
    my $content = $filter->run_program( $self->{catdoc}, $file );

    return unless $content;

    # update the document's content type
    $filter->set_content_type( 'text/plain' );

    # return the document
    return \$content;
}
1;

__END__

=head1 NAME

SWISH::Filters::Doc2txt - Perl extension for filtering MSWord documents with Swish-e

=head1 DESCRIPTION

This is a plug-in module that uses the "catdoc" program to convert MS Word documents
to text for indexing by Swish-e.  "catdoc" can be downloaded from:

    http://www.ice.ru/~vitus/catdoc/ver-0.9.html

The program "catdoc" must be installed and your PATH before running Swish-e.

=head1 BUGS

This filter does not specify input or output character encodings.  This will change in the
future to all use of the user_data to set the encoding.

A minor optimization during spidering (i.e. when docs are in memory instead of on disk)
would be to use open2() call to let catdoc read from stdin instead of from a file.

=head1 AUTHOR

Bill Moseley

=head1 SEE ALSO

L<SWISH::Filter>


