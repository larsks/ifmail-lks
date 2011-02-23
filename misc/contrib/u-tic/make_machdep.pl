#!/usr/bin/perl

($ARGV[0] && $ARGV[1]) || exit(2);

print qq|\$CONFIG="$ARGV[0]";\n|;
print qq|\$VERSION="$ARGV[1]";\n|;

open(F,"./makeflock|") || exit(2);
while (<F>)
{
	print STDERR "Bad makeflock\n", exit(2) unless /^([lisc])\s+(\S+)/;
	$fmt.=$1;
	push(@arglist,$2); 
}
$arglist=join(",",@arglist);

print <<EOF;
eval 'sub f_lock {return fcntl(\$_[0],\$_[2]?&F_SETLK:&F_SETLKW,pack("$fmt",$arglist));}'; die \$@ if \$@;
1;
EOF

exit(0);
