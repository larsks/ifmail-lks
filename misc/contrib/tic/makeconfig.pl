#!/usr/bin/perl
#
# makeconfig.pl			Generate configuration database
#
#   makeconfig.pl,v 1.1 1994/03/19 16:34:48 cg Exp
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

$MyDir = $0;
$MyDir =~ s|(.*)/.*$|$1|;
require "$MyDir/config.pli";

#
# This module contains a number of routines, one per type of configuration
# file, which convert the configuration files into dbm-databases under the
# same name. This so we can use them more easily from a number of modules.
#
&ReadConfig;

&ReadNodes("$ConfigDir/$NodeFileName");
&ReadAreas("$ConfigDir/$AreaFileName");
&ReadProcessors("$ConfigDir/$ProcessorFileName");
&ReadTriggers("$ConfigDir/$TriggerFileName");
&ReadPostings("$ConfigDir/$PostingFileName");

#
# Now, it gets a little bit harder: we make a dbm-database which
# connects Nodes to Areas.
#
dbmopen(%Areas, "$ConfigDir/$AreaFileName", 0644);
@AreaList = keys(%Areas);
dbmclose(%Areas);
if ( -f "$ConfigDir/$AreaNodesFileName.pag" ) {
	unlink("$ConfigDir/$AreaNodesFileName.pag");
	unlink("$ConfigDir/$AreaNodesFileName.dir");
}
dbmopen(%AreaNodes, "$ConfigDir/$AreaNodesFileName", 0644);
for ($Counter = 0; $Counter <= $#AreaList; $Counter++) {
	$CurArea = $AreaList[$Counter];
	print "Looking for nodes connected to $CurArea\n";
	dbmopen(%Nodes, "$ConfigDir/$NodeFileName", 0644);
	while (($CurNode,$NodeData) = each %Nodes) {
		if ($NodeData =~ $CurArea) {
			print "$CurNode ";
			$AreaNodeList .= "$CurNode,";
		}
	}
	print "\n";
	dbmclose(%Nodes);
	$AreaNodeList =~ s/,$//;
	$AreaNodes{$CurArea} = $AreaNodeList;
	$AreaNodeList = "";
}
dbmclose(%Areas);
dbmclose(%AreaNodes);
	
sub ReadAreas {
	local($AreaFile) = @_;
	local($Junk, $Tag, $Groups, $Dir, $FOwn, $FGroup, $FMode, $Posting);
	local($CurArea); # For debugging only

	#
	# Read the Area file, ignore comments and empty lines.
	# Between the Area-lines, there are description lines
	#
	open (AreaFile);
	while (<AreaFile>) {
		/^#/		&& next;
		/^[ \t]*$/	&& next;
		chop;
		if (/^Area/) {
			#
			# We got an Area-statement. Add it to the list
			#
			($Junk, $Tag, $Groups, $Dir, $FOwn, $FGroup, $FMode, $Posting) =
				split(/[ \t]+/, $_);
			$AreaList[$#AreaList + 1] = $Tag;
			$AreaGroupsList{$Tag} = $Groups;
			$AreaDirList{$Tag} = $Dir;
			$AreaFOwnList{$Tag} = $FOwn;
			$AreaFGroupList{$Tag} = $FGroup;
			$AreaFModeList{$Tag} = $FMode;
			$AreaPostingList{$Tag} = $Posting;
		}
		else {
			#
			# Non-empty line - assume it is the Area Comment
			#
			s/[ \t]*//;
			$AreaDescList{$Tag} = $_;
		}
	}
	close (AreaFile);

	#
	# Convert the whole stuff into a dbm-database
	#
	if ( -f "${AreaFile}.pag" ) {
		unlink($AreaFile . ".pag");
		unlink($AreaFile . ".dir");
	}
	dbmopen(%Areas, $AreaFile, 0644);
	for $CurArea (@AreaList)
	{
		#
		# Convert the user/group names to numbers
		#
		$Uid = getpwnam($AreaFOwnList{$CurArea});
		$Gid = getgrnam($AreaFGroupList{$CurArea});

		$Areas{$CurArea} = join(':',
			$AreaGroupsList{$CurArea},
			$AreaDirList{$CurArea},
			$Uid, $Gid,
			$AreaFModeList{$CurArea},
			$AreaPostingList{$CurArea},
			$AreaDescList{$CurArea}
		);
	}
	dbmclose(%Areas);
}

sub ReadNodes {
	local($NodeFile) = @_;
	local($Junk, $Node, $Password, $Groups, $Processor);
	local($CurNode); # For debugging only

	#
	# Read the Nodes file, ignore comments and empty lines.
	# Between the Nodes-lines, there are comma-separated lists of
	# areas they're attached to
	#
	open (NodeFile);
	while (<NodeFile>) {
		/^#/		&& next;
		/^[ \t]*$/	&& next;
		chop;
		if (/^Node/) {
			#
			# We got a node-statement. Add the node to the list
			#
			($Junk, $Node, $Password, $Groups, $Processor) =
				split(/[ \t]+/, $_);
			$NodeList[$#NodeList + 1] = $Node;
			$NodePwList{$Node} = $Password;
			$NodeGroupList{$Node} = $Groups;
			$NodeProcessorList{$Node} = $Processor;
		}
		else {
			#
			# Non-empty line - assume it is a list of Areas.
			#
			s/[ \t]*//g;
			$NodeAreaList{$Node} .= $_ . ',';
		}
	}
	close (NodeFile);

	#
	# Convert the whole stuff into a dbm-database
	#
	if ( -f "${NodeFile}.pag" ) {
		unlink($NodeFile . ".pag");
		unlink($NodeFile . ".dir");
	}
	dbmopen(%Nodes, $NodeFile, 0644);
	for $CurNode (@NodeList)
	{
		$NodeAreaList{$CurNode} =~ s/,$//;

		$Nodes{$CurNode} = join(':',
			$NodePwList{$CurNode}, $NodeGroupList{$CurNode},
			$NodeProcessorList{$CurNode}, $NodeAreaList{$CurNode}
		);
	}
	dbmclose(%Nodes);
}

sub ReadTriggers {
	local($TriggerFile) = @_;
	local($FileName, $Area, $Trigger);

	#
	# Read the Triggers file, ignore comments and empty lines.
	# As these have a simple one-line format, we can do them
	# in one pass.
	#
	if ( -f "${TriggerFile}.pag" ) {
		unlink($TriggerFile . ".pag");
		unlink($TriggerFile . ".dir");
	}
	dbmopen(%Triggers, $TriggerFile, 0644);
	open (TriggerFile);
	while (<TriggerFile>) {
		/^#/		&& next;
		/^[ \t]*$/	&& next;
		chop;
		($FileName, $Area, $Trigger) = split(/[ \t]+/, $_, 3);
		if ($Area eq "X") {
			$Area = "";
		}
		$Triggers{$FileName} = join(':', $Area, $Trigger);
	}
	close (TriggerFile);
	dbmclose(%Triggers);
}

sub ReadProcessors {
	local($ProcessorFile) = @_;
	local($Tag, $Method, $Data);
	local($CurProcessor); # For debugging only

	#
	# Read the Processors file, ignore comments and empty lines.
	# Write them into the dbm database.
	#
	if ( -f "${ProcessorFile}.pag" ) {
		unlink($ProcessorFile . ".pag");
		unlink($ProcessorFile . ".dir");
	}
	dbmopen(Processors, $ProcessorFile, 0644);
	open (ProcessorFile);
	while (<ProcessorFile>) {
		/^#/		&& next;
		/^[ \t]*$/	&& next;
		chop;
		($Tag, $Method, $Data) = split(/[ \t]+/, $_, 3);
		$Processors{$Tag} = join(':', $Method, $Data);
	}
	close (ProcessorFile);
	dbmclose(%Processors);
}

sub ReadPostings {
	local($PostingFile) = @_;
	local($Tag, $Frequency, $NewsGroup, $Template);
	local($CurPosting); # For debugging only

	#
	# Read the Postings file, ignore comments and empty lines.
	# Write them into the dbm database.
	#
	if ( -f "${PostingFile}.pag" ) {
		unlink($PostingFile . ".pag");
		unlink($PostingFile . ".dir");
	}
	dbmopen(%Postings, $PostingFile, 0644);
	open (PostingFile);
	while (<PostingFile>) {
		/^#/		&& next;
		/^[ \t]*$/	&& next;
		chop;
		($Tag, $Frequency, $NewsGroup, $Template) = split(/[ \t]+/, $_, 4);
		$Postings{$Tag} = join(':', $Frequency, $NewsGroup, $Template);
	}
	close (PostingFile);
	dbmclose(%Postings);
}

#
# makeconfig.pl,v
# Revision 1.1  1994/03/19  16:34:48  cg
# Filebase first version that functions more or less. More a 'backup'
# commit than a real release...
#
#
