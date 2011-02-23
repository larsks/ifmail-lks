#!/usr/bin/perl

$aliasdb="/var/spool/ifmail/ifdbm";

unshift(@INC,"/usr/local/etc/httpd/bin");
require "cgi-lib.pl";

print &PrintHeader;
&ReadParse;

unless (dbmopen(DB,$aliasdb,0770)) {
	print "cannot open $aliasdb: $!\n";
	exit(1);
}

$action=$in{'action'};
$oldname=$in{'oldname'};
$newname=$in{'newname'};
$newaddr=$in{'newaddr'};

print	"<HTML><HEAD><TITLE>Edit Name-Address DB</TITLE></HEAD>\n",
	"<BODY BGCOLOR=#fffff0>\n",
	"<CENTER>\n",
	"<H1>Edit Name-Address DB</H1>\n";

if ($oldname) {
	$oldaddr=$DB{$oldname};
}

if (($action eq "Clone") || ($action eq "Replace")) {
	$newaddr=$DB{$newname};
	if ($newname eq $oldname) {
		print "New name matches old, nothing to change<BR>\n";
	} elsif ($newaddr) {
		print "New name already exists, remove it first<BR>\n";
	} elsif ($oldaddr eq "") {
		print "Old entry does not exist, nothing to change<BR>\n";
	} else {
		$DB{$newname}=$oldaddr;
		if ($action eq "Replace") {
			delete $DB{$oldname};
			print "Entry for \"$oldname\" removed<BR>\n";
		}
		print "New entry for \"$newname\" has address \"$oldaddr\"<BR>\n";
		$newaddr=$oldaddr;
	}
} elsif ($action eq "Update") {
	if ($newname ne $oldname) {
		print "New name does not match old, will not change address<BR>\n";
	} elsif ($newaddr eq $oldaddr) {
		print "New address is the same as old, nothing to change<BR>\n";
	} elsif ($oldaddr eq "") {
		print "Old entry does not exist, nothing to change<BR>\n";
	} else {
		$DB{$newname}=$newaddr;
		print "New entry for \"$newname\" now has address \"$newaddr\"<BR>\n";
	}
} elsif ($action eq "Remove") {
	if ($newname) {
		delete $DB{$newname};
		print "Entry for \"$newname\" deleted<BR>\n";
		$newname="";
		$newaddr="";
	}
} else {
	$newaddr=$DB{$newname};
}

dbmclose(DB);

print	"<FORM METHOD=get>\n";

print	"<TABLE BORDER=1>\n";
print	"<INPUT TYPE=hidden NAME=oldname VALUE=\"$newname\">\n";
print	"<TR><TD>\n";
print	"Name </TD><TD><INPUT TYPE=text SIZE=40 NAME=newname VALUE=\"$newname\">\n";
print	"<INPUT TYPE=submit NAME=action VALUE=Search><BR>\n";
print	"</TD></TR><TR><TD>\n";
print	"Address </TD><TD><INPUT TYPE=text SIZE=50 NAME=newaddr VALUE=\"$newaddr\"><BR>\n";
print	"</TD></TR><TR><TD>&nbsp;</TD><TD>\n";
print	"<INPUT TYPE=submit NAME=action VALUE=Clone>\n";
print	"<INPUT TYPE=submit NAME=action VALUE=Replace>\n";
print	"<INPUT TYPE=submit NAME=action VALUE=Update>\n";
print	"<INPUT TYPE=submit NAME=action VALUE=Remove><BR>\n";
print	"</TD></TR></TABLE>\n";

print	"</FORM>\n",
	"</CENTER>\n",
	"<HR>\n",
	"<A HREF=/>www.fido7.ru</A>\n",
	"</BODY></HTML>\n";
