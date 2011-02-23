#!/usr/bin/perl
#   
# process_tics.pl	- Process .TIC files
#
#   process_tics.pl,v 1.2 1994/04/21 18:51:00 cg Exp
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
# Find out where the .pli's should live.
#
$MyDir = $0;
$MyDir =~ s|(.*)/.*$|$1|;
require "$MyDir/config.pli";
require "$MyDir/readtick.pli";
require "$MyDir/sequencer.pli";

&ReadConfig;

print "ConfigDir = $ConfigDir\n";
print "FileBase  = $FileBase\n";
print "Inbound   = $Inbound\n";
chdir($Inbound);

TickFile:
for $CurTic (<*.tic>) {
	print "Reading $CurTic\n";
	$Status = &ReadTickFile($CurTic);
	print "status = $Status\n";
	if (!$Status) {
		print "Error was : $ReadTickError\n";
		next TickFile;
	}

	#
	# Dupe checking. 
	#
	dbmopen(Dupes, "$FileBase/$HistFileName", 0644);
	($DupeArea, $DupeFile, $Junk) = split(/:/, $Dupes{$FileContents{'Crc'}});
	if (($DupeArea eq $FileContents{'Area'}) && 
	    ($DupeFile eq $FileContents{'File'})) {
		print "Duplicate file, ignored\n";
		next TickFile;
	}
	$Dupes{$FileContents{'Crc'}} = join(':', 
		$FileContents{'Area'}, $FileContents{'File'}, time());
	dbmclose(Dupes);

	#
	# Tick is OK. Fine, move it to the destination directory
	#
	system "mv $FileContents{'File'} $AreaDir";
	$ProcessFile = "$AreaDir/$FileContents{'File'}";
	$TarOwn = ($AreaFOwn eq "") ? $> : $AreaFOwn;
	$TarGroup = ($AreaFGroup eq "") ? $) : $AreaFGroup;
	chown $TarOwn, $TarGroup, $ProcessFile;
	print "chown $TarOwn, $TarGroup, $ProcessFile\n";
	$TarMode = ($AreaFMode eq "") ? 0644 : $AreaFMode;
	chmod oct($TarMode), $ProcessFile;
	print "chmod $TarMode, $ProcessFile\n";

	#
	# Update the .files.bbs. .Files.bbs has a colon-separated format,
	# possibly it could be dbm later on.
	#
	open(DescFile, ">>$AreaDir/$FilesBBSFileName");
	print DescFile "$FileContents{'File'}:@FileDesc\n";
	close(DescFile);

	#
	# Look if some posting applies. When yes, write out the file
	# data so it may be reported later on. We write it into a dbm
	# database, so partial postings easily can remove the work done.
	# key is Area/Crc (to be sure it is a little bit unique).
	#
	if ($AreaPosting ne "None") {
		dbmopen(PostList, "$FileBase/$ToPostFileName", 0644);
		$PostList{"$FileContents{'Crc'}"} =
			join(':', $FileContents{'Area'}, 
				$FileContents{'File'}, $AreaPosting);
		dbmclose(PostList);
	}

	#
	# Look if some trigger applies
	#
	dbmopen(Triggers, "$ConfigDir/$TriggerFileName", 0644);
	while (($CurTrig, $CurTrigData) = each %Triggers) {
		if ($FileContents{'File'} =~ $CurTrig) {
			print "Found match on trigger $CurTrig\n";
			($TrigArea, $TrigCommand) = split(/:/, $CurTrigData);
			if (($TrigArea eq "") || ($TrigArea eq $FileContents{'Area'})) {
				print ".. and Area matches/is empty\n";
				$CurTrigData =~ s/\$1/$ProcessFile/;
				#
				# Strange. If Area is empty, the leading ':' isn't dropped.
				# we can filter it anyway, a command can start with it.
				#
				$CurTrigData =~ s/^://;
				print "executing: $CurTrigData\n";
				system $CurTrigData;
			}
		}
	}

	#
	# Now, build a list of nodes to copy it to. 
	# First get all possible nodes.
	#
	dbmopen(AreaNodes, "$ConfigDir/$AreaNodesFileName", 0644);
	$OutgoingNodes = $AreaNodes{$FileContents{'Area'}};
	@OutgoingNodes = split(/,/, $OutgoingNodes);
	print "Sending to nodes: \"@OutgoingNodes\"\n";

	#
	# Now, check the Seen-by list from the Ticfile
	#
	for $CurSeenby (@FileSeenby) {
		for $CurCheck (@OutgoingNodes) {
			if ($CurCheck eq $CurSeenby) {
				print "Node $CurCheck already seen, purging\n";
				$CurCheck = '';
			}
		}
	}
	print "Sending to @OutgoingNodes\n";

	#
	# Append them to the Seen-by list
	#
	for $CurSeenby (@OutgoingNodes) {
		if ($CurSeenby gt '') {
			@FileSeenby[$#FileSeenby + 1] = $CurSeenby;
		}
	}
	
	#
	# Append myself to the PATH-List
	#
	$Now = time();
	open(Date, "date|");
	$Date = <Date>;
	chop ($Date);
	close(Date);
	$FilePath[$#FilePath + 1] = "$MainAka $Now $Date";

	#
	# Fine. For every node, create a tic-file and send the stuff.
	#
	for $CurNode (@OutgoingNodes) {
		if ($CurNode gt '') {
			print "Sending to $CurNode\n";

			#
			# Get the name for a ticfile
			#
			&Sequencer;

			#
			# And write the thing. Yes, it is a DOS file :-(
			#
			print "Writing to $Outbound/$SequenceFileName\n";
			open(TicFile, ">$Outbound/$SequenceFileName");
			$OldFile = select(TicFile);
			print "Area $FileContents{'Area'}\r\n";
			print "Origin $FileContents{'Origin'}\r\n";
			print "From $MainAka\r\n";
			print "To Sysop, $CurNode\r\n";
			print "File $FileContents{'File'}\r\n";
			for $CurDesc (@FileDesc) {
				print "Desc $CurDesc\r\n";
			}
			print "Crc $FileContents{'Crc'}\r\n";
			print "Created $FileContents{'Created'}\r\n";
			for $CurPath (@FilePath) {
				print "Path $CurPath\r\n";
			}
			for $CurSeenby (@FileSeenby) {
				print "Seenby $CurSeenby\r\n";
			}
			dbmopen(Nodes, "$ConfigDir/$NodeFileName", 0644);
			($CurNodePW, $Junk) = split(/:/, $Nodes{$CurNode});
			dbmclose(Nodes);
			print "Pw $CurNodePW\r\n";
			close(TicFile);
			select($OldFile);

			#
			# Write the .flo file
			#
			# XXX Of course, this and above stuff should be split for
			# different Processors...
			#
			($Zone, $Net, $Node, $Point) = split(/[:\/]/, $CurNode);
			print "Sending to $Zone $Net $Node $Point\n";

			if ($Point ne "") {
				$PointDir = sprintf("%s/%04x%04x.pnt", 
					$Outbound, $Net, $Node);
				if (! -d $PointDir) {
					mkdir($PointDir, 0750);
				}
				$FloFile = sprintf("%s/0000%04x.flo",
					$PointDir, $Point);
			}
			else {
				$FloFile = sprintf("%s/%04x%04x.flo",
					$Outbound, $Net, $Node);
			}
			print "Flo is $FloFile\n";

			#
			# See FSC-0028: The File itself *MUST* come first, then
			# the .Tic!
			#
			open(FloFile, ">>$FloFile");
			$OldFile = select(FloFile);
			print "$AreaDir/$FileContents{'File'}\n";
			print "^$Outbound/$SequenceFileName\n";
			close(FloFile);
			select($OldFile);
		}
	}
	unlink($CurTic);
}

#
# process_tics.pl,v
# Revision 1.2  1994/04/21  18:51:00  cg
# This one works for me - I activated all `unsafe' statements, and it
# processes LinuxNet; that's the primary goal, of course ;-)
#
# Revision 1.1  1994/03/19  16:34:50  cg
# Filebase first version that functions more or less. More a 'backup'
# commit than a real release...
#
#
