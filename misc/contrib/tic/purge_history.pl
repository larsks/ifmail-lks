#!/usr/bin/perl
#   
# purge_history.pl		Purge Dupe-history
#
#   purge_history.pl,v 1.1 1994/03/19 16:34:51 cg Exp
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
# This module contains a perl script which purges entries over 90 days
# old in the history.
#

#
# Find out where the .pli's should live.
#
$MyDir = $0;
$MyDir =~ s|(.*)/.*$|$1|;
require "$MyDir/config.pli";

&ReadConfig;

#
# Calculate what to purge. Times are in seconds in the histfile
#
$Purged = 0;
print "Starting purge on history file\n";
$PurgeBefore = time() - ($KeepHistory * 24 * 60 * 60);
dbmopen(History, "$FileBase/$HistFileName", 0644);
while (($Key, $Val) = each %History)
{
	($Junk1, $Junk2, $HistTime) = split(/:/, $Val);
	if ($HistTime < $PurgeBefore) {
		delete $History{$Key};
		$Purged++;
	}
}
print "Done. Purged $Purged entries\n";
#
# purge_history.pl,v
# Revision 1.1  1994/03/19  16:34:51  cg
# Filebase first version that functions more or less. More a 'backup'
# commit than a real release...
#
#
