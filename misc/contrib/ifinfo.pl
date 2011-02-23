#!/usr/bin/perl
$Ver = "0.5";
#
# this is a simple perl script to extract informations from
# the nodelist about a given sysop name.
# i don't know about the format of the nodelist, so perhaps it
# will not work 100% right :)
#
# Usage: ifinfo <sysop_name>
#    if you give the full name quote it or replace the space
#    through a '_'
#
# rasca, berlin 1994,97 (Rasca Gmelch, 2:2410/305.4)
# changed by Andreas Jellinghaus, 2:246/8112.16

# *** EDIT the next LINE to the right path!!! ***
$listpath = '/var/spool/fnet/Nodelist/[A-Za-z]*.[0-9][0-9][0-9]
			/var/spool/fnet/Nodelist/[a-z]*.bln';

# default domain
$domain = "fidonet.org";

if ((@ARGV != 1) || $ARGV[0] eq "-?" || $ARGV[0] eq "-h") {
	&usage;
}
$input = $ARGV[0];

# substitute spaces in the name argument through "_"
$input =~ s/ +/_/g;

open (FIND, "find $listpath|") || die "error finding the nodelists";
while ($nodelist = <FIND>) {
	&get_info;
}
close (FIND);

if (!$found) {
	print ("$0: \"$input\" not found\n");
	exit (1);
}

# process for every nodelist
sub get_info {
	$zone = "???";
	$hub = "";
	open (NLIST, "<$nodelist")  || die "can't open nodelist '$nodelist'";
	while ($line=<NLIST>) {
		#
		# skip lines with comments
		#
		if ($line =~ /^[^;]/) {
			#
			# split the line..
			($key, $num, $sys, $locate, $name, $phone, $speed, $flags)
					= split(/,/, $line, 8);
			#
			# and have a look at the first field..
			#
			if ($key eq "Zone") {
				$zone = $num;
				$net = 0;
				$num = 0;
				$hub = "";

			} elsif ($key eq "Host") {
				$net = $num;
				$num = 0;
				$hub = "";

			} elsif ($key eq "Hub") {
				$hub = "f$num.n$net.z$zone.$domain ($sys)";
				$num = 0;

			} elsif ($name =~ /$input/) {
				# i don't know about the meaning of all keywords in the nodelist,
				# so print the not known keys behind the system name :-)
				if ($key ne "")
					{ $sys .= " ($key)"; }
				$name =~ s/_/\./g;
				print ("System:    $sys\n");
				print ("Sysop:     $name\@f$num.n$net.z$zone.$domain\n");
				print ("Location:  $locate\n");
				print ("PhoneNo.:  $phone\n");
				print ("Speed:     $speed\n");
				print ("NL-Flags:  $flags");
				print ("HubSystem: $hub\n\n");
				$found++;
			}
		}
	}
	close (NLIST);
}


# usage()
sub usage {
	print ("$0 Version $Ver\n");
	print ("Usage: $0 <sysop_name>\n");
	exit (2);
}

