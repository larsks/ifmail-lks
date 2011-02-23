#!/usr/bin/perl
#   
# lister.pl		Master file generator.
#
#   lister.pl,v 1.2 1994/04/21 18:50:57 cg Exp
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
# This module contains a perl script which processes all areas for
# a certain group. From it, it creates a master listing in the base
# dir.
#

#
# Find out where the .pli's should live.
#
$MyDir = $0;
$MyDir =~ s|(.*)/.*$|$1|;
require "$MyDir/config.pli";
require "$MyDir/getdate.pli";

&ReadConfig;

#
# A form for the FILES.BBS lines
#
format GroupListFile =
@<<<<<<<<<<<< @>>>>>> @>>>>>>>@^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$CurFile,     $CurSize, $CDate, $NewFlag, $CurDesc
~~                             ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                               $CurDesc
.

#
# First, sort the areas according to their group tags
#
dbmopen(Areas, "$ConfigDir/$AreaFileName", 0644);
while (($Key,$Value) = each %Areas)
{
	($Group, $Junk) = split(/:/, $Value);
	$GroupList{$Group} .= "$Key:";
	print "Added Area $Key in group $Group\n";
}

#
# Now, for every group, create the master listing
#
while (($CurGroup, $CurList) = each %GroupList) {
	print "Processing $CurGroup ($CurList)\n";

	#
	# Open the file for the group
	#
	open(GroupListFile, ">$FileBase/group_$CurGroup.lst");
	$OldFD = select(GroupListFile);

	#
	# Get the global header and write it to the listing
	#
	open(GlobHeader, "$ConfigDir/global.hdr");
	while (<GlobHeader>) {
		print;
	}
	close(GlobHeader);

	#
	# Get the group header and write it to the listing
	#
	open(GroupHeader, "$ConfigDir/group_$CurGroup.hdr");
	while (<GroupHeader>) {
		print;
	}
	close(GroupHeader);

	#
	# Walk through the areas, and process them. We could have done
	# this with templates, but W.T.H.
	#
	$TotalCount = 0; $TotalSize = 0;
	AreaLoop:
	while (1) {
		($CurArea, $CurList) = split(/:/, $CurList, 2);
		if ($CurArea eq '') {
			last AreaLoop
		}
		print STDOUT "Processing $CurArea (left: $CurList)\n";
		($AreaGroup, $AreaDir, $AreaFOwn, $AreaFGroup, $AreaFMode,
		 $AreaPost, $AreaDesc) = split(/:/, $Areas{$CurArea});

		#
		# Print a header for the area
		#
		print "\n";
		print "=" x 78;
		print "\n";
		print "Area: $CurArea, $AreaDesc\n";
		print "      ($AreaDir)\n";
		print "=" x 78;
		print "\n";

		#
		# Open the files.bbs, and write out every line
		#
		open(FilesBBS, "$AreaDir/$FilesBBSFileName");
		while (<FilesBBS>) {
			($CurFile, $CurDesc) = split(/:/, $_);
			($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $CurSize,
			$atime, $mtime, $ctime, $blksize, $blocks) =
			stat("$AreaDir/$CurFile");
			($sec,$min,$hour,$mday,$mon,$year,$wday,$isdst) = gmtime($mtime);
			$CDate = sprintf("%02d-%02d-%02d", $mday, $mon, $year);
			if (time() - $mtime < (14 * 24 * 3600)) {
				$NewFlag = "*";
			} else {
				$NewFlag = " ";
			}

			write;

			$TotalCount++;
			$TotalSize += $CurSize;
		}
		close(FilesBBS);
	}
	print "=" x 78;
	print "\n";
	print "Grand Total of $TotalCount files, $TotalSize bytes\n";
	close(GroupListFile);
	select($OldFD);
}
dbmclose(Areas);

#
# lister.pl,v
# Revision 1.2  1994/04/21  18:50:57  cg
# This one works for me - I activated all `unsafe' statements, and it
# processes LinuxNet; that's the primary goal, of course ;-)
#
# Revision 1.1  1994/03/19  16:34:47  cg
# Filebase first version that functions more or less. More a 'backup'
# commit than a real release...
#
# 
