#!/usr/bin/perl
#   
# poster.pl			Process outstanding postings
#
#   poster.pl,v 1.2 1994/04/21 18:50:58 cg Exp
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
# This module contains a perl script which processes all outstanding
# postings. It should be called with a posting frequency as $ARGV[0].
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
# Go through all the lines in the file
#
dbmopen(PostData, "$ConfigDir/$PostingFileName", 0644);
dbmopen(ToPost, "$FileBase/$ToPostFileName", 0644);
while (($Key,$Val) = each %ToPost)
{
	#
	# Split it up and check the type
	#
	($FileArea, $FileName, $PostType) = split(/:/, $Val);
	($Freq, $Board, $Form) = split(/:/, $PostData{$PostType});
	if ($Freq eq '') {
		print "No such posting type $PostType!\n";
		next;
	}

	#
	# If the frequency matches, store the data
	#
	print "Checking $Freq/$Board/$Form\n";
	if ($Freq eq $ARGV[0]) {
		print "Will do $FileArea/$FileName\n";
		$PostingList[$#PostingList + 1] = 
				"$PostType:$FileArea:$FileName:$Key";
	}
}


#
# Now sort the array, and go through it.
#
dbmopen(Areas, "$ConfigDir/$AreaFileName", 0644);
sort @PostingList;
$LastType = '';
for ($Counter = 0; $Counter <= $#PostingList; $Counter++) {
	($CurType, $CurArea, $CurFile, $CurCrc) = split(/:/, 
		$PostingList[$Counter]);
	if ($CurType ne $LastType) {
		#
		# We changed types. Send the article, setup the next
		#
		if ($LastType ne "") {
			close(Article);
			select($OldFD);
		}
		($Freq, $Board, $Form) = split(/:/, $PostData{$PostType});
		open(Article,"|$NewsInput");
		$OldFD = select(Article);
		$LastType = $CurType;

	}

	#
	# Get the Area Data
	#
	($AreaGroup, $AreaDir, $AreaFOwn, $AreaFGroup, $AreaFMode, $AreaPost, 
		$AreaDesc) = split(/:/, $Areas{$CurArea});

	#
	# Get the File Data
	#
	open(FilesBBS, "$AreaDir/$FilesBBSFileName");
	while (<FilesBBS>) {
		if (/^$CurFile/) {
			($Junk, $CurDesc) = split(/:/, $_, 2);
			last;
		}
	}
	close(FileBBS);

	($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$CurSize,
	  $atime,$mtime,$ctime,$blksize,$blocks) =
	  stat("$AreaDir/$CurFile");

	#
	# Get the form. Barf.
	#
	require "$ConfigDir/$Form.frm";
	$~ = $Form;
	$^ = "${Form}_TOP";

	&GetDate;

	$AtKludge = "@";

	#
	# Write all kinds of stuff, and delete it.
	#
	print STDOUT "Filedescription = $CurDesc\n";
	print STDOUT "Forms are: $~ / $^ \n\n";
	write;
	delete $ToPost{$CurCrc};
}
close(Article);
select($OldFD);

dbmclose(PostData);
dbmclose(ToPost);

# poster.pl,v
# Revision 1.2  1994/04/21  18:50:58  cg
# This one works for me - I activated all `unsafe' statements, and it
# processes LinuxNet; that's the primary goal, of course ;-)
#
# Revision 1.1  1994/03/19  16:34:49  cg
# Filebase first version that functions more or less. More a 'backup'
# commit than a real release...
#
#
