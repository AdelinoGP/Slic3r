#!/usr/bin/perl

package Slic3r::Electronics::Electronics;
use strict;
use warnings;
use utf8;

use File::Basename;
use Slic3r::Electronics::Filereaders::Eagle;
use Slic3r::Electronics::Filereaders::3DElectronics;

#######################################################################
# Purpose    : Reads a given file
# Parameters : A filename
# Returns    : Schematic of Electronics
# Commet     : Delegates the reading of the file to the Filereaders
#######################################################################
sub readFile {
    my $self = shift;
    my ($filename, $config) = @_;
    my ($base,$path,$type) = fileparse($filename,('.sch','.SCH','3de','.3DE'));
    my $schematic;
    if ($type eq "sch" || $type eq "SCH" || $type eq ".sch" || $type eq ".SCH") {
        $schematic = Slic3r::Electronics::Filereaders::Eagle->readFile($filename, $config);
        return $schematic;
    } elsif ($type eq "3de" || $type eq "3DE" || $type eq ".3de" || $type eq ".3DE") {
        $schematic = Slic3r::Electronics::Filereaders::3DElectronics->readFile($filename, $config);
        return $schematic;
    }
    
    return ;
}

#######################################################################
# Purpose    : Writes a file
# Parameters : see Slic3r::Electronics::Filereaders::3DElectronics->writeFile
# Returns    : see Slic3r::Electronics::Filereaders::3DElectronics->writeFile
# Commet     :
#######################################################################
sub writeFile {
    my $self = shift;
    my ($schematic) = @_;
    return Slic3r::Electronics::Filereaders::3DElectronics->writeFile($schematic);
}

1;