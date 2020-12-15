#!/usr/bin/env perl

use JSON::PP;
use File::Basename;

use strict;
use warnings;

die "./parse-json.pl <run> <sanitizer> <compile-database> <dir> <file>" if @ARGV < 4;

my ($run, $san, $json_file, $dir_match, $file_match) = (shift,shift,shift,shift,shift);

print STDERR "ARGV=@ARGV\n";
print STDERR "json=$json_file\n";
print STDERR "dir match=$dir_match\n";
print STDERR "file match=$file_match\n";

sub readFile {
    my $json = shift;
    open my $fd, "<", $json;
    my $lines = do { local $/; <$fd> };
    close $fd;
    return $lines;
}

sub decodeFile {
    my $data = shift;
    my $json = decode_json $data;
    return $json;
}

my $raw = &readFile($json_file);
my $json = &decodeFile($raw);
my $dirname = dirname(__FILE__);

map {
    my ($dir, $file) = ($_->{'directory'}, $_->{'file'});
    my $cmd = "$dirname/transform.sh $san $json_file $file @ARGV";
    if ($dir =~ /$dir_match/ && $file =~ /$file_match/) {
        print "$cmd\n";
        print `$cmd` if ($run == 1);
    }
} @{$json};
