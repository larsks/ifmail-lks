#!/usr/bin/perl  
#
# checker.pl	Consistency checker for the filebase
#
#   checker.pl,v 1.1 1994/03/19 16:34:43 cg Exp
#
#   Filebase - filebase management for Unix systems
#   Copyright (C) 1994	Cees de Groot
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#   
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#   You can contact the author
#   by SnailMail:	Cees de Groot
#			Ortsstrasse 1a
#			88718 Daisendorf
#			Germany
#   by Phone:            +49 7532 5131
#   by InternetMail:     de_groot@decus.decus.de
#   on his Box:          +49 7532 2532 (14k4/V42bis)
#
#

#
# This perl script checks wheither the .files.bbs files are consistent
# with what's on the disk.
#

#
# Find out where the .pli's should live.
#
$MyDir = $0;
$MyDir =~ s|(.*)/.*$|$1|;
require "$MyDir/config.pli";

&ReadConfig;

#
# Loop through all areas
#
dbmopen(Areas,"$ConfigDir/$AreaFileName", 0644);
while (($Key, $Value) = each %Areas) {
	($Junk, $AreaDir, $Junk) = split(/:/, $Value);

	#
	# Read the .files.bbs. Build a list of the contents, and 
	# check wheither the files are there.
	#
	undef %FileList;
	open(ListFile, "$AreaDir/$FilesBBSFileName");
	while (<ListFile>) {
		($FKey, $FVal) = split(/:/, $_);
		$FileList{$FKey} = $FVal;
		if (! -e "$AreaDir/$FKey") {
			print "File not on disk: $AreaDir/$FKey\n";
		}
	}
	close(ListFile);

	#
	# Check 2: Walk the directory and see if all files in the directory
	# are in the file list
	#
	opendir(TheDir, $AreaDir);
	while (($CheckFile = readdir(TheDir))) {
		if ($CheckFile =~ /^\./) {
			next;
		}
		if (-d "$AreaDir/$CheckFile") {
			next;
		}
		$CheckFile =~ s|/.*/(.*)$|$1|;

		if (!defined($FileList{$CheckFile})) {
			print "File not in list: $AreaDir/$CheckFile\n";
		}
	}
	closedir(TheDir);
}
close(Areas);
#
# checker.pl,v
# Revision 1.1  1994/03/19  16:34:43  cg
# Filebase first version that functions more or less. More a 'backup'
# commit than a real release...
#
# 
 
